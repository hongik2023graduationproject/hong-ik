// '아니면 만약 <조건> 라면:' else-if 체인 — 파서 구조, 오류 메시지, VM·트리워킹 결과 일치, 타입 검사 거짓 경고 없음.
#include "analyzer/type_checker.h"
#include "ast/statements.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "run_source_helper.h"
#include "utf8_converter/utf8_converter.h"
#include "util/token_utils.h"
#include <gtest/gtest.h>
#include <string>

using test_helper::expectSameOutputOnBoth;
using test_helper::runSourceFile;

namespace {

    std::shared_ptr<Program> parseSource(Parser& parser, const std::string& src) {
        Lexer lexer;
        auto tokens = lexer.Tokenize(Utf8Converter::Convert(src));
        token_utils::appendMissingBlockClosers(tokens);
        return parser.Parsing(tokens);
    }

    std::string firstParseError(const std::string& src) {
        Parser parser;
        parseSource(parser, src);
        return parser.getErrors().empty() ? std::string() : parser.getErrors().front();
    }

} // namespace

TEST(ElseIfParserTest, ChainDesugarsToNestedIfInElseBlock) {
    Parser parser;
    auto program = parseSource(parser, "정수 x = 1\n"
                                       "만약 x > 2 라면:\n"
                                       "    출력(\"a\")\n"
                                       "아니면 만약 x > 1 라면:\n"
                                       "    출력(\"b\")\n"
                                       "아니면 만약 x > 0 라면:\n"
                                       "    출력(\"c\")\n"
                                       "아니면:\n"
                                       "    출력(\"d\")\n");
    ASSERT_TRUE(parser.getErrors().empty()) << parser.getErrors().front();
    ASSERT_EQ(program->statements.size(), 2u);

    auto first = std::dynamic_pointer_cast<IfStatement>(program->statements[1]);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->line, 2);
    ASSERT_NE(first->then, nullptr);
    ASSERT_EQ(first->then->statements.size(), 1u);

    auto second = std::dynamic_pointer_cast<IfStatement>(first->then->statements[0]);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->line, 4);
    EXPECT_EQ(second->condition->String(), "(x > 1)");
    ASSERT_NE(second->then, nullptr);
    ASSERT_EQ(second->then->statements.size(), 1u);

    auto third = std::dynamic_pointer_cast<IfStatement>(second->then->statements[0]);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(third->line, 6);
    // 마지막 '아니면:' 블록은 중첩 if가 아닌 일반 블록
    ASSERT_NE(third->then, nullptr);
    ASSERT_EQ(third->then->statements.size(), 1u);
    EXPECT_EQ(std::dynamic_pointer_cast<IfStatement>(third->then->statements[0]), nullptr);
}

TEST(ElseIfParserTest, ChainWithoutFinalElse) {
    Parser parser;
    auto program = parseSource(parser, "만약 true 라면:\n"
                                       "    1\n"
                                       "아니면 만약 false 라면:\n"
                                       "    2\n"
                                       "3\n");
    ASSERT_TRUE(parser.getErrors().empty()) << parser.getErrors().front();
    ASSERT_EQ(program->statements.size(), 2u);
    auto first = std::dynamic_pointer_cast<IfStatement>(program->statements[0]);
    ASSERT_NE(first, nullptr);
    auto second = std::dynamic_pointer_cast<IfStatement>(first->then->statements[0]);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->then, nullptr);
}

TEST(ElseIfParserTest, ElseFollowedByGarbageIsClearKoreanError) {
    auto err = firstParseError("정수 x = 1\n"
                               "만약 x > 5 라면:\n"
                               "    출력(\"a\")\n"
                               "아니면 x > 0 라면:\n"
                               "    출력(\"b\")\n");
    EXPECT_NE(err.find("[줄 4]"), std::string::npos) << err;
    EXPECT_NE(err.find("'아니면' 뒤에는 ':' 또는 '만약 <조건> 라면:'이 와야 합니다."), std::string::npos) << err;
}

TEST(ElseIfParserTest, ElseIfMissingRamyeonReportsExpectedToken) {
    auto err = firstParseError("정수 x = 1\n"
                               "만약 x > 5 라면:\n"
                               "    출력(\"a\")\n"
                               "아니면 만약 x > 0:\n"
                               "    출력(\"b\")\n");
    EXPECT_NE(err.find("[줄 4]"), std::string::npos) << err;
    EXPECT_NE(err.find("예상 토큰: '라면'"), std::string::npos) << err;
}

TEST(ElseIfParserTest, ElseIfAfterFinalElseIsRejected) {
    auto err = firstParseError("정수 x = 1\n"
                               "만약 x > 5 라면:\n"
                               "    출력(\"a\")\n"
                               "아니면:\n"
                               "    출력(\"c\")\n"
                               "아니면 만약 x > 0 라면:\n"
                               "    출력(\"b\")\n");
    EXPECT_NE(err.find("[줄 6]"), std::string::npos) << err;
    EXPECT_NE(err.find("'아니면:' 블록 뒤에는 '아니면'이 더 올 수 없습니다."), std::string::npos) << err;
}

TEST(ElseIfParserTest, DanglingElseAtEndOfFile) {
    auto err = firstParseError("만약 true 라면:\n"
                               "    1\n"
                               "아니면");
    EXPECT_NE(err.find("'아니면' 뒤에는"), std::string::npos) << err;
}

TEST(ElseIfRunTest, GradeChainPicksFirstMatchingBranch) {
    const std::string program = "각각 정수 점수 [95, 90, 85, 80, 79, 0] 에서:\n"
                                "    만약 점수 >= 90 라면:\n"
                                "        출력(\"A\")\n"
                                "    아니면 만약 점수 >= 80 라면:\n"
                                "        출력(\"B\")\n"
                                "    아니면:\n"
                                "        출력(\"C\")\n";
    expectSameOutputOnBoth(program, "A\nA\nB\nB\nC\nC\n");
}

TEST(ElseIfRunTest, ChainInsideFunctionWithReturnsAndNoFinalElse) {
    const std::string program = "함수 등급(정수 s) -> 문자:\n"
                                "    만약 s >= 90 라면:\n"
                                "        리턴 \"A\"\n"
                                "    아니면 만약 s >= 80 라면:\n"
                                "        리턴 \"B\"\n"
                                "    아니면 만약 s >= 70 라면:\n"
                                "        리턴 \"C\"\n"
                                "    리턴 \"F\"\n"
                                "출력(등급(99))\n"
                                "출력(등급(80))\n"
                                "출력(등급(70))\n"
                                "출력(등급(69))\n";
    expectSameOutputOnBoth(program, "A\nB\nC\nF\n");
}

TEST(ElseIfRunTest, NestedChainsAndLoopControl) {
    const std::string program = "정수 합 = 0\n"
                                "반복 정수 i = 0 부터 10 까지:\n"
                                "    만약 i == 3 라면:\n"
                                "        계속\n"
                                "    아니면 만약 i == 8 라면:\n"
                                "        중단\n"
                                "    아니면 만약 i % 2 == 0 라면:\n"
                                "        만약 i > 4 라면:\n"
                                "            합 += 100\n"
                                "        아니면 만약 i > 1 라면:\n"
                                "            합 += 10\n"
                                "        아니면:\n"
                                "            합 += 1\n"
                                "    아니면:\n"
                                "        합 += 1000\n"
                                "출력(합)\n";
    // i=0:+1, 1:+1000, 2:+10, 3:계속, 4:+10, 5:+1000, 6:+100, 7:+1000, 8:중단
    expectSameOutputOnBoth(program, "3121\n");
}

TEST(ElseIfRunTest, BranchDeclarationsAreBlockScoped) {
    // 각 분기에서 같은 이름을 선언해도 충돌하지 않고, 체인 밖에서는 보이지 않는다.
    const std::string ok = "정수 x = 2\n"
                           "만약 x == 1 라면:\n"
                           "    문자 결과 = \"하나\"\n"
                           "    출력(결과)\n"
                           "아니면 만약 x == 2 라면:\n"
                           "    문자 결과 = \"둘\"\n"
                           "    출력(결과)\n"
                           "아니면:\n"
                           "    문자 결과 = \"기타\"\n"
                           "    출력(결과)\n";
    expectSameOutputOnBoth(ok, "둘\n");

    const std::string leak = "정수 x = 2\n"
                             "만약 x == 1 라면:\n"
                             "    정수 y = 1\n"
                             "아니면 만약 x == 2 라면:\n"
                             "    정수 y = 2\n"
                             "출력(y)\n";
    for (bool vm : {true, false}) {
        auto r = runSourceFile(leak, vm, TypeCheckMode::Off);
        EXPECT_NE(r.output.find("'y' 존재하지 않는 식별자입니다."), std::string::npos)
            << (vm ? "VM: " : "트리워킹: ") << r.output;
    }
}

TEST(ElseIfRunTest, OnlyFirstTrueConditionIsEvaluatedAfterIt) {
    // 앞 조건이 참이면 뒤 조건식은 평가되지 않는다 (부작용 없는지 확인).
    const std::string program = "정수 호출수 = 0\n"
                                "함수 검사(논리 v) -> 논리:\n"
                                "    호출수 += 1\n"
                                "    리턴 v\n"
                                "만약 검사(false) 라면:\n"
                                "    출력(\"1\")\n"
                                "아니면 만약 검사(true) 라면:\n"
                                "    출력(\"2\")\n"
                                "아니면 만약 검사(true) 라면:\n"
                                "    출력(\"3\")\n"
                                "출력(호출수)\n";
    expectSameOutputOnBoth(program, "2\n2\n");
}

TEST(ElseIfRunTest, LongChain) {
    constexpr int kBranches = 300;
    std::string program     = "정수 x = " + std::to_string(kBranches - 1) + "\n";
    for (int i = 0; i < kBranches; i++) {
        program += (i == 0 ? "만약 x == " : "아니면 만약 x == ") + std::to_string(i) + " 라면:\n";
        program += "    출력(" + std::to_string(i) + ")\n";
    }
    program += "아니면:\n    출력(-1)\n";
    expectSameOutputOnBoth(program, std::to_string(kBranches - 1) + "\n");
}

TEST(ElseIfTypeCheckTest, OptionalNarrowingCarriesThroughChain) {
    // 앞 분기 조건의 부정이 뒤 분기로 이어진다: 'x == 없음'이 아니면 x는 정수.
    const std::string program = "함수 설명(정수? x) -> 문자:\n"
                                "    만약 x == 없음 라면:\n"
                                "        리턴 \"없음\"\n"
                                "    아니면 만약 x > 10 라면:\n"
                                "        리턴 \"큼\"\n"
                                "    아니면:\n"
                                "        정수 y = x + 1\n"
                                "        리턴 \"작음\"\n"
                                "출력(설명(없음))\n"
                                "출력(설명(11))\n"
                                "출력(설명(3))\n";
    expectSameOutputOnBoth(program, "없음\n큼\n작음\n");
}
