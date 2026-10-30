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

TEST(DeclNameTokenTest, FunctionAndVariableCarryNameTokens) {
    Parser parser;
    auto program = parseWith(parser,
                             "정수 개수 = 3\n"
                             "함수 더하기(정수 a, 정수 b) -> 정수:\n"
                             "    리턴 a + b\n"
                             "\n");
    ASSERT_TRUE(parser.getErrors().empty());
    ASSERT_EQ(program->statements.size(), 2u);

    auto init = std::dynamic_pointer_cast<InitializationStatement>(program->statements[0]);
    ASSERT_NE(init, nullptr);
    ASSERT_NE(init->nameToken, nullptr);
    EXPECT_EQ(init->nameToken->text, "개수");
    EXPECT_EQ(init->nameToken->line, 1);
    EXPECT_EQ(init->nameToken->column, 3);  // 정(0)수(1)␣(2)개(3)수(4)

    auto fn = std::dynamic_pointer_cast<FunctionStatement>(program->statements[1]);
    ASSERT_NE(fn, nullptr);
    ASSERT_NE(fn->nameToken, nullptr);
    EXPECT_EQ(fn->nameToken->text, "더하기");
    ASSERT_EQ(fn->parameters.size(), 2u);
    ASSERT_NE(fn->parameters[0]->token, nullptr);
    EXPECT_EQ(fn->parameters[0]->token->text, "a");
}

TEST(DeclNameTokenTest, ClassCarriesNameAndFieldTokens) {
    Parser parser;
    auto program = parseWith(parser,
                             "클래스 점:\n"
                             "    정수 x\n"
                             "    생성(정수 값):\n"
                             "        자기.x = 값\n"
                             "\n");
    ASSERT_TRUE(parser.getErrors().empty());
    auto cls = std::dynamic_pointer_cast<ClassStatement>(program->statements[0]);
    ASSERT_NE(cls, nullptr);
    ASSERT_NE(cls->nameToken, nullptr);
    EXPECT_EQ(cls->nameToken->text, "점");
    ASSERT_EQ(cls->fieldNameTokens.size(), 1u);
    EXPECT_EQ(cls->fieldNameTokens[0]->text, "x");
}
