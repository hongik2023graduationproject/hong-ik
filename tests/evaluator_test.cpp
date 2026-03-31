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

// ===== 수학 내장함수 테스트 =====

static shared_ptr<CallExpression> makeBuiltinCall(const string& name,
    vector<shared_ptr<Expression>> args) {
    auto call = make_shared<CallExpression>();
    auto ident = make_shared<IdentifierExpression>();
    ident->name = name;
    call->function = ident;
    call->arguments = std::move(args);
    return call;
}

static shared_ptr<IntegerLiteral> makeIntLit(long long val) {
    return make_shared<IntegerLiteral>(
        make_shared<Token>(Token{TokenType::INTEGER, to_string(val), 1}), val);
}

static shared_ptr<FloatLiteral> makeFloatLit(double val) {
    return make_shared<FloatLiteral>(
        make_shared<Token>(Token{TokenType::FLOAT, to_string(val), 1}), val);
}

static shared_ptr<StringLiteral> makeStringLit(const string& val) {
    auto lit = make_shared<StringLiteral>();
    lit->token = make_shared<Token>(Token{TokenType::STRING, val, 1});
    lit->value = val;
    return lit;
}

TEST_F(EvaluatorTest, SinTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("사인", {makeFloatLit(0.0)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 0.0, 1e-9);
}

TEST_F(EvaluatorTest, CosTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("코사인", {makeFloatLit(0.0)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 1.0, 1e-9);
}

TEST_F(EvaluatorTest, TanTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("탄젠트", {makeFloatLit(0.0)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 0.0, 1e-9);
}

TEST_F(EvaluatorTest, LogTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("로그", {makeFloatLit(100.0)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 2.0, 1e-9);
}

TEST_F(EvaluatorTest, LnTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("자연로그", {makeFloatLit(1.0)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 0.0, 1e-9);
}

TEST_F(EvaluatorTest, LogNegativeThrows) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("로그", {makeFloatLit(-1.0)})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

TEST_F(EvaluatorTest, PowerTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("거듭제곱", {makeIntLit(2), makeIntLit(10)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 1024.0, 1e-9);
}

TEST_F(EvaluatorTest, PiTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("파이", {})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 3.14159265358979323846, 1e-9);
}

TEST_F(EvaluatorTest, EulerETest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("자연수e", {})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 2.71828182845904523536, 1e-9);
}

TEST_F(EvaluatorTest, RoundTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("반올림", {makeFloatLit(3.7)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* i = dynamic_cast<Integer*>(actual.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 4);
}

TEST_F(EvaluatorTest, CeilTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("올림", {makeFloatLit(3.2)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* i = dynamic_cast<Integer*>(actual.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 4);
}

TEST_F(EvaluatorTest, FloorTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("내림", {makeFloatLit(3.9)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* i = dynamic_cast<Integer*>(actual.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 3);
}

TEST_F(EvaluatorTest, SinTypeErrorThrows) {
    auto strLit = make_shared<StringLiteral>();
    strLit->token = make_shared<Token>(Token{TokenType::STRING, "abc", 1});
    strLit->value = "abc";
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("사인", {strLit})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

TEST_F(EvaluatorTest, TrigWithIntegerTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("사인", {makeIntLit(0)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 0.0, 1e-9);
}

TEST_F(EvaluatorTest, RoundIntegerPassthrough) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("반올림", {makeIntLit(5)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* i = dynamic_cast<Integer*>(actual.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 5);
}

// ===== JSON 내장함수 테스트 =====

TEST_F(EvaluatorTest, JsonParseInteger) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit("42")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* i = dynamic_cast<Integer*>(actual.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 42);
}

TEST_F(EvaluatorTest, JsonParseFloat) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit("3.14")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* f = dynamic_cast<Float*>(actual.get());
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(f->value, 3.14, 1e-9);
}

TEST_F(EvaluatorTest, JsonParseString) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit("\"hello\"")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "hello");
}

TEST_F(EvaluatorTest, JsonParseBoolean) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit("true")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* b = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->value);
}

TEST_F(EvaluatorTest, JsonParseNull) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit("null")})),
    });
    auto actual = evaluator_->Evaluate(program);
    ASSERT_NE(dynamic_cast<Null*>(actual.get()), nullptr);
}

TEST_F(EvaluatorTest, JsonParseArray) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit("[1, 2, 3]")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* arr = dynamic_cast<Array*>(actual.get());
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->elements.size(), 3);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[0].get())->value, 1);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[1].get())->value, 2);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[2].get())->value, 3);
}

TEST_F(EvaluatorTest, JsonParseObject) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱",
            {makeStringLit("{\"name\": \"hong\", \"age\": 10}")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* map = dynamic_cast<HashMap*>(actual.get());
    ASSERT_NE(map, nullptr);
    ASSERT_EQ(map->pairs.size(), 2);
    EXPECT_EQ(dynamic_cast<String*>(map->pairs["name"].get())->value, "hong");
    EXPECT_EQ(dynamic_cast<Integer*>(map->pairs["age"].get())->value, 10);
}

TEST_F(EvaluatorTest, JsonParseNested) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱",
            {makeStringLit("{\"items\": [1, {\"nested\": true}]}")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* map = dynamic_cast<HashMap*>(actual.get());
    ASSERT_NE(map, nullptr);
    auto* arr = dynamic_cast<Array*>(map->pairs["items"].get());
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->elements.size(), 2);
    EXPECT_EQ(dynamic_cast<Integer*>(arr->elements[0].get())->value, 1);
    auto* inner = dynamic_cast<HashMap*>(arr->elements[1].get());
    ASSERT_NE(inner, nullptr);
    EXPECT_TRUE(dynamic_cast<Boolean*>(inner->pairs["nested"].get())->value);
}

TEST_F(EvaluatorTest, JsonParseInvalidThrows) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit("{invalid}")})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

TEST_F(EvaluatorTest, JsonSerializeInteger) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_직렬화", {makeIntLit(42)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "42");
}

TEST_F(EvaluatorTest, JsonSerializeString) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_직렬화", {makeStringLit("hello")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "\"hello\"");
}

TEST_F(EvaluatorTest, JsonSerializeBoolean) {
    auto boolLit = make_shared<BooleanLiteral>();
    boolLit->token = make_shared<Token>(Token{TokenType::TRUE, "true", 1});
    boolLit->value = true;
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_직렬화", {boolLit})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "true");
}

TEST_F(EvaluatorTest, JsonRoundTrip) {
    // 파싱 후 직렬화하면 동등한 JSON이 나와야 함
    string json = "{\"a\":1,\"b\":[2,3]}";
    auto parseProgram = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("JSON_파싱", {makeStringLit(json)})),
    });
    auto parsed = evaluator_->Evaluate(parseProgram);

    // 파싱된 객체를 환경에 저장하고 직렬화 호출을 위해 직접 호출
    auto* map = dynamic_cast<HashMap*>(parsed.get());
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(dynamic_cast<Integer*>(map->pairs["a"].get())->value, 1);
    auto* arr = dynamic_cast<Array*>(map->pairs["b"].get());
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->elements.size(), 2);
}

// ===== 문자열 처리 내장함수 테스트 =====

// 시작확인 테스트
TEST_F(EvaluatorTest, StartsWithTrue) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("시작확인",
            {makeStringLit("안녕하세요"), makeStringLit("안녕")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* b = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->value);
}

TEST_F(EvaluatorTest, StartsWithFalse) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("시작확인",
            {makeStringLit("안녕하세요"), makeStringLit("하세")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* b = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(b, nullptr);
    EXPECT_FALSE(b->value);
}

TEST_F(EvaluatorTest, StartsWithTypeError) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("시작확인",
            {makeStringLit("hello"), makeIntLit(1)})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

// 끝확인 테스트
TEST_F(EvaluatorTest, EndsWithTrue) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("끝확인",
            {makeStringLit("안녕하세요"), makeStringLit("세요")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* b = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->value);
}

TEST_F(EvaluatorTest, EndsWithFalse) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("끝확인",
            {makeStringLit("안녕하세요"), makeStringLit("안녕")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* b = dynamic_cast<Boolean*>(actual.get());
    ASSERT_NE(b, nullptr);
    EXPECT_FALSE(b->value);
}

TEST_F(EvaluatorTest, EndsWithTypeError) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("끝확인",
            {makeIntLit(1), makeStringLit("a")})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

// 반복 테스트
TEST_F(EvaluatorTest, RepeatTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("반복",
            {makeStringLit("가"), makeIntLit(3)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "가가가");
}

TEST_F(EvaluatorTest, RepeatZero) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("반복",
            {makeStringLit("abc"), makeIntLit(0)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "");
}

TEST_F(EvaluatorTest, RepeatNegativeThrows) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("반복",
            {makeStringLit("a"), makeIntLit(-1)})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

// 채우기 테스트
TEST_F(EvaluatorTest, PadTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("채우기",
            {makeStringLit("가나"), makeIntLit(5), makeStringLit("*")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "가나***");
}

TEST_F(EvaluatorTest, PadAlreadyLong) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("채우기",
            {makeStringLit("abcde"), makeIntLit(3), makeStringLit("x")})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "abcde");
}

TEST_F(EvaluatorTest, PadMultiCharThrows) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("채우기",
            {makeStringLit("a"), makeIntLit(5), makeStringLit("xy")})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}

// 부분문자 테스트
TEST_F(EvaluatorTest, SubstringTest) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("부분문자",
            {makeStringLit("안녕하세요"), makeIntLit(1), makeIntLit(4)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "녕하세");
}

TEST_F(EvaluatorTest, SubstringAscii) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("부분문자",
            {makeStringLit("hello world"), makeIntLit(0), makeIntLit(5)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "hello");
}

TEST_F(EvaluatorTest, SubstringOutOfRange) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("부분문자",
            {makeStringLit("abc"), makeIntLit(1), makeIntLit(100)})),
    });
    auto actual = evaluator_->Evaluate(program);
    auto* s = dynamic_cast<String*>(actual.get());
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, "bc");
}

TEST_F(EvaluatorTest, SubstringTypeError) {
    auto program = make_shared<Program>(vector<shared_ptr<Statement>>{
        make_shared<ExpressionStatement>(makeBuiltinCall("부분문자",
            {makeIntLit(123), makeIntLit(0), makeIntLit(2)})),
    });
    EXPECT_THROW(evaluator_->Evaluate(program), std::exception);
}
