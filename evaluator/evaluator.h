#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <vector>

#include "../ast/program.h"
#include "../object/object.h"

class Evaluator {
public:
    std::vector<Object *> evaluate(Program *program);

private:
    std::vector<Object *> evalProgram(const Program *program);

    Object *eval(Node *statement);

    Object *evalInfixExpression(Token *token, Object *left, Object *right);

    Object *evalIntegerInfixExpression(Token *token, Object *left, Object *right);

    Object *evalPrefixExpression(Token *token, Object *right);

    Object *evalMinusPrefixExpression(Object *right);
};


#endif //EVALUATOR_H
