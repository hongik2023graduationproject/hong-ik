#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "../ast/program.h"
#include "../environment/environment.h"
#include "../object/built_in.h"
#include "../object/object.h"
#include <vector>

class Evaluator {
public:
    Evaluator();

    Object* Evaluate(Program* program);

private:
    Environment* environment;


    std::map<std::string, Builtin*> builtins = {
        {"길이", new Length},
        {"출력", new Print},
        {"추가", new Push},
    };

    Object* evalProgram(const Program* program, Environment* environment);

    Object* eval(Node* statement, Environment* environment);

    Object* evalBlockStatement(const std::vector<Statement*>& statements, Environment* environment);

    Object* evalInfixExpression(Token* token, Object* left, Object* right);

    Object* evalIntegerInfixExpression(Token* token, Object* left, Object* right);

    Object* evalBooleanInfixExpression(Token* token, Object* left, Object* right);

    Object* evalStringInfixExpression(Token* token, Object* left, Object* right);

    Object* evalPrefixExpression(Token* token, Object* right);

    Object* evalMinusPrefixExpression(Object* right);

    Object* evalBangPrefixExpression(Object* right);

    Object* evalIndexExpression(Object* left, Object* index);

    Object* evalArrayIndexExpression(Object* array, Object* index);

    Object* applyFunction(Object* function, std::vector<Object*> arguments);

    Environment* extendFunctionEnvironment(Function* function, std::vector<Object*> arguments);

    Object* unwarpReturnValue(Object* object);

    bool typeCheck(Token* type, Object* value);

    bool typeCheck(ObjectType type, Object* value);
};


#endif // EVALUATOR_H
