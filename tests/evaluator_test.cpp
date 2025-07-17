#include "ast/literals.h"
#include "ast/program.h"
#include "ast/statements.h"
#include "ast/expressions.h"
#include "parser/parser.h"
#include "evaluator/evaluator.h"
#include <gtest/gtest.h>

using namespace std;

class EvaluatorTest : public ::testing::Test {
protected:
    Evaluator* evaluator_ = new Evaluator();

    // void ExpectAstEqual(Program* actual, Program* expected) {
    //     EXPECT_EQ(actual->String(), expected->String());
    // }
};


// TEST_F(EvaluatorTest, OperatorTest) {
//     auto program = new Program({
//         new ExpressionStatement(new InfixExpression(new Token{TokenType::PLUS, "+", 1},
//             new IntegerLiteral(new Token{TokenType::INTEGER, "4", 1}, 4),
//             new IntegerLiteral(new Token{TokenType::INTEGER, "11", 1}, 11))),
//     });
//
//
//     auto actual = evaluator_->Evaluate(program);
//
// }
