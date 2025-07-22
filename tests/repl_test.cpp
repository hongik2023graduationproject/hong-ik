#include "ast/expressions.h"
#include "ast/literals.h"
#include "ast/program.h"
#include "ast/statements.h"
#include "evaluator/evaluator.h"
#include "parser/parser.h"
#include "repl/repl.h"
#include <gtest/gtest.h>

using namespace std;

class ReplTest : public ::testing::Test {
protected:
    Repl* repl = new Repl();
};



TEST_F(ReplTest, testTest) {

    std::string user_input;
    user_input += "1 + 2\n";
    user_input += "[정수] a = 10\n";
    user_input += "a + 5\n";
    user_input += "종료하기\n";

    // 1. 준비: 가상 입력 및 출력 버퍼 생성
    std::istringstream fakeStdin(user_input);
    std::ostringstream fakeStdout;

    // 2. std::cin, std::cout 버퍼 백업
    auto* cinbuf = std::cin.rdbuf();
    auto* coutbuf = std::cout.rdbuf();

    // 3. 버퍼 교체
    std::cin.rdbuf(fakeStdin.rdbuf());
    std::cout.rdbuf(fakeStdout.rdbuf());

    // 4. REPL 실행
    Repl repl;
    repl.Run();

    // 5. 원복
    std::cin.rdbuf(cinbuf);
    std::cout.rdbuf(coutbuf);

    // 6. 실제 출력 결과 검사
    std::string output = fakeStdout.str();
    // std::cout << "Result:\n" << output << std::endl; // 디버깅

    // 7. 원하는 결과가 출력됐는지 확인 (예: "3")
    EXPECT_NE(output.find("3"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
    EXPECT_NE(output.find("15"), std::string::npos);
}



TEST_F(ReplTest, ifTest1) {
    std::string user_input;
    user_input += "[정수] 사과 = 11\n";
    user_input += "만약 사과 - 2 == 9 라면:\n";
    user_input += "    사과 = 5\n";
    user_input += "\n";
    user_input += "종료하기\n";

    // 1. 준비: 가상 입력 및 출력 버퍼 생성
    std::istringstream fakeStdin(user_input);
    std::ostringstream fakeStdout;

    // 2. std::cin, std::cout 버퍼 백업
    auto* cinbuf = std::cin.rdbuf();
    auto* coutbuf = std::cout.rdbuf();

    // 3. 버퍼 교체
    std::cin.rdbuf(fakeStdin.rdbuf());
    std::cout.rdbuf(fakeStdout.rdbuf());

    // 4. REPL 실행
    Repl repl;
    repl.Run();

    // 5. 원복
    std::cin.rdbuf(cinbuf);
    std::cout.rdbuf(coutbuf);

    // 6. 실제 출력 결과 검사
    std::string output = fakeStdout.str();
    // std::cout << "Result:\n" << output << std::endl; // 디버깅

    // 7. 원하는 결과가 출력됐는지 확인 (예: "3")
    EXPECT_NE(output.find("11"), std::string::npos);
    EXPECT_NE(output.find("5"), std::string::npos);
}




TEST_F(ReplTest, ifTest2) {
    std::string user_input;
    user_input += "[정수] 사과 = 11\n";
    user_input += "만약 사과 - 2 == 9 라면:\n";
    user_input += "    사과 = 5\n";
    user_input += "아니면:\n";
    user_input += "    사과 = 10\n";
    user_input += "\n";
    user_input += "종료하기\n";

    // 1. 준비: 가상 입력 및 출력 버퍼 생성
    std::istringstream fakeStdin(user_input);
    std::ostringstream fakeStdout;

    // 2. std::cin, std::cout 버퍼 백업
    auto* cinbuf = std::cin.rdbuf();
    auto* coutbuf = std::cout.rdbuf();

    // 3. 버퍼 교체
    std::cin.rdbuf(fakeStdin.rdbuf());
    std::cout.rdbuf(fakeStdout.rdbuf());

    // 4. REPL 실행
    Repl repl;
    repl.Run();

    // 5. 원복
    std::cin.rdbuf(cinbuf);
    std::cout.rdbuf(coutbuf);

    // 6. 실제 출력 결과 검사
    std::string output = fakeStdout.str();
    // std::cout << "Result:\n" << output << std::endl; // 디버깅

    // 7. 원하는 결과가 출력됐는지 확인 (예: "3")
    EXPECT_NE(output.find("11"), std::string::npos);
    EXPECT_NE(output.find("5"), std::string::npos);
}



TEST_F(ReplTest, ifTest3) {
    std::string user_input;
    user_input += "[정수] 사과 = 15\n";
    user_input += "만약 사과 - 2 == 9 라면:\n";
    user_input += "    사과 = 5\n";
    user_input += "아니면:\n";
    user_input += "    사과 = 10\n";
    user_input += "\n";
    user_input += "종료하기\n";

    // 1. 준비: 가상 입력 및 출력 버퍼 생성
    std::istringstream fakeStdin(user_input);
    std::ostringstream fakeStdout;

    // 2. std::cin, std::cout 버퍼 백업
    auto* cinbuf = std::cin.rdbuf();
    auto* coutbuf = std::cout.rdbuf();

    // 3. 버퍼 교체
    std::cin.rdbuf(fakeStdin.rdbuf());
    std::cout.rdbuf(fakeStdout.rdbuf());

    // 4. REPL 실행
    Repl repl;
    repl.Run();

    // 5. 원복
    std::cin.rdbuf(cinbuf);
    std::cout.rdbuf(coutbuf);

    // 6. 실제 출력 결과 검사
    std::string output = fakeStdout.str();
    // std::cout << "Result:\n" << output << std::endl; // 디버깅

    // 7. 원하는 결과가 출력됐는지 확인 (예: "3")
    EXPECT_NE(output.find("15"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
}


TEST_F(ReplTest, functionTest) {
    std::string user_input;
    user_input += "함수: [정수]수 피보나치 -> [정수]:\n";
    user_input += "    만약 수 == 0 라면:\n";
    user_input += "        리턴 0\n";
    user_input += "    만약 수 == 1 라면:\n";
    user_input += "        리턴 1\n";
    user_input += "    리턴 :(수 - 1)피보나치 + :(수 - 2)피보나치\n";
    user_input += "\n";
    user_input += ":(10)피보나치\n";
    user_input += "종료하기\n";

    // 1. 준비: 가상 입력 및 출력 버퍼 생성
    std::istringstream fakeStdin(user_input);
    std::ostringstream fakeStdout;

    // 2. std::cin, std::cout 버퍼 백업
    auto* cinbuf = std::cin.rdbuf();
    auto* coutbuf = std::cout.rdbuf();

    // 3. 버퍼 교체
    std::cin.rdbuf(fakeStdin.rdbuf());
    std::cout.rdbuf(fakeStdout.rdbuf());

    // 4. REPL 실행
    Repl repl;
    repl.Run();

    // 5. 원복
    std::cin.rdbuf(cinbuf);
    std::cout.rdbuf(coutbuf);

    // 6. 실제 출력 결과 검사
    std::string output = fakeStdout.str();
    std::cout << "Result:\n" << output << std::endl; // 디버깅

    // 7. 원하는 결과가 출력됐는지 확인 (예: "3")
    EXPECT_NE(output.find("함수:"), std::string::npos);
    EXPECT_NE(output.find("55"), std::string::npos);
}
