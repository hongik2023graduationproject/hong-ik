#include "evaluator.h"

#include "../ast/literals.h"
#include "../ast/expressions.h"

using namespace std;

vector<Object *> Evaluator::evaluate(Program *program) {
    return evalProgram(program);
}


vector<Object *> Evaluator::evalProgram(const Program *program) {
    vector<Object *> objects;

    objects.reserve(program->statements.size());
    for (const auto statement: program->statements) {
        objects.push_back(eval(statement));
    }

    return objects;
}


Object *Evaluator::eval(Node *node) {
    if (auto *expression_statement = dynamic_cast<ExpressionStatement *>(node)) {
        return eval(expression_statement->expression);
    }
    if (auto *integer_literal = dynamic_cast<IntegerLiteral *>(node)) {
        auto *integer = new Integer(integer_literal->value);
        return integer;
    }
    if (auto* prefix_expression = dynamic_cast<PrefixExpression*>(node)) {
        Object* right = eval(prefix_expression->right);
        return evalPrefixExpression(prefix_expression->token, right);
    }
    if (auto *infix_expression = dynamic_cast<InfixExpression *>(node)) {
        Object *left = eval(infix_expression->left);
        Object *right = eval(infix_expression->right);
        return evalInfixExpression(infix_expression->token, left, right);
    }
}


Object *Evaluator::evalInfixExpression(Token *token, Object *left, Object *right) {
    if (left->type == ObjectType::INTEGER && right->type == ObjectType::INTEGER) {
        return evalIntegerInfixExpression(token, left, right);
    }
}

Object *Evaluator::evalIntegerInfixExpression(Token *token, Object *left, Object *right) {
    auto *left_integer = dynamic_cast<Integer *>(left);
    auto *right_integer = dynamic_cast<Integer *>(right);

    long long left_value = left_integer->value;
    long long right_value = right_integer->value;

    if (token->type == TokenType::PLUS) {
        return new Integer(left_value + right_value);
    }
    if (token->type == TokenType::MINUS) {
        return new Integer(left_value - right_value);
    }
    if (token->type == TokenType::ASTERISK) {
        return new Integer(left_value * right_value);
    }
    if (token->type == TokenType::SLASH) {
        return new Integer(left_value / right_value);
    }
}

Object *Evaluator::evalPrefixExpression(Token *token, Object *right) {
    if (token->type == TokenType::MINUS) {
        return evalMinusPrefixExpression(right);
    }
}

Object *Evaluator::evalMinusPrefixExpression(Object *right) {
    if (auto integer = dynamic_cast<Integer *>(right)) {
        return new Integer(-integer->value);
    }
}