#include "io/io_interface.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sandbox/execution_limiter.h"
#include "vm/compiler.h"
#include "vm/vm.h"
#include "utf8_converter/utf8_converter.h"
#include "exception/exception.h"
#include <gtest/gtest.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace std;

class VMTest : public ::testing::Test {
protected:
    shared_ptr<Object> runVM(const string& source) {
        // 줄 단위 처리 (REPL처럼)
        Lexer lexer;
        vector<shared_ptr<Token>> allTokens;
        istringstream stream(source);
        string line;
        int indent = 0;

        while (getline(stream, line)) {
            line += "\n";
            auto utf8 = Utf8Converter::Convert(line);
            auto tokens = lexer.Tokenize(utf8);
            allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());
            for (auto& t : tokens) {
                if (t->type == TokenType::START_BLOCK) indent++;
                if (t->type == TokenType::END_BLOCK) indent--;
            }
        }
        for (int i = 0; i < indent; i++) {
            allTokens.push_back(make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
        }

        Parser parser;
        auto program = parser.Parsing(allTokens);

        Compiler compiler;
        auto bytecode = compiler.Compile(program);

        VM vm;
        return vm.Execute(bytecode);
    }

    // 출력 캡처용
    string runVMWithOutput(const string& source) {
        ostringstream fakeStdout;
        auto* coutbuf = cout.rdbuf();
        cout.rdbuf(fakeStdout.rdbuf());

        runVM(source);

        cout.rdbuf(coutbuf);
        return fakeStdout.str();
    }
};

// ===== 기본 산술 =====

TEST_F(VMTest, IntegerArithmetic) {
    auto result = runVM("1 + 2 * 3\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 7);
}

// 거듭제곱: long long 범위 안의 결과는 정수로 유지
TEST_F(VMTest, PowIntegerInRange) {
    auto result = runVM("2 ** 62\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 4611686018427387904LL);
}

// 거듭제곱: long long 오버플로 시 double로 폴백 (UB 방지)
TEST_F(VMTest, PowIntegerOverflowFallsBackToFloat) {
    auto result = runVM("2 ** 63\n");
    auto* f = dynamic_cast<Float*>(result.get());
    ASSERT_NE(f, nullptr) << "overflow should fall back to Float, not silently wrap";
    EXPECT_NEAR(f->value, 9.223372036854776e18, 1e10);
}

TEST_F(VMTest, PowIntegerLargeOverflow) {
    auto result = runVM("10 ** 30\n");
    auto* f = dynamic_cast<Float*>(result.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 1e30, 1e22);
}

TEST_F(VMTest, FloatArithmetic) {
    auto result = runVM("3.14 + 1.0\n");
    auto* f = dynamic_cast<Float*>(result.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 4.14, 0.001);
}

TEST_F(VMTest, BooleanLogic) {
    auto result = runVM("true && false\n");
    auto* b = dynamic_cast<Boolean*>(result.get());
    ASSERT_NE(b, nullptr);
    EXPECT_FALSE(b->value);
}

TEST_F(VMTest, Comparison) {
    auto result = runVM("10 > 5\n");
    auto* b = dynamic_cast<Boolean*>(result.get());
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->value);
}

TEST_F(VMTest, StringConcat) {
    auto result = runVM("\"hello\" + \" world\"\n");
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "hello world");
}

TEST_F(VMTest, NegatePrefix) {
    auto result = runVM("-42\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, -42);
}

TEST_F(VMTest, BangPrefix) {
    auto result = runVM("!true\n");
    auto* b = dynamic_cast<Boolean*>(result.get());
    ASSERT_NE(b, nullptr);
    EXPECT_FALSE(b->value);
}

// ===== 변수 =====

TEST_F(VMTest, GlobalVariable) {
    auto result = runVM("정수 x = 42\nx\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 42);
}

TEST_F(VMTest, GlobalAssignment) {
    auto result = runVM("정수 x = 10\nx = 20\nx\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 20);
}

TEST_F(VMTest, CompoundAssignment) {
    auto result = runVM("정수 x = 10\nx += 5\nx\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 15);
}

// ===== 내장 함수 =====

TEST_F(VMTest, PrintBuiltin) {
    string output = runVMWithOutput("출력(\"hello\")\n");
    EXPECT_NE(output.find("hello"), string::npos);
}

TEST_F(VMTest, LengthBuiltin) {
    auto result = runVM("길이(\"hello\")\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 5);
}

// ===== 제어문 =====

TEST_F(VMTest, IfStatement) {
    string output = runVMWithOutput(
        "정수 x = 10\n"
        "만약 x > 5 라면:\n"
        "    출력(\"크다\")\n"
        "\n"
    );
    EXPECT_NE(output.find("크다"), string::npos);
}

TEST_F(VMTest, IfElseStatement) {
    string output = runVMWithOutput(
        "정수 x = 3\n"
        "만약 x > 5 라면:\n"
        "    출력(\"크다\")\n"
        "아니면:\n"
        "    출력(\"작다\")\n"
        "\n"
    );
    EXPECT_NE(output.find("작다"), string::npos);
}

TEST_F(VMTest, WhileLoop) {
    auto result = runVM(
        "정수 합계 = 0\n"
        "정수 i = 1\n"
        "반복 i <= 10 동안:\n"
        "    합계 += i\n"
        "    i += 1\n"
        "\n"
        "합계\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 55);
}

// ===== 함수 =====

TEST_F(VMTest, FunctionCall) {
    auto result = runVM(
        "함수 더하기(정수 a, 정수 b) -> 정수:\n"
        "    리턴 a + b\n"
        "\n"
        "더하기(3, 7)\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 10);
}

// ===== 자료구조 =====

TEST_F(VMTest, ArrayLiteral) {
    auto result = runVM("[1, 2, 3]\n");
    auto* arr = dynamic_cast<Array*>(result.get());
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(arr->elements.size(), 3u);
}

TEST_F(VMTest, ArrayIndex) {
    auto result = runVM("[10, 20, 30][1]\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 20);
}

TEST_F(VMTest, TupleIndex) {
    auto result = runVM("(10, 20, 30)[1]\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 20);
}

TEST_F(VMTest, ArraySlice) {
    auto result = runVM("[10, 20, 30, 40, 50][1:3]\n");
    auto* arr = dynamic_cast<Array*>(result.get());
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->elements.size(), 2u);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[0].get())->value, 20);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[1].get())->value, 30);
}

TEST_F(VMTest, ArraySliceFromStart) {
    auto result = runVM("[10, 20, 30][:2]\n");
    auto* arr = dynamic_cast<Array*>(result.get());
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->elements.size(), 2u);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[0].get())->value, 10);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[1].get())->value, 20);
}

TEST_F(VMTest, ArraySliceToEnd) {
    auto result = runVM("[10, 20, 30][1:]\n");
    auto* arr = dynamic_cast<Array*>(result.get());
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->elements.size(), 2u);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[0].get())->value, 20);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[1].get())->value, 30);
}

TEST_F(VMTest, ArraySliceNegativeEnd) {
    auto result = runVM("[10, 20, 30, 40][:-1]\n");
    auto* arr = dynamic_cast<Array*>(result.get());
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->elements.size(), 3u);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[0].get())->value, 10);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[2].get())->value, 30);
}

TEST_F(VMTest, StringSlice) {
    auto result = runVM("\"hello\"[1:3]\n");
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "el");
}

TEST_F(VMTest, StringSliceNegativeEnd) {
    auto result = runVM("\"hello\"[:-1]\n");
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "hell");
}

TEST_F(VMTest, NullValue) {
    auto result = runVM("없음\n");
    ASSERT_NE(dynamic_cast<Null*>(result.get()), nullptr);
}

TEST_F(VMTest, BitwiseOps) {
    auto result = runVM("5 & 3\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 1);
}

TEST_F(VMTest, BitwiseOrOps) {
    auto result = runVM("5 | 3\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 7);
}

TEST_F(VMTest, StringInterpolation) {
    auto result = runVM(
        "문자 이름 = \"홍길동\"\n"
        "\"안녕 {이름}님\"\n"
    );
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "안녕 홍길동님");
}

// ===== Match =====

TEST_F(VMTest, MatchStatement) {
    string output = runVMWithOutput(
        "정수 x = 2\n"
        "비교 x:\n"
        "    경우 1:\n"
        "        출력(\"일\")\n"
        "    경우 2:\n"
        "        출력(\"이\")\n"
        "    기본:\n"
        "        출력(\"기타\")\n"
        "\n"
    );
    EXPECT_NE(output.find("이"), string::npos);
}

TEST_F(VMTest, MatchRangePattern) {
    string output = runVMWithOutput(
        "정수 x = 3\n"
        "비교 x:\n"
        "    경우 1~5:\n"
        "        출력(\"범위안\")\n"
        "    기본:\n"
        "        출력(\"범위밖\")\n"
        "\n"
    );
    EXPECT_NE(output.find("범위안"), string::npos);
}

TEST_F(VMTest, MatchTypePattern) {
    string output = runVMWithOutput(
        "정수 x = 42\n"
        "비교 x:\n"
        "    경우 문자:\n"
        "        출력(\"문자열\")\n"
        "    경우 정수:\n"
        "        출력(\"정수값\")\n"
        "    기본:\n"
        "        출력(\"기타\")\n"
        "\n"
    );
    EXPECT_NE(output.find("정수값"), string::npos);
}

TEST_F(VMTest, MatchGuardCondition) {
    string output = runVMWithOutput(
        "정수 x = 7\n"
        "비교 x:\n"
        "    경우 7 만약 x > 5:\n"
        "        출력(\"큰7\")\n"
        "    경우 7:\n"
        "        출력(\"작은7\")\n"
        "    기본:\n"
        "        출력(\"기타\")\n"
        "\n"
    );
    EXPECT_NE(output.find("큰7"), string::npos);
}

// ===== Try-Catch =====

TEST_F(VMTest, TryCatch) {
    string output = runVMWithOutput(
        "시도:\n"
        "    정수 x = 10 / 0\n"
        "실패 오류:\n"
        "    출력(오류)\n"
        "\n"
    );
    EXPECT_NE(output.find("0으로 나눌 수 없습니다"), string::npos);
}

// ===== 클래스 =====

TEST_F(VMTest, ClassNoConstructor) {
    auto result = runVM(
        "클래스 점:\n"
        "    정수 x\n"
        "\n"
        "점 p = 점()\n"
        "p.x\n"
    );
    // 필드 기본값은 없음(null)
    ASSERT_NE(dynamic_cast<Null*>(result.get()), nullptr);
}

TEST_F(VMTest, ClassWithConstructor) {
    auto result = runVM(
        "클래스 점:\n"
        "    정수 x\n"
        "    생성(정수 값):\n"
        "        자기.x = 값\n"
        "\n"
        "점 p = 점(42)\n"
        "p.x\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 42);
}

TEST_F(VMTest, ClassFieldAccess) {
    string output = runVMWithOutput(
        "클래스 동물:\n"
        "    문자 이름\n"
        "    생성(문자 이름):\n"
        "        자기.이름 = 이름\n"
        "\n"
        "동물 강아지 = 동물(\"뽀삐\")\n"
        "출력(강아지.이름)\n"
    );
    EXPECT_NE(output.find("뽀삐"), string::npos);
}

TEST_F(VMTest, ClassMethodCall) {
    string output = runVMWithOutput(
        "클래스 동물:\n"
        "    문자 이름\n"
        "    생성(문자 이름):\n"
        "        자기.이름 = 이름\n"
        "    함수 소개() -> 문자:\n"
        "        리턴 자기.이름\n"
        "\n"
        "동물 강아지 = 동물(\"뽀삐\")\n"
        "출력(강아지.소개())\n"
    );
    EXPECT_NE(output.find("뽀삐"), string::npos);
}

// ===== 클로저 =====

TEST_F(VMTest, ClosureCapture) {
    auto result = runVM(
        "정수 x = 10\n"
        "함수 읽기() -> 정수:\n"
        "    리턴 x\n"
        "\n"
        "읽기()\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 10);
}

// ===== 상수 폴딩 =====

TEST_F(VMTest, ConstantFoldIntegerAdd) {
    auto result = runVM("1 + 2\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 3);
}

TEST_F(VMTest, ConstantFoldIntegerSub) {
    auto result = runVM("10 - 3\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 7);
}

TEST_F(VMTest, ConstantFoldIntegerMul) {
    auto result = runVM("4 * 5\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 20);
}

TEST_F(VMTest, ConstantFoldIntegerDiv) {
    auto result = runVM("20 / 4\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 5);
}

TEST_F(VMTest, ConstantFoldFloatAdd) {
    auto result = runVM("1.5 + 2.5\n");
    auto* f = dynamic_cast<Float*>(result.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 4.0, 0.001);
}

TEST_F(VMTest, ConstantFoldFloatMul) {
    auto result = runVM("3.0 * 2.0\n");
    auto* f = dynamic_cast<Float*>(result.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 6.0, 0.001);
}

TEST_F(VMTest, ConstantFoldStringConcat) {
    auto result = runVM("\"hello\" + \" world\"\n");
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "hello world");
}

TEST_F(VMTest, ConstantFoldDivByZeroNotFolded) {
    // 0으로 나누기는 폴딩하지 않고 런타임에 처리
    string output = runVMWithOutput(
        "시도:\n"
        "    정수 x = 10 / 0\n"
        "실패 오류:\n"
        "    출력(오류)\n"
        "\n"
    );
    EXPECT_NE(output.find("0으로 나눌 수 없습니다"), string::npos);
}

TEST_F(VMTest, ConstantFoldBytecodeSize) {
    // 상수 폴딩 시 바이트코드가 더 작아야 함
    // 1 + 2는 OP_CONSTANT 1개로 컴파일되어야 함 (OP_CONSTANT + OP_RETURN)
    Lexer lexer;
    vector<shared_ptr<Token>> allTokens;
    string source = "1 + 2\n";
    auto utf8 = Utf8Converter::Convert(source);
    auto tokens = lexer.Tokenize(utf8);
    allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());

    Parser parser;
    auto program = parser.Parsing(allTokens);

    Compiler compiler;
    auto bytecode = compiler.Compile(program);

    // 폴딩된 경우: OP_CONSTANT(3바이트) + OP_RETURN(1바이트) = 4바이트
    // 폴딩 안 된 경우: OP_CONSTANT(3) + OP_CONSTANT(3) + OP_ADD(1) + OP_RETURN(1) = 8바이트
    EXPECT_EQ(bytecode->code.size(), 4u);
    EXPECT_EQ(static_cast<OpCode>(bytecode->code[0]), OpCode::OP_CONSTANT);
    EXPECT_EQ(static_cast<OpCode>(bytecode->code[3]), OpCode::OP_RETURN);

    // 상수 풀에 결과값 3만 있어야 함
    ASSERT_EQ(bytecode->constants.size(), 1u);
    auto* val = dynamic_cast<Integer*>(bytecode->constants[0].get());
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(val->value, 3);
}

TEST_F(VMTest, ClassMethod) {
    auto result = runVM(
        "클래스 계산기:\n"
        "    정수 값\n"
        "    생성(정수 초기값):\n"
        "        자기.값 = 초기값\n"
        "    함수 더하기(정수 수) -> 정수:\n"
        "        리턴 자기.값 + 수\n"
        "\n"
        "계산기 계 = 계산기(10)\n"
        "계.더하기(5)\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 15);
}

TEST_F(VMTest, StringIterationUTF8) {
    auto output = runVMWithOutput(
        "각각 문자 c \"안녕\" 에서:\n"
        "    출력(c)\n"
    );
    EXPECT_EQ(output, "안\n녕\n");
}

TEST_F(VMTest, StringIndexUTF8) {
    auto result = runVM("\"안녕하세요\"[1]\n");
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "녕");
}

TEST_F(VMTest, StringNegativeIndexUTF8) {
    auto result = runVM("\"안녕하세요\"[-1]\n");
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "요");
}

// ===== 기본 매개변수 =====

TEST_F(VMTest, DefaultParameter) {
    auto result = runVM(
        "함수 인사(문자 이름, 문자 접미사 = \"님\") -> 문자:\n"
        "    리턴 이름 + 접미사\n"
        "인사(\"홍길동\")\n"
    );
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "홍길동님");
}

TEST_F(VMTest, DefaultParameterOverride) {
    auto result = runVM(
        "함수 인사(문자 이름, 문자 접미사 = \"님\") -> 문자:\n"
        "    리턴 이름 + 접미사\n"
        "인사(\"홍길동\", \"씨\")\n"
    );
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "홍길동씨");
}

TEST_F(VMTest, DefaultParameterAllDefaults) {
    auto result = runVM(
        "함수 더하기(정수 a = 1, 정수 b = 2) -> 정수:\n"
        "    리턴 a + b\n"
        "더하기()\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 3);
}

// ===== 인덱스 대입 =====

TEST_F(VMTest, IndexAssignmentArray) {
    auto result = runVM(
        "배열 목록 = [1, 2, 3]\n"
        "목록[0] = 10\n"
        "목록[0]\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 10);
}

TEST_F(VMTest, IndexAssignmentNegative) {
    auto result = runVM(
        "배열 목록 = [1, 2, 3]\n"
        "목록[-1] = 30\n"
        "목록[2]\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 30);
}

TEST_F(VMTest, IndexAssignmentHashMap) {
    auto result = runVM(
        "사전 정보 = {\"이름\": \"홍길동\"}\n"
        "정보[\"나이\"] = \"25\"\n"
        "정보[\"나이\"]\n"
    );
    auto* s = dynamic_cast<String*>(result.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "25");
}

TEST_F(VMTest, ImportBasic) {
    std::string tmpFile = "test_import_vm.hik";
    {
        std::ofstream out(tmpFile);
        out << "함수 제곱(정수 x) -> 정수:\n    리턴 x * x\n";
        out.close();
    }

    auto result = runVM(
        "가져오기 \"" + tmpFile + "\"\n"
        "제곱(5)\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 25);

    std::remove(tmpFile.c_str());
}

TEST_F(VMTest, ImportDoubleNoError) {
    std::string tmpFile = "test_import_double_vm.hik";
    {
        std::ofstream out(tmpFile);
        out << "함수 하나() -> 정수:\n    리턴 1\n";
        out.close();
    }

    auto result = runVM(
        "가져오기 \"" + tmpFile + "\"\n"
        "가져오기 \"" + tmpFile + "\"\n"
        "하나()\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 1);

    std::remove(tmpFile.c_str());
}

// 보안: 가져오기 경로 검증 (FileRead/FileWrite와 동일한 규칙)
TEST_F(VMTest, ImportRejectsParentPath) {
    EXPECT_THROW({
        runVM("가져오기 \"../secret.hik\"\n");
    }, RuntimeException);
}

TEST_F(VMTest, ImportRejectsAbsoluteUnixPath) {
    EXPECT_THROW({
        runVM("가져오기 \"/etc/passwd\"\n");
    }, RuntimeException);
}

TEST_F(VMTest, ImportRejectsAbsoluteBackslashPath) {
    EXPECT_THROW({
        runVM("가져오기 \"\\\\windows\\\\path\"\n");
    }, RuntimeException);
}

TEST_F(VMTest, ImportRejectsWindowsDriveLetter) {
    EXPECT_THROW({
        runVM("가져오기 \"C:/Windows/System32/cmd.exe\"\n");
    }, RuntimeException);
}

// ===== 제너레이터/생산 =====

TEST_F(VMTest, GeneratorBasic) {
    auto output = runVMWithOutput(
        "함수 세개() -> 정수:\n"
        "    생산 1\n"
        "    생산 2\n"
        "    생산 3\n"
        "각각 정수 값 세개() 에서:\n"
        "    출력(값)\n"
    );
    EXPECT_EQ(output, "1\n2\n3\n");
}

TEST_F(VMTest, GeneratorWithLoop) {
    auto output = runVMWithOutput(
        "함수 범위(정수 n) -> 정수:\n"
        "    반복 정수 i = 0 부터 n 까지:\n"
        "        생산 i\n"
        "각각 정수 x 범위(3) 에서:\n"
        "    출력(x)\n"
    );
    EXPECT_EQ(output, "0\n1\n2\n");
}

TEST_F(VMTest, GeneratorEmpty) {
    auto output = runVMWithOutput(
        "함수 빈것() -> 정수:\n"
        "    만약 false 라면:\n"
        "        생산 1\n"
        "각각 정수 x 빈것() 에서:\n"
        "    출력(x)\n"
    );
    EXPECT_EQ(output, "");
}

TEST_F(VMTest, IntegrationDefaultsAndIndexAssignment) {
    auto output = runVMWithOutput(
        "함수 초기화(정수 크기 = 3) -> 배열:\n"
        "    배열 결과 = [0, 0, 0]\n"
        "    반복 정수 i = 0 부터 크기 까지:\n"
        "        결과[i] = i * 2\n"
        "    리턴 결과\n"
        "배열 목록 = 초기화()\n"
        "각각 정수 값 목록 에서:\n"
        "    출력(값)\n"
    );
    EXPECT_EQ(output, "0\n2\n4\n");
}

TEST_F(VMTest, IntegrationGeneratorWithDefaults) {
    auto output = runVMWithOutput(
        "함수 짝수(정수 최대 = 6) -> 정수:\n"
        "    반복 정수 i = 0 부터 최대 까지:\n"
        "        만약 i % 2 == 0 라면:\n"
        "            생산 i\n"
        "각각 정수 n 짝수() 에서:\n"
        "    출력(n)\n"
    );
    EXPECT_EQ(output, "0\n2\n4\n");
}

TEST_F(VMTest, ConstantFoldingComparison) {
    auto result = runVM("2 > 1\n");
    auto* b = dynamic_cast<Boolean*>(result.get());
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->value);
}

TEST_F(VMTest, ConstantFoldingModulo) {
    auto result = runVM("10 % 3\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 1);
}

TEST_F(VMTest, UnaryConstantFolding) {
    auto result = runVM("-42\n");
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, -42);
}

TEST_F(VMTest, DeadCodeAfterReturn) {
    auto result = runVM(
        "함수 테스트() -> 정수:\n"
        "    리턴 42\n"
        "    리턴 99\n"
        "테스트()\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 42);
}

TEST_F(VMTest, OptimizationVerification) {
    auto result = runVM(
        "정수 합 = 0\n"
        "반복 정수 i = 0 부터 100 까지:\n"
        "    합 = 합 + i\n"
        "합\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 4950);
}

// ===== 런타임 타입 체크 =====

TEST_F(VMTest, FunctionParamTypeError) {
    EXPECT_THROW({
        runVM(
            "함수 더하기(정수 x) -> 정수:\n"
            "    리턴 x + 1\n"
            "\n"
            "더하기(\"문자열\")\n"
        );
    }, RuntimeException);
}

TEST_F(VMTest, FunctionReturnTypeError) {
    EXPECT_THROW({
        runVM(
            "함수 변환(정수 x) -> 문자:\n"
            "    리턴 x + 1\n"
            "\n"
            "변환(10)\n"
        );
    }, RuntimeException);
}

TEST_F(VMTest, FunctionCorrectTypes) {
    auto result = runVM(
        "함수 합계(정수 x, 정수 y) -> 정수:\n"
        "    리턴 x + y\n"
        "\n"
        "합계(3, 4)\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 7);
}

TEST_F(VMTest, BinaryOpBetterErrorMessage) {
    try {
        runVM("1 + \"문자\"\n");
        FAIL() << "Expected RuntimeException";
    } catch (const RuntimeException& e) {
        string msg = e.what();
        EXPECT_TRUE(msg.find("+") != string::npos) << "Error message should contain operator: " << msg;
        EXPECT_TRUE(msg.find("정수") != string::npos) << "Error message should contain left type: " << msg;
        EXPECT_TRUE(msg.find("문자") != string::npos) << "Error message should contain right type: " << msg;
    }
}

TEST_F(VMTest, ErrorMessageIncludesIndexInfo) {
    try {
        runVM("배열 a = [1, 2, 3]\na[10]\n");
        FAIL() << "Expected RuntimeException";
    } catch (const RuntimeException& e) {
        string msg = e.what();
        EXPECT_TRUE(msg.find("10") != string::npos) << "Error message should contain index: " << msg;
        EXPECT_TRUE(msg.find("3") != string::npos) << "Error message should contain array size: " << msg;
    }
}

TEST_F(VMTest, ErrorMessageIncludesUncallableType) {
    try {
        runVM("정수 x = 42\nx()\n");
        FAIL() << "Expected RuntimeException";
    } catch (const RuntimeException& e) {
        string msg = e.what();
        EXPECT_TRUE(msg.find("정수") != string::npos || msg.find("호출") != string::npos)
            << "Error message should contain type or call info: " << msg;
    }
}

// ===== ExecutionLimiter wiring (S1 of VM unification) =====
// 헬퍼: limiter를 받는 VM 실행 경로. runVM과 동일하게 lex→parse→compile→execute한다.
namespace {
std::shared_ptr<Object> runVMWithLimiter(const std::string& source, ExecutionLimiter* limiter) {
    Lexer lexer;
    std::vector<std::shared_ptr<Token>> allTokens;
    std::istringstream stream(source);
    std::string line;
    int indent = 0;
    while (std::getline(stream, line)) {
        line += "\n";
        auto utf8 = Utf8Converter::Convert(line);
        auto tokens = lexer.Tokenize(utf8);
        allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());
        for (auto& t : tokens) {
            if (t->type == TokenType::START_BLOCK) indent++;
            if (t->type == TokenType::END_BLOCK) indent--;
        }
    }
    for (int i = 0; i < indent; i++) {
        allTokens.push_back(std::make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
    }
    Parser parser;
    auto program = parser.Parsing(allTokens);
    Compiler compiler;
    auto bytecode = compiler.Compile(program);
    VM vm(/*ioCtx=*/nullptr, limiter);
    return vm.Execute(bytecode);
}
} // namespace

TEST_F(VMTest, LimiterAbsentIsNoOp) {
    // limiter 미전달 시(=nullptr) 기존 동작 그대로. 큰 루프도 그냥 돈다.
    auto result = runVM(
        "정수 sum = 0\n"
        "정수 i = 1\n"
        "반복 i <= 1000 동안:\n"
        "    sum = sum + i\n"
        "    i = i + 1\n"
        "sum\n"
    );
    auto* integer = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(integer, nullptr);
    EXPECT_EQ(integer->value, 500500);
}

TEST_F(VMTest, WhileLoopHitsIterationCap) {
    // 무한 루프가 maxLoopIterations 초과 시점에 중단되는지 확인.
    // ExecutionLimiter는 std::runtime_error를 throw하지만, VM의 dispatch
    // try/catch가 이를 RuntimeException으로 wrap한다(vm.cpp:401-411).
    // 그 결과 최종적으로 RuntimeException으로 surface된다.
    ExecutionLimiter limiter(60000, /*maxLoopIterations=*/100);
    EXPECT_THROW({
        runVMWithLimiter(
            "정수 i = 0\n"
            "반복 true 동안:\n"
            "    i = i + 1\n",
            &limiter);
    }, RuntimeException);
    EXPECT_GE(limiter.getLoopIterationCount(), 100);
}

TEST_F(VMTest, ForEachHitsIterationCap) {
    // for-each는 OP_ITER_NEXT 경로를 거치므로 별도로 검증한다.
    // 10원소 배열을 한 번 돌리면 비-소진 iter 호출이 10번 발생한다.
    // cap=9로 두면 10번째 호출에서 throw.
    ExecutionLimiter limiter(60000, /*maxLoopIterations=*/9);
    EXPECT_THROW({
        runVMWithLimiter(
            "정수 sum = 0\n"
            "배열 a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]\n"
            "각각 정수 x a 에서:\n"
            "    sum = sum + x\n",
            &limiter);
    }, RuntimeException);
    EXPECT_GE(limiter.getLoopIterationCount(), 9);
}

// ===== IOContext wiring (S4 of VM unification) =====
// 이전에는 VM이 IOContext를 받지 않아 출력이 std::cout 직행이었고,
// WASM/test harness가 buffer로 캡쳐할 수 없었다. 이제 BuiltinRegistry가
// ioCtx를 Print/Input/FileRead/FileWrite에 전달하므로, 콜백을 설정하면
// 출력이 그쪽으로 라우팅돼야 한다.
TEST_F(VMTest, OutputRoutesThroughIOContext) {
    std::string captured;
    IOContext ctx;
    ctx.print = [&captured](const std::string& s) { captured += s; };

    Lexer lexer;
    auto utf8 = Utf8Converter::Convert(std::string("출력(\"안녕\")\n"));
    auto tokens = lexer.Tokenize(utf8);
    Parser parser;
    auto program = parser.Parsing(tokens);
    Compiler compiler;
    auto bytecode = compiler.Compile(program);
    VM vm(&ctx);
    vm.Execute(bytecode);

    // 출력 builtin이 endl을 emit하므로 trailing newline 포함.
    EXPECT_NE(captured.find("안녕"), std::string::npos)
        << "VM output should route through IOContext.print, but captured: '"
        << captured << "'";
}

TEST_F(VMTest, TimeoutZeroFiresImmediatelyInLoop) {
    // timeoutMs=0이면 첫 루프 cycle에서 RuntimeException으로 즉시 종료된다.
    // (incrementLoopCounter 이후 checkLimits에서 throw)
    ExecutionLimiter limiter(0, ExecutionLimiter::DEFAULT_MAX_LOOP_ITERATIONS);
    EXPECT_THROW({
        runVMWithLimiter(
            "정수 i = 0\n"
            "반복 true 동안:\n"
            "    i = i + 1\n",
            &limiter);
    }, RuntimeException);
}
