// 문자열 보간 "{식}" — 파서 분해(InterpolatedStringLiteral), 중괄호 이스케이프 규칙, 오류 메시지,
// VM·트리워킹 출력 일치(출력()과 같은 값 → 문자열 형식), 타입 검사기·LSP 진단.
#include "../lsp/jsonrpc.h"
#include "../lsp/server.h"
#include "analyzer/type_checker.h"
#include "ast/expressions.h"
#include "ast/literals.h"
#include "ast/statements.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "run_source_helper.h"
#include "utf8_converter/utf8_converter.h"
#include "util/token_utils.h"
#include <gtest/gtest.h>
#include <sstream>
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

    // 소스 한 줄짜리 식 문장의 식 노드
    std::shared_ptr<Expression> parseSingleExpression(const std::string& src) {
        Parser parser;
        auto program = parseSource(parser, src + "\n");
        EXPECT_TRUE(parser.getErrors().empty()) << parser.getErrors().front();
        if (program->statements.size() != 1) {
            ADD_FAILURE() << "문장 수가 1이 아님: " << program->statements.size();
            return nullptr;
        }
        auto stmt = std::dynamic_pointer_cast<ExpressionStatement>(program->statements[0]);
        return stmt ? stmt->expression : nullptr;
    }

    std::string plainValue(const std::string& src) {
        auto expr = parseSingleExpression(src);
        auto* lit = dynamic_cast<StringLiteral*>(expr.get());
        if (!lit) {
            ADD_FAILURE() << "StringLiteral이 아님: " << src;
            return "<not plain>";
        }
        return lit->value;
    }

    std::string firstParseError(const std::string& src) {
        Parser parser;
        parseSource(parser, src);
        return parser.getErrors().empty() ? std::string() : parser.getErrors().front();
    }

    TypeChecker::Result typeCheck(const std::string& src) {
        Parser parser;
        auto program = parseSource(parser, src);
        EXPECT_TRUE(parser.getErrors().empty()) << parser.getErrors().front();
        TypeChecker tc;
        return tc.check(program);
    }

} // namespace

// ───────────────────────────── 파서 ─────────────────────────────

TEST(InterpolationParserTest, SplitsLiteralsAndExpressions) {
    auto expr  = parseSingleExpression("\"안녕 {이름}님, {나이 + 1}살\"");
    auto* node = dynamic_cast<InterpolatedStringLiteral*>(expr.get());
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->literals.size(), 3u);
    ASSERT_EQ(node->expressions.size(), 2u);
    EXPECT_EQ(node->literals[0], "안녕 ");
    EXPECT_EQ(node->literals[1], "님, ");
    EXPECT_EQ(node->literals[2], "살");
    auto* id = dynamic_cast<IdentifierExpression*>(node->expressions[0].get());
    ASSERT_NE(id, nullptr);
    EXPECT_EQ(id->name, "이름");
    EXPECT_NE(dynamic_cast<InfixExpression*>(node->expressions[1].get()), nullptr);
    EXPECT_EQ(node->String(), "\"안녕 {이름}님, {(나이 + 1)}살\"");
}

TEST(InterpolationParserTest, AnyExpressionKind) {
    for (const std::string src : {"\"{숫자들[0]}\"", "\"{단 * i}\"", "\"{자기.이름}\"", "\"{학생[\\\"이름\\\"]}\"",
             "\"{함수호출()}\"", "\"{p.소개(1, 2)}\"", "\"{-x}\"", "\"{(a, b)}\"", "\"{[1, 2][0]}\"",
             "\"{f({\\\"a\\\": {\\\"b\\\": 1}})}\"", "\"{  공백  }\"",
             // 사전 리터럴로 시작하는 식은 {{ 이스케이프와 겹치지 않게 한 칸 띄운다
             "\"{ {\\\"k\\\": 1}[\\\"k\\\"] }\""}) {
        auto expr  = parseSingleExpression(src);
        auto* node = dynamic_cast<InterpolatedStringLiteral*>(expr.get());
        ASSERT_NE(node, nullptr) << src;
        EXPECT_EQ(node->expressions.size(), 1u) << src;
        EXPECT_EQ(node->literals.size(), 2u) << src;
    }
}

TEST(InterpolationParserTest, StringWithoutInterpolationStaysPlain) {
    EXPECT_EQ(plainValue("\"그냥 문자열\""), "그냥 문자열");
    EXPECT_EQ(plainValue("\"\""), "");
}

TEST(InterpolationParserTest, DoubledBracesAreLiteralBraces) {
    EXPECT_EQ(plainValue("\"{{x}}\""), "{x}");
    EXPECT_EQ(plainValue("\"{{\\\"이름\\\": 1}}\""), "{\"이름\": 1}");
    EXPECT_EQ(plainValue("\"a{{b\""), "a{b");
    EXPECT_EQ(plainValue("\"a}}b\""), "a}b");
}

TEST(InterpolationParserTest, BackslashBracesAreLiteralBraces) {
    // 렉서가 \{ \} 를 {{ }} 로 바꿔 넘긴다
    EXPECT_EQ(plainValue("\"\\{\\\"이름\\\": 1\\}\""), "{\"이름\": 1}");
    EXPECT_EQ(plainValue("\"\\{x\\}\""), "{x}");
}

TEST(InterpolationParserTest, EscapedBackslashBeforeBraceStillInterpolates) {
    // "\\{x}" = 백슬래시 글자 + {x} 보간 — \{ 이스케이프와 구별된다
    auto expr  = parseSingleExpression("\"\\\\{x}\"");
    auto* node = dynamic_cast<InterpolatedStringLiteral*>(expr.get());
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->literals[0], "\\");
}

TEST(InterpolationParserTest, BackwardCompatibleLiteralBraces) {
    EXPECT_EQ(plainValue("\"{\""), "{"); // 짝 없는 {
    EXPECT_EQ(plainValue("\"\\\"a\\\": 1}\""), "\"a\": 1}"); // 짝 없는 }
    EXPECT_EQ(plainValue("\"빈 {} 사전\""), "빈 {} 사전"); // 빈 {}
    EXPECT_EQ(plainValue("\"{  }\""), "{  }");
    EXPECT_EQ(plainValue("\"a { b\""), "a { b");
}

TEST(InterpolationParserTest, BracesInsideNestedStringDoNotCloseInterpolation) {
    auto expr  = parseSingleExpression("\"<{\\\"}\\\" + x}>\"");
    auto* node = dynamic_cast<InterpolatedStringLiteral*>(expr.get());
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->expressions.size(), 1u);
    EXPECT_EQ(node->literals[0], "<");
    EXPECT_EQ(node->literals[1], ">");
    auto* infix = dynamic_cast<InfixExpression*>(node->expressions[0].get());
    ASSERT_NE(infix, nullptr);
    auto* left = dynamic_cast<StringLiteral*>(infix->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->value, "}");
}

TEST(InterpolationParserTest, NestedInterpolationInsideInnerString) {
    auto expr  = parseSingleExpression("\"바깥 {\\\"안쪽 {x}\\\"}\"");
    auto* node = dynamic_cast<InterpolatedStringLiteral*>(expr.get());
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->expressions.size(), 1u);
    EXPECT_NE(dynamic_cast<InterpolatedStringLiteral*>(node->expressions[0].get()), nullptr);
}

TEST(InterpolationParserTest, InnerNodesCarryStringLine) {
    Parser parser;
    auto program = parseSource(parser, "정수 a = 1\n\n출력(\"{a + 1}\")\n");
    ASSERT_TRUE(parser.getErrors().empty());
    auto stmt = std::dynamic_pointer_cast<ExpressionStatement>(program->statements.back());
    ASSERT_NE(stmt, nullptr);
    auto* call = dynamic_cast<CallExpression*>(stmt->expression.get());
    ASSERT_NE(call, nullptr);
    auto* node = dynamic_cast<InterpolatedStringLiteral*>(call->arguments[0].get());
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->line, 3);
    EXPECT_EQ(node->expressions[0]->line, 3);
}

TEST(InterpolationParserTest, MalformedExpressionIsKoreanParseErrorWithLine) {
    auto err = firstParseError("정수 a = 1\n\n출력(\"값: {a +}\")\n");
    EXPECT_NE(err.find("[줄 3] 문자열 보간 {a +} 안의 식을 해석할 수 없습니다"), std::string::npos) << err;
    EXPECT_NE(err.find("{{ 와 }}"), std::string::npos) << err;
}

TEST(InterpolationParserTest, TrailingTokensAreRejected) {
    auto err = firstParseError("출력(\"{a b}\")\n");
    EXPECT_NE(
        err.find("문자열 보간 {a b} 안의 식을 해석할 수 없습니다: 식 뒤에 예상하지 못한 토큰 'b'"), std::string::npos)
        << err;
}

TEST(InterpolationParserTest, JsonLikeTextWithoutEscapeExplainsBraceRule) {
    auto err = firstParseError("출력(\"{\\\"이름\\\": 1}\")\n");
    EXPECT_NE(err.find("문자열 보간 {\"이름\": 1} 안의 식을 해석할 수 없습니다"), std::string::npos) << err;
    EXPECT_NE(err.find("\\{ 와 \\}"), std::string::npos) << err;
}

TEST(InterpolationParserTest, LexerErrorInsideInterpolationIsWrapped) {
    auto err = firstParseError("출력(\"{a @ b}\")\n");
    EXPECT_NE(err.find("문자열 보간 {a @ b} 안의 식을 해석할 수 없습니다: 알 수 없는 문자 '@'"), std::string::npos)
        << err;
}

TEST(InterpolationParserTest, NestedErrorHintAppearsOnce) {
    auto err      = firstParseError("출력(\"{\\\"{1 +}\\\"}\")\n");
    const auto at = err.find("{{ 와 }}");
    ASSERT_NE(at, std::string::npos) << err;
    EXPECT_EQ(err.find("{{ 와 }}", at + 1), std::string::npos) << err;
}

TEST(InterpolationParserTest, DiagnosticPointsAtStringToken) {
    Parser parser;
    parseSource(parser, "출력(\"{1 +}\")\n");
    ASSERT_EQ(parser.getDiagnostics().size(), 1u);
    const auto& d = parser.getDiagnostics()[0];
    EXPECT_EQ(d.line, 1);
    EXPECT_EQ(d.column, 3); // 출(0)력(1)((2)"(3)
    EXPECT_GT(d.endColumn, d.column);
}

TEST(InterpolationParserTest, ExpressionDepthLimitCountsInterpolatedPart) {
    // 바깥 괄호 2000 + 보간 안 괄호 2000 → 합쳐 한도(3000) 초과
    const std::string inner = std::string(2000, '(') + "1" + std::string(2000, ')');
    const std::string src   = std::string(2000, '(') + "\"{" + inner + "}\"" + std::string(2000, ')') + "\n";
    auto err                = firstParseError(src);
    EXPECT_NE(err.find("식이 너무 깊게 중첩되었습니다"), std::string::npos) << err.substr(0, 200);
}

// ───────────────────────────── 실행 (VM·트리워킹 일치) ─────────────────────────────

TEST(InterpolationRunTest, BareIdentifierBackwardCompatible) {
    expectSameOutputOnBoth(
        "문자 이름 = \"홍길동\"\n정수 나이 = 25\n출력(\"{이름}님은 {나이}살입니다\")\n", "홍길동님은 25살입니다\n");
}

TEST(InterpolationRunTest, IndexingArithmeticMemberAndCalls) {
    const std::string program = "정수 단 = 3\n"
                                "배열 숫자들 = [10, 20, 30]\n"
                                "사전 학생 = {\"이름\": \"철수\", \"점수\": 90}\n"
                                "클래스 사람:\n"
                                "    문자 이름\n"
                                "    생성(문자 n):\n"
                                "        자기.이름 = n\n"
                                "    함수 소개() -> 문자:\n"
                                "        리턴 \"저는 {자기.이름}입니다\"\n"
                                "함수 두배(정수 x) -> 정수:\n"
                                "    리턴 x * 2\n"
                                "사람 p = 사람(\"영희\")\n"
                                "각각 정수 i [1, 2, 3] 에서:\n"
                                "    출력(\"{단} x {i} = {단 * i}\")\n"
                                "출력(\"첫째 {숫자들[0]}, 마지막 {숫자들[길이(숫자들) - 1]}\")\n"
                                "출력(\"{학생[\\\"이름\\\"]}: {학생[\\\"점수\\\"] + 5}점\")\n"
                                "출력(\"{p.이름} / {p.소개()} / {두배(21)} / {두배(두배(1))}\")\n";
    expectSameOutputOnBoth(program, "3 x 1 = 3\n3 x 2 = 6\n3 x 3 = 9\n"
                                    "첫째 10, 마지막 30\n"
                                    "철수: 95점\n"
                                    "영희 / 저는 영희입니다 / 42 / 4\n");
}

TEST(InterpolationRunTest, FormattingMatchesPrint) {
    // 값 → 문자열 변환은 출력()과 같다
    const std::string values[] = {"1.0", "-0.0", "0.1 + 0.2", "[\\\"a\\\", 1.5, 없음, true]",
        "({\\\"k\\\": \\\"v\\\"})", "(\\\"a\\\", 1)", "없음", "1 > 2", "\\\"글자\\\"", "7 / 2", "7.0 / 2"};
    for (const auto& v : values) {
        std::string unescaped;
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i] == '\\' && i + 1 < v.size()) {
                unescaped += v[++i];
            } else {
                unescaped += v[i];
            }
        }
        const std::string program = "출력(" + unescaped + ")\n출력(\"{" + v + "}\")\n";
        for (bool vm : {true, false}) {
            auto r     = runSourceFile(program, vm);
            auto split = r.output.find('\n');
            ASSERT_NE(split, std::string::npos) << r.output;
            EXPECT_EQ(r.output.substr(0, split + 1), r.output.substr(split + 1))
                << (vm ? "VM: " : "트리워킹: ") << program;
        }
    }
}

TEST(InterpolationRunTest, LiteralBraceEscapes) {
    const std::string program = "정수 n = 3\n"
                                "출력(\"{{\\\"이름\\\": {n}}}\")\n"
                                "출력(\"\\{\\\"이름\\\": {n}\\}\")\n"
                                "출력(\"{{n}} = {n}\")\n"
                                "출력(\"\\\\{n}\")\n"
                                "출력(\"빈 {} / 짝 없는 {\")\n"
                                "출력(\"닫기 } 하나\")\n"
                                "출력(\"{\" + \"\\\"a\\\": 1}\")\n";
    expectSameOutputOnBoth(program, "{\"이름\": 3}\n"
                                    "{\"이름\": 3}\n"
                                    "{n} = 3\n"
                                    "\\3\n"
                                    "빈 {} / 짝 없는 {\n"
                                    "닫기 } 하나\n"
                                    "{\"a\": 1}\n");
}

TEST(InterpolationRunTest, JsonParseOfEscapedLiteral) {
    expectSameOutputOnBoth("사전 d = JSON_파싱(\"\\{\\\"이름\\\": \\\"철수\\\"\\}\")\n출력(d[\"이름\"])\n", "철수\n");
}

TEST(InterpolationRunTest, NestedStringsAndNestedInterpolation) {
    const std::string program = "정수 x = 7\n"
                                "출력(\"바깥 {\\\"안쪽 {x}\\\" + \\\"!\\\"}\")\n"
                                "출력(\"<{\\\"}\\\" + \\\"{{\\\"}>\")\n"
                                "출력(\"{길이(\\\"가나다\\\")}글자\")\n";
    expectSameOutputOnBoth(program, "바깥 안쪽 7!\n<}{>\n3글자\n");
}

TEST(InterpolationRunTest, EvaluatedLeftToRightEveryTime) {
    const std::string program = "정수 횟수 = 0\n"
                                "함수 다음() -> 정수:\n"
                                "    횟수 += 1\n"
                                "    리턴 횟수\n"
                                "반복 정수 i = 1 부터 3 까지:\n"
                                "    출력(\"{다음()}-{다음()}\")\n";
    expectSameOutputOnBoth(program, "1-2\n3-4\n");
}

TEST(InterpolationRunTest, LocalsAndUpvaluesInsideFunctions) {
    const std::string program = "문자 접두 = \"전역\"\n"
                                "함수 꾸미기(문자 s, 정수 n) -> 문자:\n"
                                "    정수 두배 = n * 2\n"
                                "    만약 n > 0 라면:\n"
                                "        문자 표시 = \"+\"\n"
                                "        리턴 \"{접두}:{표시}{s}{두배}\"\n"
                                "    리턴 \"{접두}:{s}{두배}\"\n"
                                "출력(꾸미기(\"a\", 2))\n"
                                "출력(꾸미기(\"b\", 0))\n";
    expectSameOutputOnBoth(program, "전역:+a4\n전역:b0\n");
}

TEST(InterpolationRunTest, ConstantFoldedConcatOfBraceFragments) {
    expectSameOutputOnBoth("출력(\"a{\" + \"}b\")\n출력(\"{{\" + \"x\" + \"}}\")\n", "a{}b\n{x}\n");
}

TEST(InterpolationRunTest, UnknownIdentifierIsRuntimeErrorOnBoth) {
    for (bool vm : {true, false}) {
        auto r = runSourceFile("정수 a = 1\n출력(\"값: {없는것}\")\n", vm, TypeCheckMode::Off);
        EXPECT_NE(r.output.find("'없는것' 존재하지 않는 식별자입니다."), std::string::npos)
            << (vm ? "VM: " : "트리워킹: ") << r.output;
    }
}

TEST(InterpolationRunTest, RuntimeErrorInsideInterpolationIsCatchable) {
    const std::string program = "배열 a = [1]\n"
                                "시도:\n"
                                "    출력(\"{a[5]}\")\n"
                                "실패 오류:\n"
                                "    출력(\"잡힘\")\n";
    expectSameOutputOnBoth(program, "잡힘\n");
}

// ───────────────────────────── 타입 검사기 ─────────────────────────────

TEST(InterpolationTypeCheckTest, NoFalseWarningsForTypicalUse) {
    auto result = typeCheck("정수 단 = 3\n"
                            "배열 숫자들 = [1, 2]\n"
                            "사전 학생 = {\"이름\": \"철수\"}\n"
                            "문자 a = \"{단 * 2}단 {숫자들[0]} {학생[\\\"이름\\\"]} {길이(숫자들)} {{리터럴}}\"\n"
                            "문자 b = \"{a}\" + \"!\"\n");
    EXPECT_TRUE(result.diagnostics.empty()) << result.diagnostics[0].message;
}

TEST(InterpolationTypeCheckTest, InterpolatedStringIsString) {
    auto result = typeCheck("정수 n = \"{1 + 2}\"\n");
    ASSERT_FALSE(result.diagnostics.empty());
}

TEST(InterpolationTypeCheckTest, UnknownIdentifierInsideInterpolationIsReported) {
    auto result = typeCheck("정수 a = 1\n문자 s = \"{a} {없는것}\"\n");
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "TC006");
    EXPECT_EQ(result.diagnostics[0].line, 2);
}

// ───────────────────────────── LSP ─────────────────────────────

TEST(InterpolationLspTest, ParseErrorInsideInterpolationIsPublishedAtString) {
    std::ostringstream out;
    lsp::Dispatcher d;
    lsp::Server server{out};
    server.bind(d);
    d.handle({{"jsonrpc", "2.0"}, {"method", "textDocument/didOpen"},
                 {"params", {{"textDocument", {{"uri", "file:///s.hik"}, {"languageId", "hongik"}, {"version", 1},
                                                  {"text", "정수 a = 1\n출력(\"{a +}\")\n"}}}}}},
        out);
    std::istringstream in(out.str());
    auto body = lsp::readMessage(in);
    ASSERT_TRUE(body.has_value());
    auto msg   = nlohmann::json::parse(*body);
    auto diags = msg["params"]["diagnostics"];
    ASSERT_FALSE(diags.empty());
    EXPECT_EQ(diags[0]["severity"], 1);
    EXPECT_EQ(diags[0]["range"]["start"]["line"], 1);
    EXPECT_EQ(diags[0]["range"]["start"]["character"], 3);
    EXPECT_NE(diags[0]["message"].get<std::string>().find("문자열 보간"), std::string::npos);
}
