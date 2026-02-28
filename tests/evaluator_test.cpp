#include "ast/literals.h"
#include "ast/program.h"
#include "ast/statements.h"
#include "ast/expressions.h"
#include "parser/parser.h"
#include "evaluator/evaluator.h"
#include <gtest/gtest.h>
#include <memory>

using namespace std;

class EvaluatorTest : public ::testing::Test {
protected:
    Evaluator* evaluator_ = new Evaluator();

    ~EvaluatorTest() override {
        delete evaluator_;
    }
};


TEST_F(EvaluatorTest, OperatorTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "4", 1}), 4),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "11", 1}), 11))),
    });

    auto actual = evaluator_->Evaluate(program);

    ASSERT_NE(actual, nullptr);
    auto* integer = dynamic_cast<Integer*>(actual.get());
    ASSERT_NE(integer, nullptr);
    EXPECT_EQ(integer->value, 15);
}

TEST_F(EvaluatorTest, DivisionByZeroTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::SLASH, "/", 1}),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "10", 1}), 10),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "0", 1}), 0))),
    });

    EXPECT_THROW(evaluator_->Evaluate(program), runtime_error);
}
