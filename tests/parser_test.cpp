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
