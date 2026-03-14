#include "ast/literals.h"
#include "ast/program.h"
#include "ast/statements.h"
#include "ast/expressions.h"
#include "parser/parser.h"
#include <gtest/gtest.h>
#include <memory>

using namespace std;

class ParserTest : public ::testing::Test {
protected:
    Parser* parser = new Parser();

    ~ParserTest() override {
        delete parser;
    }

    void ExpectAstEqual(const shared_ptr<Program>& actual, const shared_ptr<Program>& expected) {
        EXPECT_EQ(actual->String(), expected->String());
    }
};


TEST_F(ParserTest, OperatorTest) {
    auto expected = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "4", 1}), 4),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "11", 1}), 11))),
    });

    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::INTEGER, "4", 1}),
        make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "11", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);

    ExpectAstEqual(actual, expected);
}

TEST_F(ParserTest, ComparisonOperatorTest) {
    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::INTEGER, "1", 1}),
        make_shared<Token>(Token{TokenType::LESS_THAN, "<", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "2", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);
    ASSERT_EQ(actual->statements.size(), 1);
    EXPECT_EQ(actual->String(), "(1 < 2)\n");
}

TEST_F(ParserTest, PrecedenceTest) {
    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::INTEGER, "1", 1}),
        make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "2", 1}),
        make_shared<Token>(Token{TokenType::ASTERISK, "*", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "3", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);
    EXPECT_EQ(actual->String(), "(1 + (2 * 3))\n");
}

TEST_F(ParserTest, PrefixBangTest) {
    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::BANG, "!", 1}),
        make_shared<Token>(Token{TokenType::TRUE, "true", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);
    EXPECT_EQ(actual->String(), "(!true)\n");
}

TEST_F(ParserTest, FloatLiteralTest) {
    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::FLOAT, "3.14", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);
    EXPECT_EQ(actual->String(), "3.14\n");
}

TEST_F(ParserTest, ModuloTest) {
    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::INTEGER, "10", 1}),
        make_shared<Token>(Token{TokenType::PERCENT, "%", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "3", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);
    EXPECT_EQ(actual->String(), "(10 % 3)\n");
}

// 함수명(인자) 호출 파싱 테스트
TEST_F(ParserTest, FunctionCallTest) {
    // 출력(42)
    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::IDENTIFIER, "출력", 1}),
        make_shared<Token>(Token{TokenType::LPAREN, "(", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "42", 1}),
        make_shared<Token>(Token{TokenType::RPAREN, ")", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);
    ASSERT_EQ(actual->statements.size(), 1);
    auto* exprStmt = dynamic_cast<ExpressionStatement*>(actual->statements[0].get());
    ASSERT_NE(exprStmt, nullptr);
    auto* call = dynamic_cast<CallExpression*>(exprStmt->expression.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->arguments.size(), 1);
}

// 정수 x = 10 변수 선언 테스트
TEST_F(ParserTest, InitializationTest) {
    const vector<shared_ptr<Token>> tokens = {
        make_shared<Token>(Token{TokenType::정수, "정수", 1}),
        make_shared<Token>(Token{TokenType::IDENTIFIER, "x", 1}),
        make_shared<Token>(Token{TokenType::ASSIGN, "=", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "10", 1}),
        make_shared<Token>(Token{TokenType::NEW_LINE, "\n", 1}),
    };

    auto actual = parser->Parsing(tokens);
    ASSERT_EQ(actual->statements.size(), 1);
    auto* init = dynamic_cast<InitializationStatement*>(actual->statements[0].get());
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->name, "x");
    EXPECT_EQ(init->type->type, TokenType::정수);
}
