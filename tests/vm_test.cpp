#include "lexer/lexer.h"
#include "parser/parser.h"
#include "vm/compiler.h"
#include "vm/vm.h"
#include "utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>
#include <memory>
#include <sstream>

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
