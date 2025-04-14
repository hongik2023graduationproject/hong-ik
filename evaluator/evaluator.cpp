#include "evaluator.h"

#include "../ast/literals.h"
#include "../ast/expressions.h"
#include "../environment/environment.h"

using namespace std;

Evaluator::Evaluator() {
    environment = new Environment();
};

vector<Object *> Evaluator::evaluate(Program *program) {
    return evalProgram(program, environment);
}


vector<Object *> Evaluator::evalProgram(const Program *program, Environment *environment) {
    vector<Object *> objects;

    objects.reserve(program->statements.size());
    for (const auto statement: program->statements) {
        Object* result = eval(statement, environment);
        if (result != nullptr)
            objects.push_back(result);
    }

    return objects;
}

// eval 하는 순서는 statement, expression, literal 순으로 배치
// 이는 많이 call 되는 순서에 따라 배치하는 것이 효율적으로 작동할 것이지만 개발의 편의성을 위한 것
// 추후에 call 되는 순서에 따라 배치하거나, map을 사용해여 최적화 할 것
Object *Evaluator::eval(Node *node, Environment *environment) {
    if (auto *expression_statement = dynamic_cast<ExpressionStatement *>(node)) {
        return eval(expression_statement->expression, environment);
    }
    if (auto *assignment_statement = dynamic_cast<AssignmentStatement *>(node)) {
        Object *value = eval(assignment_statement->value, environment);
        environment->Set(assignment_statement->name, value);
        return value;
    }
    if (auto *if_statement = dynamic_cast<IfStatement *>(node)) {
        auto *condition = eval(if_statement->condition, environment);
        Boolean *boolean = dynamic_cast<Boolean *>(condition);
        if (boolean && boolean->value) {
            return eval(if_statement->consequence, environment);
        }
        return nullptr;
    }
    if (auto *block_statement = dynamic_cast<BlockStatement *>(node)) {
        return evalBlockStatements(block_statement->statements, environment);
    }

    if (auto *prefix_expression = dynamic_cast<PrefixExpression *>(node)) {
        Object *right = eval(prefix_expression->right, environment);
        return evalPrefixExpression(prefix_expression->token, right);
    }
    if (auto *infix_expression = dynamic_cast<InfixExpression *>(node)) {
        Object *left = eval(infix_expression->left, environment);
        Object *right = eval(infix_expression->right, environment);
        return evalInfixExpression(infix_expression->token, left, right);
    }
    if (auto *identifier_expression = dynamic_cast<IdentifierExpression *>(node)) {
        Object* value = environment->Get(identifier_expression->name);
        return value;
    }
    if (auto *integer_literal = dynamic_cast<IntegerLiteral *>(node)) {
        auto *integer = new Integer(integer_literal->value);
        return integer;
    }
    if (auto *boolean_literal = dynamic_cast<BooleanLiteral *>(node)) {
        auto *boolean = new Boolean(boolean_literal->value);
        return boolean;
    }
}

Object *Evaluator::evalBlockStatements(std::vector<Statement *> statements, Environment *environment) {
    Object *result = nullptr;

    for (auto statement: statements) {
        result = eval(statement, environment);

        if (result == nullptr || dynamic_cast<ReturnValue *>(result)) {
            return result;
        }
    }

    return result;
}





Object *Evaluator::evalInfixExpression(Token *token, Object *left, Object *right) {
    if (left->type == ObjectType::INTEGER && right->type == ObjectType::INTEGER) {
        return evalIntegerInfixExpression(token, left, right);
    }
    if (left->type == ObjectType::BOOLEAN && right->type == ObjectType::BOOLEAN) {
        return evalBooleanInfixExpression(token, left, right);
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

Object *Evaluator::evalBooleanInfixExpression(Token *token, Object *left, Object *right) {
    auto *left_boolean = dynamic_cast<Boolean *>(left);
    auto *right_boolean = dynamic_cast<Boolean *>(right);

    if (token->type == TokenType::LOGICAL_AND) {
        return new Boolean(left_boolean->value && right_boolean->value);
    }
    if (token->type == TokenType::LOGICAL_OR) {
        return new Boolean(left_boolean->value || right_boolean->value);
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
