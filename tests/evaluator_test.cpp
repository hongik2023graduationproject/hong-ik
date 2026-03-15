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

    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

TEST_F(EvaluatorTest, ComparisonTest) {
    // 3 < 5 → true
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::LESS_THAN, "<", 1}),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "3", 1}), 3),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "5", 1}), 5))),
    });

    auto actual = evaluator_->Evaluate(program);
    ASSERT_NE(actual, nullptr);
    auto* boolean = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(boolean, nullptr);
    EXPECT_TRUE(boolean->value);
}

TEST_F(EvaluatorTest, GreaterEqualTest) {
    // 5 >= 5 → true
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::GREATER_EQUAL, ">=", 1}),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "5", 1}), 5),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "5", 1}), 5))),
    });

    auto actual = evaluator_->Evaluate(program);
    auto* boolean = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(boolean, nullptr);
    EXPECT_TRUE(boolean->value);
}

TEST_F(EvaluatorTest, ModuloTest) {
    // 10 % 3 → 1
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::PERCENT, "%", 1}),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "10", 1}), 10),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "3", 1}), 3))),
    });

    auto actual = evaluator_->Evaluate(program);
    auto* integer = dynamic_cast<Integer*>(actual.get());
    ASSERT_NE(integer, nullptr);
    EXPECT_EQ(integer->value, 1);
}

TEST_F(EvaluatorTest, FloatArithmeticTest) {
    // 3.14 + 1.0 → 4.14
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
            make_shared<FloatLiteral>(make_shared<Token>(Token{TokenType::FLOAT, "3.14", 1}), 3.14),
            make_shared<FloatLiteral>(make_shared<Token>(Token{TokenType::FLOAT, "1.0", 1}), 1.0))),
    });

    auto actual = evaluator_->Evaluate(program);
    auto* float_obj = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(float_obj, nullptr);
    EXPECT_NEAR(float_obj->value, 4.14, 0.001);
}

TEST_F(EvaluatorTest, IntFloatMixedTest) {
    // 2 + 3.5 → 5.5 (정수가 실수로 승격)
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
            make_shared<IntegerLiteral>(make_shared<Token>(Token{TokenType::INTEGER, "2", 1}), 2),
            make_shared<FloatLiteral>(make_shared<Token>(Token{TokenType::FLOAT, "3.5", 1}), 3.5))),
    });

    auto actual = evaluator_->Evaluate(program);
    auto* float_obj = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(float_obj, nullptr);
    EXPECT_NEAR(float_obj->value, 5.5, 0.001);
}

TEST_F(EvaluatorTest, BangPrefixTest) {
    // !true → false
    auto prefix = make_shared<PrefixExpression>();
    prefix->token = make_shared<Token>(Token{TokenType::BANG, "!", 1});
    auto boolLit = make_shared<BooleanLiteral>();
    boolLit->token = make_shared<Token>(Token{TokenType::TRUE, "true", 1});
    boolLit->value = true;
    prefix->right = boolLit;

    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(prefix),
    });

    auto actual = evaluator_->Evaluate(program);
    auto* boolean = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(boolean, nullptr);
    EXPECT_FALSE(boolean->value);
}

TEST_F(EvaluatorTest, StringConcatTest) {
    // "hello" + " world"
    auto strLit1 = make_shared<StringLiteral>();
    strLit1->token = make_shared<Token>(Token{TokenType::STRING, "hello", 1});
    strLit1->value = "hello";
    auto strLit2 = make_shared<StringLiteral>();
    strLit2->token = make_shared<Token>(Token{TokenType::STRING, " world", 1});
    strLit2->value = " world";

    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
            strLit1, strLit2)),
    });

    auto actual = evaluator_->Evaluate(program);
    auto* str = dynamic_cast<String*>(actual.get());
    ASSERT_NE(str, nullptr);
    EXPECT_EQ(str->value, "hello world");
}

TEST_F(EvaluatorTest, BooleanEqualityTest) {
    // true == false → false
    auto boolLit1 = make_shared<BooleanLiteral>();
    boolLit1->token = make_shared<Token>(Token{TokenType::TRUE, "true", 1});
    boolLit1->value = true;
    auto boolLit2 = make_shared<BooleanLiteral>();
    boolLit2->token = make_shared<Token>(Token{TokenType::FALSE, "false", 1});
    boolLit2->value = false;

    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(make_shared<InfixExpression>(
            make_shared<Token>(Token{TokenType::EQUAL, "==", 1}),
            boolLit1, boolLit2)),
    });

    auto actual = evaluator_->Evaluate(program);
    auto* boolean = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(boolean, nullptr);
    EXPECT_FALSE(boolean->value);
}
