#include "evaluator.h"

#include "../ast/literals.h"
#include "../ast/expressions.h"

using namespace std;

vector<Object *> Evaluator::evaluate(Program *program) {
    return evalProgram(program);
}


vector<Object *> Evaluator::evalProgram(Program *program) {
    vector<Object *> objects;

    for (auto statement: program->statements) {
        objects.push_back(eval(statement));
    }

    return objects;
}


Object *Evaluator::eval(Node *node) {
    if (ExpressionStatement *expressionStatement = dynamic_cast<ExpressionStatement *>(node)) {
        return eval(expressionStatement->expression);
    }
    if (IntegerLiteral *integerLiteral = dynamic_cast<IntegerLiteral *>(node)) {
        auto *integer = new Integer(integerLiteral->value);
        return integer;
    }
    // if (PrefixExpression* prefixExpression = dynamic_cast<PrefixExpression*>(node)) {
    //
    // }
    if (InfixExpression *infixExpression = dynamic_cast<InfixExpression *>(node)) {
        Object *left = eval(infixExpression->left);
        Object *right = eval(infixExpression->right);
        return evalInfixExpression(infixExpression->token, left, right);
    }
}


Object *Evaluator::evalInfixExpression(Token *token, Object *left, Object *right) {
    if (left->type == ObjectType::INTEGER && right->type == ObjectType::INTEGER) {
        return evalIntegerInfixExpression(token, left, right);
    }
}

Object *Evaluator::evalIntegerInfixExpression(Token *token, Object *left, Object *right) {
    Integer* left_integer = dynamic_cast<Integer*>(left);
    Integer* right_integer = dynamic_cast<Integer*>(right);

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
