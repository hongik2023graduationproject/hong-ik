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
    user_input += "정수 x = 없음\n";
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
