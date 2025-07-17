#include "ast/literals.h"
#include "ast/program.h"
#include "ast/statements.h"
#include "ast/expressions.h"
#include "parser/parser.h"
#include <gtest/gtest.h>

using namespace std;

class ParserTest : public ::testing::Test {
protected:
    Parser* parser = new Parser();

    void ExpectAstEqual(Program* actual, Program* expected) {
        EXPECT_EQ(actual->String(), expected->String());
    }
};


TEST_F(ParserTest, OperatorTest) {
    auto expected = new Program({
        new ExpressionStatement(new InfixExpression(new Token{TokenType::PLUS, "+", 1},
            new IntegerLiteral(new Token{TokenType::INTEGER, "4", 1}, 4),
            new IntegerLiteral(new Token{TokenType::INTEGER, "11", 1}, 11))),
    });

    const vector<Token*> tokens = {
        new Token{TokenType::INTEGER, "4", 1},
        new Token{TokenType::PLUS, "+", 1},
        new Token{TokenType::INTEGER, "11", 1}
    };

    auto actual = parser->Parsing(tokens);

    ExpectAstEqual(actual, expected);
}
