#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <vector>

#include "../ast/program.h"
#include "../environment/environment.h"
#include "../object/object.h"

class Evaluator {
public:
    Evaluator();

    std::vector<Object *> evaluate(Program *program);

private:
    Environment *environment;

    std::vector<Object *> evalProgram(const Program *program, Environment *environment);

    Object *eval(Node *statement, Environment *environment);

    Object *evalBlockStatements(std::vector<Statement *> statements, Environment *environment);

    Object *evalInfixExpression(Token *token, Object *left, Object *right);

    Object *evalIntegerInfixExpression(Token *token, Object *left, Object *right);

    Object *evalBooleanInfixExpression(Token *token, Object *left, Object *right);

    Object *evalPrefixExpression(Token *token, Object *right);

    Object *evalMinusPrefixExpression(Object *right);
};


#endif //EVALUATOR_H
