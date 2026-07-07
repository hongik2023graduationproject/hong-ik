#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>

namespace {
std::shared_ptr<Program> parseWith(Parser& parser, const std::string& src) {
    Lexer lexer;
    return parser.Parsing(lexer.Tokenize(Utf8Converter::Convert(src)));
}
} // namespace

TEST(ParserDiagnosticsTest, StructuredPositionsForMultipleErrors) {
    Parser parser;
    // 1행: '=' 뒤 표현식 없음(개행에서 에러), 3행: 동일 에러 — 2행은 정상
    parseWith(parser, "정수 x = ;\n정수 y = 2\n실수 z = ;\n");
    const auto& diags = parser.getDiagnostics();
    ASSERT_EQ(diags.size(), 2u);
    EXPECT_EQ(diags[0].line, 1);
    EXPECT_EQ(diags[1].line, 3);
    // "정수 x = ;" 코드포인트: 정(0)수(1) ␣(2) x(3) ␣(4) =(5) ␣(6) ;(7) → ';' 토큰 위치
    EXPECT_EQ(diags[0].column, 7);
    EXPECT_EQ(diags[0].endColumn, 8);
    EXPECT_FALSE(diags[0].message.empty());
    // 기존 문자열 인터페이스와 개수 일치 (하위호환)
    EXPECT_EQ(parser.getErrors().size(), diags.size());
    EXPECT_EQ(parser.getErrors()[0], diags[0].message);
}

TEST(ParserDiagnosticsTest, DiagnosticsClearedBetweenParses) {
    Parser parser;
    parseWith(parser, "정수 x = ;\n");
    ASSERT_FALSE(parser.getDiagnostics().empty());
    parseWith(parser, "정수 x = 1\n");
    EXPECT_TRUE(parser.getDiagnostics().empty());
}
