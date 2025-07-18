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
    std::cout << "Result:\n" << output << std::endl; // 디버깅

    // 7. 원하는 결과가 출력됐는지 확인 (예: "3")
    EXPECT_NE(output.find("3"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
    EXPECT_NE(output.find("15"), std::string::npos);
}
