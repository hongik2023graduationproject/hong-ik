#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <vector>

#include "../ast/program.h"
#include "../environment/environment.h"
#include "../object/built_in.h"
#include "../object/object.h"

class Evaluator {
public:
    Evaluator();

    Object *Evaluate(Program *program);

private:
    Environment *environment;


    std::map<std::string, Builtin *> builtins = {
        {"길이", new Length},
    };

    Object *evalProgram(const Program *program, Environment *environment);

    Object *eval(Node *statement, Environment *environment);

    Object *evalBlockStatement(std::vector<Statement *> statements, Environment *environment);

    Object *evalInfixExpression(Token *token, Object *left, Object *right);

    Object *evalIntegerInfixExpression(Token *token, Object *left, Object *right);

    Object *evalBooleanInfixExpression(Token *token, Object *left, Object *right);

    Object *evalStringInfixExpression(Token *token, Object *left, Object *right);

    Object *evalPrefixExpression(Token *token, Object *right);

    Object *evalMinusPrefixExpression(Object *right);

    Object *evalBangPrefixExpression(Object *right);

    Object *evalIndexExpression(Object *left, Object *index);

    Object *evalArrayIndexExpression(Object *array, Object *index);

    Object *applyFunction(Object *function, std::vector<Object *> arguments);

    Environment *extendFunctionEnvironment(Function *function, std::vector<Object *> arguments);

    Object *unwarpReturnValue(Object *object);

    // builtin function
    Object *length(std::vector<Object *> arguments);
};


#endif //EVALUATOR_H
