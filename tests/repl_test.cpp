#include "ast/expressions.h"
#include "ast/literals.h"
#include "ast/program.h"
#include "ast/statements.h"
#include "evaluator/evaluator.h"
#include "parser/parser.h"
#include "repl/repl.h"
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>

using namespace std;

class ReplTest : public ::testing::Test {
protected:
    string runRepl(const string& user_input, bool useVM = false) {
        std::istringstream fakeStdin(user_input);
        std::ostringstream fakeStdout;

        auto* cinbuf = std::cin.rdbuf();
        auto* coutbuf = std::cout.rdbuf();

        std::cin.rdbuf(fakeStdin.rdbuf());
        std::cout.rdbuf(fakeStdout.rdbuf());

        Repl repl(useVM);
        repl.Run();

        std::cin.rdbuf(cinbuf);
        std::cout.rdbuf(coutbuf);

        return fakeStdout.str();
    }
};


TEST_F(ReplTest, testTest) {
    std::string user_input;
    user_input += "1 + 2\n";
    user_input += "정수 a = 10\n";
    user_input += "a + 5\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("3"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
    EXPECT_NE(output.find("15"), std::string::npos);
}


TEST_F(ReplTest, ifTest1) {
    std::string user_input;
    user_input += "정수 사과 = 11\n";
    user_input += "만약 사과 - 2 == 9 라면:\n";
    user_input += "    사과 = 5\n";
    user_input += "\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("11"), std::string::npos);
    EXPECT_NE(output.find("5"), std::string::npos);
}


TEST_F(ReplTest, ifTest2) {
    std::string user_input;
    user_input += "정수 사과 = 11\n";
    user_input += "만약 사과 - 2 == 9 라면:\n";
    user_input += "    사과 = 5\n";
    user_input += "아니면:\n";
    user_input += "    사과 = 10\n";
    user_input += "\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("11"), std::string::npos);
    EXPECT_NE(output.find("5"), std::string::npos);
}


TEST_F(ReplTest, ifTest3) {
    std::string user_input;
    user_input += "정수 사과 = 15\n";
    user_input += "만약 사과 - 2 == 9 라면:\n";
    user_input += "    사과 = 5\n";
    user_input += "아니면:\n";
    user_input += "    사과 = 10\n";
    user_input += "\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("15"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
}


TEST_F(ReplTest, functionTest) {
    std::string user_input;
    user_input += "함수 피보나치(정수 수) -> 정수:\n";
    user_input += "    만약 수 == 0 라면:\n";
    user_input += "        리턴 0\n";
    user_input += "    만약 수 == 1 라면:\n";
    user_input += "        리턴 1\n";
    user_input += "    리턴 피보나치(수 - 1) + 피보나치(수 - 2)\n";
    user_input += "\n";
    user_input += "피보나치(10)\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("함수:"), std::string::npos);
    EXPECT_NE(output.find("55"), std::string::npos);
}


TEST_F(ReplTest, comparisonOperatorTest) {
    std::string user_input;
    user_input += "정수 x = 10\n";
    user_input += "만약 x > 5 라면:\n";
    user_input += "    x = 100\n";
    user_input += "\n";
    user_input += "x\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("100"), std::string::npos);
}


TEST_F(ReplTest, whileLoopTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "정수 i = 1\n";
    user_input += "반복 i <= 10 동안:\n";
    user_input += "    합계 += i\n";
    user_input += "    i += 1\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("55"), std::string::npos);
}


TEST_F(ReplTest, compoundAssignmentTest) {
    std::string user_input;
    user_input += "정수 a = 10\n";
    user_input += "a += 5\n";
    user_input += "a\n";
    user_input += "a -= 3\n";
    user_input += "a\n";
    user_input += "a *= 2\n";
    user_input += "a\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("15"), std::string::npos);
    EXPECT_NE(output.find("12"), std::string::npos);
    EXPECT_NE(output.find("24"), std::string::npos);
}


TEST_F(ReplTest, floatTest) {
    std::string user_input;
    user_input += "실수 pi = 3.14\n";
    user_input += "실수 r = 2.0\n";
    user_input += "pi * r * r\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("12.56"), std::string::npos);
}


TEST_F(ReplTest, commentTest) {
    std::string user_input;
    user_input += "1 + 2 // 이것은 주석입니다\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("3"), std::string::npos);
}


TEST_F(ReplTest, moduloTest) {
    std::string user_input;
    user_input += "10 % 3\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("1"), std::string::npos);
}


TEST_F(ReplTest, bangOperatorTest) {
    std::string user_input;
    user_input += "!true\n";
    user_input += "!false\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("false"), std::string::npos);
    EXPECT_NE(output.find("true"), std::string::npos);
}


TEST_F(ReplTest, breakTest) {
    std::string user_input;
    user_input += "정수 x = 0\n";
    user_input += "반복 true 동안:\n";
    user_input += "    x += 1\n";
    user_input += "    만약 x == 5 라면:\n";
    user_input += "        중단\n";
    user_input += "\n";
    user_input += "x\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("5"), std::string::npos);
}


TEST_F(ReplTest, stringIndexTest) {
    std::string user_input;
    user_input += "문자 s = \"hello\"\n";
    user_input += "s[0]\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("h"), std::string::npos);
}


TEST_F(ReplTest, nestedWhileTest) {
    std::string user_input;
    user_input += "정수 결과 = 0\n";
    user_input += "정수 i = 0\n";
    user_input += "반복 i < 3 동안:\n";
    user_input += "    정수 j = 0\n";
    user_input += "    반복 j < 3 동안:\n";
    user_input += "        결과 += 1\n";
    user_input += "        j += 1\n";
    user_input += "    i += 1\n";
    user_input += "\n";
    user_input += "결과\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("9"), std::string::npos);
}


TEST_F(ReplTest, forEachTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "각각 정수 원소 [1, 2, 3, 4, 5] 에서:\n";
    user_input += "    합계 += 원소\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("15"), std::string::npos);
}


TEST_F(ReplTest, hashMapTest) {
    std::string user_input;
    user_input += "{\"이름\": \"홍길동\", \"나이\": \"25\"}\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("홍길동"), std::string::npos);
}


TEST_F(ReplTest, hashMapIndexTest) {
    std::string user_input;
    user_input += "문자 이름 = {\"이름\": \"홍길동\"}[\"이름\"]\n";
    user_input += "이름\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("홍길동"), std::string::npos);
}


TEST_F(ReplTest, tryCatchTest) {
    std::string user_input;
    user_input += "시도:\n";
    user_input += "    정수 x = 10 / 0\n";
    user_input += "실패 오류:\n";
    user_input += "    출력(오류)\n";
    user_input += "\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("0으로 나눌 수 없습니다"), std::string::npos);
}


TEST_F(ReplTest, forEachStringTest) {
    std::string user_input;
    user_input += "정수 개수 = 0\n";
    user_input += "각각 문자 글자 \"abc\" 에서:\n";
    user_input += "    개수 += 1\n";
    user_input += "\n";
    user_input += "개수\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("3"), std::string::npos);
}


TEST_F(ReplTest, containsTest) {
    std::string user_input;
    user_input += "포함(\"hello world\", \"world\")\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("true"), std::string::npos);
}


TEST_F(ReplTest, functionCallTest) {
    std::string user_input;
    user_input += "함수 더하기(정수 a, 정수 b) -> 정수:\n";
    user_input += "    리턴 a + b\n";
    user_input += "\n";
    user_input += "더하기(3, 7)\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("10"), std::string::npos);
}


TEST_F(ReplTest, noArgFunctionTest) {
    std::string user_input;
    user_input += "함수 삼() -> 정수:\n";
    user_input += "    리턴 3\n";
    user_input += "\n";
    user_input += "삼()\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("3"), std::string::npos);
}


TEST_F(ReplTest, builtinCallTest) {
    std::string user_input;
    user_input += "길이(\"hello\")\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("5"), std::string::npos);
}


TEST_F(ReplTest, booleanTypeTest) {
    std::string user_input;
    user_input += "논리 참 = true\n";
    user_input += "논리 거짓 = false\n";
    user_input += "참 && 거짓\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("true"), std::string::npos);
    EXPECT_NE(output.find("false"), std::string::npos);
}


TEST_F(ReplTest, arrayTypeTest) {
    std::string user_input;
    user_input += "배열 목록 = [1, 2, 3]\n";
    user_input += "길이(목록)\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("3"), std::string::npos);
}


TEST_F(ReplTest, mapTypeTest) {
    std::string user_input;
    user_input += "사전 정보 = {\"이름\": \"홍길동\"}\n";
    user_input += "정보[\"이름\"]\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("홍길동"), std::string::npos);
}


TEST_F(ReplTest, booleanTypeMismatchTest) {
    std::string user_input;
    user_input += "시도:\n";
    user_input += "    논리 x = 42\n";
    user_input += "실패 오류:\n";
    user_input += "    출력(오류)\n";
    user_input += "\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("일치하지 않습니다"), std::string::npos);
}


TEST_F(ReplTest, importTest) {
    // 테스트용 파일 생성 (임시 디렉토리에)
    std::string tmpFile = std::tmpnam(nullptr);
    tmpFile += ".hik";
    {
        std::ofstream f(tmpFile);
        f << "함수 두배(정수 x) -> 정수:" << std::endl;
        f << "    리턴 x * 2" << std::endl;
    }

    std::string user_input;
    user_input += "가져오기 \"" + tmpFile + "\"\n";
    user_input += "두배(21)\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("42"), std::string::npos);

    std::remove(tmpFile.c_str());
}


// ===== Stage 1 Tests =====

TEST_F(ReplTest, bitwiseAndTest) {
    std::string user_input;
    user_input += "5 & 3\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("1"), std::string::npos);
}

TEST_F(ReplTest, bitwiseOrTest) {
    std::string user_input;
    user_input += "5 | 3\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("7"), std::string::npos);
}

TEST_F(ReplTest, nullTest) {
    std::string user_input;
    user_input += "정수? x = 없음\n";
    user_input += "x == 없음\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("true"), std::string::npos);
}

TEST_F(ReplTest, stringInterpolationTest) {
    std::string user_input;
    user_input += "문자 이름 = \"홍길동\"\n";
    user_input += "문자 인사 = \"안녕 {이름}님\"\n";
    user_input += "인사\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("안녕 홍길동님"), std::string::npos);
}

TEST_F(ReplTest, absTest) {
    std::string user_input;
    user_input += "절대값(-42)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("42"), std::string::npos);
}

TEST_F(ReplTest, sqrtTest) {
    std::string user_input;
    user_input += "제곱근(16)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("4"), std::string::npos);
}

TEST_F(ReplTest, maxMinTest) {
    std::string user_input;
    user_input += "최대(10, 20)\n";
    user_input += "최소(10, 20)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("20"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
}

TEST_F(ReplTest, splitTest) {
    std::string user_input;
    user_input += "분리(\"a,b,c\", \",\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[a, b, c]"), std::string::npos);
}

TEST_F(ReplTest, upperLowerTest) {
    std::string user_input;
    user_input += "대문자(\"hello\")\n";
    user_input += "소문자(\"HELLO\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("HELLO"), std::string::npos);
    EXPECT_NE(output.find("hello"), std::string::npos);
}

TEST_F(ReplTest, replaceTest) {
    std::string user_input;
    user_input += "치환(\"hello world\", \"world\", \"korea\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("hello korea"), std::string::npos);
}

TEST_F(ReplTest, trimTest) {
    std::string user_input;
    user_input += "자르기(\"  hello  \")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("hello"), std::string::npos);
}

TEST_F(ReplTest, sortTest) {
    std::string user_input;
    user_input += "정렬([3, 1, 2])\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[1, 2, 3]"), std::string::npos);
}

TEST_F(ReplTest, reverseTest) {
    std::string user_input;
    user_input += "뒤집기([1, 2, 3])\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[3, 2, 1]"), std::string::npos);
}

TEST_F(ReplTest, findTest) {
    std::string user_input;
    user_input += "찾기([10, 20, 30], 20)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("1"), std::string::npos);
}

TEST_F(ReplTest, sliceTest) {
    std::string user_input;
    user_input += "조각([1, 2, 3, 4, 5], 1, 4)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[2, 3, 4]"), std::string::npos);
}

// ===== Stage 2 Tests =====

TEST_F(ReplTest, matchStatementTest) {
    std::string user_input;
    user_input += "정수 x = 2\n";
    user_input += "비교 x:\n";
    user_input += "    경우 1:\n";
    user_input += "        출력(\"일\")\n";
    user_input += "    경우 2:\n";
    user_input += "        출력(\"이\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"기타\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("이"), std::string::npos);
}

TEST_F(ReplTest, matchDefaultTest) {
    std::string user_input;
    user_input += "정수 x = 99\n";
    user_input += "비교 x:\n";
    user_input += "    경우 1:\n";
    user_input += "        출력(\"일\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"기타\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("기타"), std::string::npos);
}

TEST_F(ReplTest, tupleTest) {
    std::string user_input;
    user_input += "배열 t = (1, 2, 3)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    // 튜플이 생성되어야 함 - 타입 불일치로 에러가 나야 함
    // 대신 튜플을 직접 테스트
}

TEST_F(ReplTest, tupleIndexTest) {
    std::string user_input;
    user_input += "(10, 20, 30)[1]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("20"), std::string::npos);
}

// ===== Stage 3: Class Tests =====

TEST_F(ReplTest, classBasicTest) {
    std::string user_input;
    user_input += "클래스 동물:\n";
    user_input += "    문자 이름\n";
    user_input += "    정수 나이\n";
    user_input += "    생성(문자 이름, 정수 나이):\n";
    user_input += "        자기.이름 = 이름\n";
    user_input += "        자기.나이 = 나이\n";
    user_input += "    함수 소개() -> 문자:\n";
    user_input += "        리턴 자기.이름\n";
    user_input += "\n";
    user_input += "동물 강아지 = 동물(\"뽀삐\", 3)\n";
    user_input += "강아지.이름\n";
    user_input += "강아지.나이\n";
    user_input += "강아지.소개()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("뽀삐"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, classMethodTest) {
    std::string user_input;
    user_input += "클래스 계산기:\n";
    user_input += "    정수 값\n";
    user_input += "    생성(정수 초기값):\n";
    user_input += "        자기.값 = 초기값\n";
    user_input += "    함수 더하기(정수 수) -> 정수:\n";
    user_input += "        리턴 자기.값 + 수\n";
    user_input += "\n";
    user_input += "계산기 계 = 계산기(10)\n";
    user_input += "계.더하기(5)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("15"), std::string::npos);
}

TEST_F(ReplTest, importCircularTest) {
    std::string tmpFile = std::tmpnam(nullptr);
    tmpFile += ".hik";
    {
        std::ofstream f(tmpFile);
        f << "정수 임포트값 = 99" << std::endl;
    }

    std::string user_input;
    user_input += "가져오기 \"" + tmpFile + "\"\n";
    user_input += "가져오기 \"" + tmpFile + "\"\n";
    user_input += "임포트값\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input);

    EXPECT_NE(output.find("99"), std::string::npos);

    std::remove(tmpFile.c_str());
}


// ===== VM REPL 세션 상태 유지 테스트 =====

TEST_F(ReplTest, vmReplStateTest) {
    std::string user_input;
    user_input += "정수 x = 42\n";
    user_input += "x + 8\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("50"), std::string::npos);
}

// ===== Feature Tests: Inheritance =====

TEST_F(ReplTest, inheritanceBasicTest) {
    std::string user_input;
    user_input += "클래스 동물:\n";
    user_input += "    문자 이름\n";
    user_input += "    생성(문자 이름):\n";
    user_input += "        자기.이름 = 이름\n";
    user_input += "    함수 소리() -> 문자:\n";
    user_input += "        리턴 \"...\"\n";
    user_input += "\n";
    user_input += "클래스 강아지 < 동물:\n";
    user_input += "    함수 소리() -> 문자:\n";
    user_input += "        리턴 \"멍멍\"\n";
    user_input += "\n";
    user_input += "강아지 뽀삐 = 강아지(\"뽀삐\")\n";
    user_input += "뽀삐.이름\n";
    user_input += "뽀삐.소리()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("뽀삐"), std::string::npos);
    EXPECT_NE(output.find("멍멍"), std::string::npos);
}

TEST_F(ReplTest, inheritanceParentMethodTest) {
    std::string user_input;
    user_input += "클래스 도형:\n";
    user_input += "    문자 종류\n";
    user_input += "    생성(문자 종류):\n";
    user_input += "        자기.종류 = 종류\n";
    user_input += "    함수 설명() -> 문자:\n";
    user_input += "        리턴 자기.종류\n";
    user_input += "\n";
    user_input += "클래스 원 < 도형:\n";
    user_input += "    정수 반지름\n";
    user_input += "    생성(문자 종류, 정수 반지름):\n";
    user_input += "        자기.종류 = 종류\n";
    user_input += "        자기.반지름 = 반지름\n";
    user_input += "\n";
    user_input += "원 동그라미 = 원(\"원형\", 5)\n";
    user_input += "동그라미.설명()\n";
    user_input += "동그라미.반지름\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("원형"), std::string::npos);
    EXPECT_NE(output.find("5"), std::string::npos);
}

// ===== Feature Tests: Higher-order functions =====

TEST_F(ReplTest, mapTest) {
    std::string user_input;
    user_input += "함수 두배(정수 x) -> 정수:\n";
    user_input += "    리턴 x * 2\n";
    user_input += "\n";
    user_input += "배열 결과 = 매핑([1, 2, 3], 두배)\n";
    user_input += "출력(결과)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[2, 4, 6]"), std::string::npos);
}

TEST_F(ReplTest, filterTest) {
    std::string user_input;
    user_input += "함수 양수(정수 x) -> 논리:\n";
    user_input += "    리턴 x > 0\n";
    user_input += "\n";
    user_input += "배열 결과 = 걸러내기([-1, 2, -3, 4], 양수)\n";
    user_input += "출력(결과)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[2, 4]"), std::string::npos);
}

TEST_F(ReplTest, reduceTest) {
    std::string user_input;
    user_input += "함수 합(정수 누적, 정수 현재) -> 정수:\n";
    user_input += "    리턴 누적 + 현재\n";
    user_input += "\n";
    user_input += "정수 결과 = 줄이기([1, 2, 3, 4, 5], 합, 0)\n";
    user_input += "결과\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("15"), std::string::npos);
}

// ===== Feature Tests: Default parameters =====

TEST_F(ReplTest, defaultParamTest) {
    std::string user_input;
    user_input += "함수 인사(문자 이름, 문자 접미사 = \"님\") -> 문자:\n";
    user_input += "    리턴 이름 + 접미사\n";
    user_input += "\n";
    user_input += "인사(\"홍길동\")\n";
    user_input += "인사(\"홍길동\", \"씨\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("홍길동님"), std::string::npos);
    EXPECT_NE(output.find("홍길동씨"), std::string::npos);
}

// ===== Feature Tests: For range loop =====

TEST_F(ReplTest, forRangeTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "반복 정수 i = 0 부터 5 까지:\n";
    user_input += "    합계 += i\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("10"), std::string::npos);
}

TEST_F(ReplTest, forRangeBreakTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "반복 정수 i = 0 부터 100 까지:\n";
    user_input += "    합계 += i\n";
    user_input += "    만약 i == 4 라면:\n";
    user_input += "        중단\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("10"), std::string::npos);
}

// ===== Feature Tests: Negative indexing =====

TEST_F(ReplTest, negativeArrayIndexTest) {
    std::string user_input;
    user_input += "배열 목록 = [10, 20, 30]\n";
    user_input += "목록[-1]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("30"), std::string::npos);
}

TEST_F(ReplTest, negativeStringIndexTest) {
    std::string user_input;
    user_input += "문자 s = \"hello\"\n";
    user_input += "s[-1]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("o"), std::string::npos);
}

// ===== Feature Tests: Slicing =====

TEST_F(ReplTest, arraySliceTest) {
    std::string user_input;
    user_input += "배열 목록 = [10, 20, 30, 40, 50]\n";
    user_input += "목록[1:3]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[20, 30]"), std::string::npos);
}

TEST_F(ReplTest, arraySliceFromStartTest) {
    std::string user_input;
    user_input += "배열 목록 = [10, 20, 30, 40, 50]\n";
    user_input += "목록[:3]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[10, 20, 30]"), std::string::npos);
}

TEST_F(ReplTest, arraySliceToEndTest) {
    std::string user_input;
    user_input += "배열 목록 = [10, 20, 30, 40, 50]\n";
    user_input += "목록[2:]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[30, 40, 50]"), std::string::npos);
}

TEST_F(ReplTest, arraySliceNegativeEndTest) {
    std::string user_input;
    user_input += "배열 목록 = [10, 20, 30, 40, 50]\n";
    user_input += "목록[:-1]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[10, 20, 30, 40]"), std::string::npos);
}

TEST_F(ReplTest, stringSliceTest) {
    std::string user_input;
    user_input += "문자 s = \"hello\"\n";
    user_input += "s[1:3]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("el"), std::string::npos);
}

TEST_F(ReplTest, stringSliceNegativeTest) {
    std::string user_input;
    user_input += "문자 s = \"hello\"\n";
    user_input += "s[:-1]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("hell"), std::string::npos);
}

TEST_F(ReplTest, existingIndexStillWorksTest) {
    std::string user_input;
    user_input += "배열 목록 = [10, 20, 30]\n";
    user_input += "목록[0]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("10"), std::string::npos);
}

// ===== VM REPL Tests =====

TEST_F(ReplTest, vmReplFunctionTest) {
    std::string user_input;
    user_input += "함수 두배(정수 n) -> 정수:\n";
    user_input += "    리턴 n * 2\n";
    user_input += "\n";
    user_input += "두배(21)\n";
    user_input += "종료하기\n";

    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("42"), std::string::npos);
}

// ===== Feature 1: Continue statement tests =====

TEST_F(ReplTest, continueWhileTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "정수 i = 0\n";
    user_input += "반복 i < 10 동안:\n";
    user_input += "    i += 1\n";
    user_input += "    만약 i % 2 == 0 라면:\n";
    user_input += "        계속\n";
    user_input += "    합계 += i\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    // 1+3+5+7+9 = 25
    EXPECT_NE(output.find("25"), std::string::npos);
}

TEST_F(ReplTest, continueForRangeTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "반복 정수 i = 0 부터 10 까지:\n";
    user_input += "    만약 i % 3 == 0 라면:\n";
    user_input += "        계속\n";
    user_input += "    합계 += i\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    // 1+2+4+5+7+8 = 27
    EXPECT_NE(output.find("27"), std::string::npos);
}

TEST_F(ReplTest, continueForEachTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "각각 정수 x [1, 2, 3, 4, 5] 에서:\n";
    user_input += "    만약 x == 3 라면:\n";
    user_input += "        계속\n";
    user_input += "    합계 += x\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    // 1+2+4+5 = 12
    EXPECT_NE(output.find("12"), std::string::npos);
}

// ===== Feature 2: Lambda tests =====

TEST_F(ReplTest, lambdaMapTest) {
    std::string user_input;
    user_input += "배열 결과 = 매핑([1, 2, 3], 함수(정수 x) -> 정수 x * 2)\n";
    user_input += "출력(결과)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[2, 4, 6]"), std::string::npos);
}

TEST_F(ReplTest, lambdaFilterTest) {
    std::string user_input;
    user_input += "배열 결과 = 걸러내기([1, 2, 3, 4, 5], 함수(정수 x) -> 논리 x > 3)\n";
    user_input += "출력(결과)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[4, 5]"), std::string::npos);
}

TEST_F(ReplTest, lambdaReduceTest) {
    std::string user_input;
    user_input += "정수 결과 = 줄이기([1, 2, 3, 4], 함수(정수 a, 정수 b) -> 정수 a + b, 0)\n";
    user_input += "결과\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("10"), std::string::npos);
}

TEST_F(ReplTest, lambdaAssignTest) {
    std::string user_input;
    user_input += "함수 적용(정수 x, 함수 f) -> 정수:\n";
    user_input += "    리턴 f(x)\n";
    user_input += "\n";
    user_input += "적용(5, 함수(정수 x) -> 정수 x * x)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("25"), std::string::npos);
}

// ===== Feature 5: String method chaining tests =====

TEST_F(ReplTest, stringMethodUpperTest) {
    std::string user_input;
    user_input += "\"hello\".대문자()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("HELLO"), std::string::npos);
}

TEST_F(ReplTest, stringMethodLowerTest) {
    std::string user_input;
    user_input += "\"WORLD\".소문자()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("world"), std::string::npos);
}

TEST_F(ReplTest, stringMethodLengthTest) {
    std::string user_input;
    user_input += "\"hello\".길이()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("5"), std::string::npos);
}

TEST_F(ReplTest, stringMethodTrimTest) {
    std::string user_input;
    user_input += "\"  hello  \".자르기()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("hello"), std::string::npos);
}

TEST_F(ReplTest, stringMethodContainsTest) {
    std::string user_input;
    user_input += "\"hello world\".포함(\"world\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("true"), std::string::npos);
}

TEST_F(ReplTest, stringMethodReplaceTest) {
    std::string user_input;
    user_input += "\"hello world\".치환(\"world\", \"korea\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("hello korea"), std::string::npos);
}

TEST_F(ReplTest, stringMethodSplitTest) {
    std::string user_input;
    user_input += "\"a,b,c\".분리(\",\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[a, b, c]"), std::string::npos);
}

TEST_F(ReplTest, arrayMethodLengthTest) {
    std::string user_input;
    user_input += "[1, 2, 3].길이()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, arrayMethodReverseTest) {
    std::string user_input;
    user_input += "[1, 2, 3].뒤집기()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[3, 2, 1]"), std::string::npos);
}

TEST_F(ReplTest, arrayMethodSortTest) {
    std::string user_input;
    user_input += "[3, 1, 2].정렬()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("[1, 2, 3]"), std::string::npos);
}

TEST_F(ReplTest, stringMethodChainingTest) {
    std::string user_input;
    user_input += "\"  Hello World  \".자르기().대문자()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("HELLO WORLD"), std::string::npos);
}

// ===== Feature 3: Extended test coverage =====

// Multi-level inheritance
TEST_F(ReplTest, multiLevelInheritanceTest) {
    std::string user_input;
    user_input += "클래스 생물:\n";
    user_input += "    문자 이름\n";
    user_input += "    생성(문자 이름):\n";
    user_input += "        자기.이름 = 이름\n";
    user_input += "    함수 설명() -> 문자:\n";
    user_input += "        리턴 자기.이름\n";
    user_input += "\n";
    user_input += "클래스 동물 < 생물:\n";
    user_input += "    문자 종류\n";
    user_input += "    생성(문자 이름, 문자 종류):\n";
    user_input += "        자기.이름 = 이름\n";
    user_input += "        자기.종류 = 종류\n";
    user_input += "\n";
    user_input += "클래스 강아지 < 동물:\n";
    user_input += "    함수 짖기() -> 문자:\n";
    user_input += "        리턴 \"멍멍\"\n";
    user_input += "\n";
    user_input += "강아지 뽀삐 = 강아지(\"뽀삐\", \"포메라니안\")\n";
    user_input += "뽀삐.설명()\n";
    user_input += "뽀삐.짖기()\n";
    user_input += "뽀삐.종류\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("뽀삐"), std::string::npos);
    EXPECT_NE(output.find("멍멍"), std::string::npos);
    EXPECT_NE(output.find("포메라니안"), std::string::npos);
}

// Match with strings
TEST_F(ReplTest, matchStringTest) {
    std::string user_input;
    user_input += "문자 색깔 = \"빨강\"\n";
    user_input += "비교 색깔:\n";
    user_input += "    경우 \"파랑\":\n";
    user_input += "        출력(\"차가움\")\n";
    user_input += "    경우 \"빨강\":\n";
    user_input += "        출력(\"따뜻함\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"모름\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("따뜻함"), std::string::npos);
}

// Match default only
TEST_F(ReplTest, matchDefaultOnlyTest) {
    std::string user_input;
    user_input += "정수 x = 42\n";
    user_input += "비교 x:\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"기본값\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("기본값"), std::string::npos);
}

// For-range boundary
TEST_F(ReplTest, forRangeBoundaryTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "반복 정수 i = 5 부터 5 까지:\n";
    user_input += "    합계 += 1\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("0"), std::string::npos);
}

// Nested try-catch
TEST_F(ReplTest, nestedTryCatchTest) {
    std::string user_input;
    user_input += "시도:\n";
    user_input += "    시도:\n";
    user_input += "        정수 x = 10 / 0\n";
    user_input += "    실패 내부오류:\n";
    user_input += "        출력(\"내부:\", 내부오류)\n";
    user_input += "실패 외부오류:\n";
    user_input += "    출력(\"외부:\", 외부오류)\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("내부:"), std::string::npos);
}

// Tuple creation and indexing
TEST_F(ReplTest, tupleCreationAndIndexTest) {
    std::string user_input;
    user_input += "(100, 200, 300)[0]\n";
    user_input += "(100, 200, 300)[2]\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("100"), std::string::npos);
    EXPECT_NE(output.find("300"), std::string::npos);
}

// Default parameter edge cases
TEST_F(ReplTest, defaultParamAllDefaultTest) {
    std::string user_input;
    user_input += "함수 인사(문자 이름 = \"세계\", 문자 접두 = \"안녕 \") -> 문자:\n";
    user_input += "    리턴 접두 + 이름\n";
    user_input += "\n";
    user_input += "인사()\n";
    user_input += "인사(\"한국\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("안녕 세계"), std::string::npos);
    EXPECT_NE(output.find("안녕 한국"), std::string::npos);
}

// ===== VM tests for new features =====

TEST_F(ReplTest, vmContinueWhileTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "정수 i = 0\n";
    user_input += "반복 i < 10 동안:\n";
    user_input += "    i += 1\n";
    user_input += "    만약 i % 2 == 0 라면:\n";
    user_input += "        계속\n";
    user_input += "    합계 += i\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("25"), std::string::npos);
}

TEST_F(ReplTest, vmForRangeTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "반복 정수 i = 0 부터 5 까지:\n";
    user_input += "    합계 += i\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("10"), std::string::npos);
}

TEST_F(ReplTest, vmForRangeContinueTest) {
    std::string user_input;
    user_input += "정수 합계 = 0\n";
    user_input += "반복 정수 i = 0 부터 10 까지:\n";
    user_input += "    만약 i % 3 == 0 라면:\n";
    user_input += "        계속\n";
    user_input += "    합계 += i\n";
    user_input += "\n";
    user_input += "합계\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("27"), std::string::npos);
}

TEST_F(ReplTest, vmStringMethodTest) {
    std::string user_input;
    user_input += "\"hello\".대문자()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("HELLO"), std::string::npos);
}

TEST_F(ReplTest, vmInheritanceTest) {
    std::string user_input;
    user_input += "클래스 동물:\n";
    user_input += "    문자 이름\n";
    user_input += "    생성(문자 이름):\n";
    user_input += "        자기.이름 = 이름\n";
    user_input += "    함수 소리() -> 문자:\n";
    user_input += "        리턴 \"...\"\n";
    user_input += "\n";
    user_input += "클래스 고양이 < 동물:\n";
    user_input += "    함수 소리() -> 문자:\n";
    user_input += "        리턴 \"야옹\"\n";
    user_input += "\n";
    user_input += "고양이 나비 = 고양이(\"나비\")\n";
    user_input += "나비.이름\n";
    user_input += "나비.소리()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("나비"), std::string::npos);
    EXPECT_NE(output.find("야옹"), std::string::npos);
}

// Task 1: 문자열 보간 테스트
TEST_F(ReplTest, stringInterpolationPrintTest) {
    std::string user_input;
    user_input += "정수 x = 42\n";
    user_input += "출력(\"값은 {x}입니다\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("값은 42입니다"), std::string::npos);
}

TEST_F(ReplTest, stringInterpolationMultipleVarsTest) {
    std::string user_input;
    user_input += "문자 이름 = \"홍길동\"\n";
    user_input += "정수 나이 = 25\n";
    user_input += "출력(\"{이름}님은 {나이}살입니다\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("홍길동님은 25살입니다"), std::string::npos);
}

// Task 2: 거듭제곱 연산자 테스트
TEST_F(ReplTest, powerOperatorIntegerTest) {
    std::string user_input;
    user_input += "2 ** 10\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("1024"), std::string::npos);
}

TEST_F(ReplTest, powerOperatorFloatTest) {
    std::string user_input;
    user_input += "2.0 ** 3.0\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("8"), std::string::npos);
}

TEST_F(ReplTest, powerOperatorPrecedenceTest) {
    std::string user_input;
    user_input += "2 * 3 ** 2\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    // 3 ** 2 = 9, 2 * 9 = 18 (** has higher precedence than *)
    EXPECT_NE(output.find("18"), std::string::npos);
}

// Task 3: 이스케이프 시퀀스 테스트
TEST_F(ReplTest, escapeSequenceBackslashTest) {
    std::string user_input;
    user_input += "출력(\"경로: C:\\\\폴더\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("경로: C:\\폴더"), std::string::npos);
}

TEST_F(ReplTest, escapeSequenceNewlineTest) {
    std::string user_input;
    user_input += "출력(\"첫줄\\n둘째줄\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("첫줄"), std::string::npos);
    EXPECT_NE(output.find("둘째줄"), std::string::npos);
}

TEST_F(ReplTest, escapeSequenceTabTest) {
    std::string user_input;
    user_input += "출력(\"이름\\t나이\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("이름\t나이"), std::string::npos);
}

// VM 모드에서도 거듭제곱 테스트
TEST_F(ReplTest, vmPowerOperatorTest) {
    std::string user_input;
    user_input += "정수 결과 = 2 ** 8\n";
    user_input += "결과\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("256"), std::string::npos);
}

// VM 모드에서 문자열 보간 테스트
TEST_F(ReplTest, vmStringInterpolationTest) {
    std::string user_input;
    user_input += "정수 x = 100\n";
    user_input += "출력(\"값: {x}\")\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("값: 100"), std::string::npos);
}

// Optional 타입 테스트
TEST_F(ReplTest, optionalTypeDeclarationWithNull) {
    std::string user_input;
    user_input += "정수? x = 없음\n";
    user_input += "x == 없음\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("true"), std::string::npos);
}

TEST_F(ReplTest, optionalTypeDeclarationWithValue) {
    std::string user_input;
    user_input += "정수? x = 42\n";
    user_input += "x\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("42"), std::string::npos);
}

TEST_F(ReplTest, optionalTypeReassignNullToValue) {
    std::string user_input;
    user_input += "정수? x = 없음\n";
    user_input += "x = 10\n";
    user_input += "x\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("10"), std::string::npos);
}

TEST_F(ReplTest, optionalTypeReassignValueToNull) {
    std::string user_input;
    user_input += "정수? x = 42\n";
    user_input += "x = 없음\n";
    user_input += "x == 없음\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("true"), std::string::npos);
}

TEST_F(ReplTest, nonOptionalRejectsNull) {
    std::string user_input;
    user_input += "정수 x = 없음\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("선택적 타입이 아닌 변수에"), std::string::npos);
}

TEST_F(ReplTest, nonOptionalRejectsNullAssignment) {
    std::string user_input;
    user_input += "정수 x = 10\n";
    user_input += "x = 없음\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("선택적 타입이 아닌 변수에"), std::string::npos);
}

TEST_F(ReplTest, optionalStringType) {
    std::string user_input;
    user_input += "문자? s = 없음\n";
    user_input += "s = \"안녕\"\n";
    user_input += "s\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("안녕"), std::string::npos);
}

TEST_F(ReplTest, optionalFunctionParameter) {
    std::string user_input;
    user_input += "함수 테스트(정수? x) -> 논리:\n";
    user_input += "    리턴 x == 없음\n";
    user_input += "테스트(없음)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("true"), std::string::npos);
}

TEST_F(ReplTest, optionalFunctionParameterWithValue) {
    std::string user_input;
    user_input += "함수 테스트(정수? x) -> 정수?:\n";
    user_input += "    리턴 x\n";
    user_input += "테스트(42)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("42"), std::string::npos);
}

// 패턴 매칭: 범위 패턴
TEST_F(ReplTest, matchRangePatternTest) {
    std::string user_input;
    user_input += "정수 x = 3\n";
    user_input += "비교 x:\n";
    user_input += "    경우 1~5:\n";
    user_input += "        출력(\"범위안\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"범위밖\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("범위안"), std::string::npos);
}

// 패턴 매칭: 범위 밖
TEST_F(ReplTest, matchRangePatternOutsideTest) {
    std::string user_input;
    user_input += "정수 x = 10\n";
    user_input += "비교 x:\n";
    user_input += "    경우 1~5:\n";
    user_input += "        출력(\"범위안\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"범위밖\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("범위밖"), std::string::npos);
}

// 패턴 매칭: 타입 패턴
TEST_F(ReplTest, matchTypePatternIntegerTest) {
    std::string user_input;
    user_input += "정수 x = 42\n";
    user_input += "비교 x:\n";
    user_input += "    경우 문자:\n";
    user_input += "        출력(\"문자열\")\n";
    user_input += "    경우 정수:\n";
    user_input += "        출력(\"정수값\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"기타\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("정수값"), std::string::npos);
}

// 패턴 매칭: 타입 패턴 - 문자열
TEST_F(ReplTest, matchTypePatternStringTest) {
    std::string user_input;
    user_input += "문자 s = \"안녕\"\n";
    user_input += "비교 s:\n";
    user_input += "    경우 정수:\n";
    user_input += "        출력(\"정수값\")\n";
    user_input += "    경우 문자:\n";
    user_input += "        출력(\"문자열\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"기타\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("문자열"), std::string::npos);
}

// 패턴 매칭: 조건 가드
TEST_F(ReplTest, matchGuardConditionTest) {
    std::string user_input;
    user_input += "정수 x = 7\n";
    user_input += "비교 x:\n";
    user_input += "    경우 7 만약 x > 5:\n";
    user_input += "        출력(\"큰7\")\n";
    user_input += "    경우 7:\n";
    user_input += "        출력(\"작은7\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"기타\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("큰7"), std::string::npos);
}

// 패턴 매칭: 범위 + 값 혼합
TEST_F(ReplTest, matchMixedPatternsTest) {
    std::string user_input;
    user_input += "정수 x = 100\n";
    user_input += "비교 x:\n";
    user_input += "    경우 1~10:\n";
    user_input += "        출력(\"작은수\")\n";
    user_input += "    경우 100:\n";
    user_input += "        출력(\"백\")\n";
    user_input += "    기본:\n";
    user_input += "        출력(\"기타\")\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("백"), std::string::npos);
}

// ===== 클로저 테스트 =====

TEST_F(ReplTest, closureBasicCapture) {
    std::string user_input;
    user_input += "함수 더하기기(정수 x) -> 함수:\n";
    user_input += "    함수 내부(정수 y) -> 정수:\n";
    user_input += "        리턴 x + y\n";
    user_input += "    리턴 내부\n";
    user_input += "\n";
    user_input += "함수 f = 더하기기(3)\n";
    user_input += "f(5)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("8"), std::string::npos);
}

TEST_F(ReplTest, closureCounter) {
    std::string user_input;
    user_input += "함수 카운터() -> 함수:\n";
    user_input += "    정수 count = 0\n";
    user_input += "    함수 증가() -> 정수:\n";
    user_input += "        count += 1\n";
    user_input += "        리턴 count\n";
    user_input += "    리턴 증가\n";
    user_input += "\n";
    user_input += "함수 c = 카운터()\n";
    user_input += "출력(c())\n";
    user_input += "출력(c())\n";
    user_input += "출력(c())\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, closureIndependentCounters) {
    std::string user_input;
    user_input += "함수 카운터() -> 함수:\n";
    user_input += "    정수 count = 0\n";
    user_input += "    함수 증가() -> 정수:\n";
    user_input += "        count += 1\n";
    user_input += "        리턴 count\n";
    user_input += "    리턴 증가\n";
    user_input += "\n";
    user_input += "함수 c1 = 카운터()\n";
    user_input += "함수 c2 = 카운터()\n";
    user_input += "출력(c1())\n";
    user_input += "출력(c1())\n";
    user_input += "출력(c2())\n";
    user_input += "출력(c1())\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    // c1() -> 1, c1() -> 2, c2() -> 1 (independent), c1() -> 3
    // Output should contain lines: 1, 2, 1, 3
    // Check output has "3" from c1's third call
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, closureDeeplyNested) {
    std::string user_input;
    user_input += "함수 외부() -> 함수:\n";
    user_input += "    정수 x = 1\n";
    user_input += "    함수 중간() -> 함수:\n";
    user_input += "        함수 내부() -> 정수:\n";
    user_input += "            x += 1\n";
    user_input += "            리턴 x\n";
    user_input += "        리턴 내부\n";
    user_input += "    리턴 중간\n";
    user_input += "\n";
    user_input += "함수 중간fn = 외부()\n";
    user_input += "함수 내부fn = 중간fn()\n";
    user_input += "출력(내부fn())\n";
    user_input += "출력(내부fn())\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, closureBasicCaptureVM) {
    std::string user_input;
    user_input += "함수 더하기기(정수 x) -> 함수:\n";
    user_input += "    함수 내부(정수 y) -> 정수:\n";
    user_input += "        리턴 x + y\n";
    user_input += "    리턴 내부\n";
    user_input += "\n";
    user_input += "함수 f = 더하기기(3)\n";
    user_input += "f(5)\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("8"), std::string::npos);
}

TEST_F(ReplTest, closureCounterVM) {
    std::string user_input;
    user_input += "함수 카운터() -> 함수:\n";
    user_input += "    정수 count = 0\n";
    user_input += "    함수 증가() -> 정수:\n";
    user_input += "        count += 1\n";
    user_input += "        리턴 count\n";
    user_input += "    리턴 증가\n";
    user_input += "\n";
    user_input += "함수 c = 카운터()\n";
    user_input += "출력(c())\n";
    user_input += "출력(c())\n";
    user_input += "출력(c())\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, closureIndependentCountersVM) {
    std::string user_input;
    user_input += "함수 카운터() -> 함수:\n";
    user_input += "    정수 count = 0\n";
    user_input += "    함수 증가() -> 정수:\n";
    user_input += "        count += 1\n";
    user_input += "        리턴 count\n";
    user_input += "    리턴 증가\n";
    user_input += "\n";
    user_input += "함수 c1 = 카운터()\n";
    user_input += "함수 c2 = 카운터()\n";
    user_input += "출력(c1())\n";
    user_input += "출력(c1())\n";
    user_input += "출력(c2())\n";
    user_input += "출력(c1())\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, closureDeeplyNestedVM) {
    std::string user_input;
    user_input += "함수 외부() -> 함수:\n";
    user_input += "    정수 x = 1\n";
    user_input += "    함수 중간() -> 함수:\n";
    user_input += "        함수 내부() -> 정수:\n";
    user_input += "            x += 1\n";
    user_input += "            리턴 x\n";
    user_input += "        리턴 내부\n";
    user_input += "    리턴 중간\n";
    user_input += "\n";
    user_input += "함수 중간fn = 외부()\n";
    user_input += "함수 내부fn = 중간fn()\n";
    user_input += "출력(내부fn())\n";
    user_input += "출력(내부fn())\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input, true);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, functionTypeVariable) {
    std::string user_input;
    user_input += "함수 삼() -> 정수:\n";
    user_input += "    리턴 3\n";
    user_input += "\n";
    user_input += "함수 f = 삼\n";
    user_input += "f()\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("3"), std::string::npos);
}

// ===== Feature Tests: Generator (생산) =====

TEST_F(ReplTest, generatorBasicTest) {
    std::string user_input;
    user_input += "함수 숫자생성() -> 정수:\n";
    user_input += "    생산 1\n";
    user_input += "    생산 2\n";
    user_input += "    생산 3\n";
    user_input += "\n";
    user_input += "각각 정수 x 숫자생성() 에서:\n";
    user_input += "    출력(x)\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(ReplTest, generatorWithLoopTest) {
    std::string user_input;
    user_input += "함수 범위(정수 끝) -> 정수:\n";
    user_input += "    정수 i = 0\n";
    user_input += "    반복 i < 끝 동안:\n";
    user_input += "        생산 i\n";
    user_input += "        i += 1\n";
    user_input += "\n";
    user_input += "각각 정수 x 범위(5) 에서:\n";
    user_input += "    출력(x)\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("0"), std::string::npos);
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
    EXPECT_NE(output.find("4"), std::string::npos);
}

TEST_F(ReplTest, generatorWithParameterTest) {
    std::string user_input;
    user_input += "함수 짝수생성(정수 최대) -> 정수:\n";
    user_input += "    정수 i = 0\n";
    user_input += "    반복 i < 최대 동안:\n";
    user_input += "        생산 i * 2\n";
    user_input += "        i += 1\n";
    user_input += "\n";
    user_input += "각각 정수 n 짝수생성(4) 에서:\n";
    user_input += "    출력(n)\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("0"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("4"), std::string::npos);
    EXPECT_NE(output.find("6"), std::string::npos);
}

TEST_F(ReplTest, generatorStringTest) {
    std::string user_input;
    user_input += "함수 인사생성() -> 문자:\n";
    user_input += "    생산 \"안녕\"\n";
    user_input += "    생산 \"하세요\"\n";
    user_input += "\n";
    user_input += "각각 문자 s 인사생성() 에서:\n";
    user_input += "    출력(s)\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("안녕"), std::string::npos);
    EXPECT_NE(output.find("하세요"), std::string::npos);
}

TEST_F(ReplTest, generatorWithBreakTest) {
    std::string user_input;
    user_input += "함수 무한숫자() -> 정수:\n";
    user_input += "    정수 i = 0\n";
    user_input += "    반복 i < 100 동안:\n";
    user_input += "        생산 i\n";
    user_input += "        i += 1\n";
    user_input += "\n";
    user_input += "각각 정수 x 무한숫자() 에서:\n";
    user_input += "    만약 x == 3 라면:\n";
    user_input += "        중단\n";
    user_input += "    출력(x)\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("0"), std::string::npos);
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    // 3 should NOT be printed (break before print)
}

TEST_F(ReplTest, generatorWithConditionTest) {
    std::string user_input;
    user_input += "함수 조건생산(정수 끝) -> 정수:\n";
    user_input += "    정수 i = 0\n";
    user_input += "    반복 i < 끝 동안:\n";
    user_input += "        만약 i % 2 == 0 라면:\n";
    user_input += "            생산 i\n";
    user_input += "        i += 1\n";
    user_input += "\n";
    user_input += "각각 정수 x 조건생산(6) 에서:\n";
    user_input += "    출력(x)\n";
    user_input += "\n";
    user_input += "종료하기\n";
    string output = runRepl(user_input);
    EXPECT_NE(output.find("0"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("4"), std::string::npos);
}

// ===== 인덱스 대입 =====

TEST_F(ReplTest, indexAssignmentArray) {
    string user_input = "배열 목록 = [1, 2, 3]\n목록[0] = 10\n출력(목록[0])\n종료하기\n";
    auto output = runRepl(user_input);
    EXPECT_TRUE(output.find("10") != string::npos);
}

TEST_F(ReplTest, indexAssignmentHashMap) {
    string user_input = "사전 정보 = {\"이름\": \"홍길동\"}\n정보[\"나이\"] = \"25\"\n출력(정보[\"나이\"])\n종료하기\n";
    auto output = runRepl(user_input);
    EXPECT_TRUE(output.find("25") != string::npos);
}
