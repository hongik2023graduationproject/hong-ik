#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "../ast/program.h"
#include "../environment/environment.h"
#include "../object/built_in.h"
#include "../object/object.h"
#include <memory>
#include <vector>

class Evaluator {
public:
    Evaluator();
    ~Evaluator();

    std::shared_ptr<Object> Evaluate(std::shared_ptr<Program> program);

private:
    std::shared_ptr<Environment> environment;


    std::map<std::string, std::shared_ptr<Builtin>> builtins = {
        {"길이", std::make_shared<Length>()},
        {"출력", std::make_shared<Print>()},
        {"추가", std::make_shared<Push>()},
    };

    std::shared_ptr<Object> evalProgram(const std::shared_ptr<Program>& program, Environment* environment);

    std::shared_ptr<Object> eval(Node* statement, Environment* environment);

    std::shared_ptr<Object> evalBlockStatement(const std::vector<std::shared_ptr<Statement>>& statements, Environment* environment);

    std::shared_ptr<Object> evalInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalIntegerInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalBooleanInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalStringInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalPrefixExpression(Token* token, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalMinusPrefixExpression(std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalBangPrefixExpression(std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalIndexExpression(std::shared_ptr<Object> left, std::shared_ptr<Object> index);

    std::shared_ptr<Object> evalArrayIndexExpression(std::shared_ptr<Object> array, std::shared_ptr<Object> index);

    std::shared_ptr<Object> applyFunction(std::shared_ptr<Object> function, std::vector<std::shared_ptr<Object>> arguments);

    std::shared_ptr<Environment> extendFunctionEnvironment(Function* function, std::vector<std::shared_ptr<Object>> arguments);

    std::shared_ptr<Object> unwrapReturnValue(std::shared_ptr<Object> object);

    bool typeCheck(Token* type, const std::shared_ptr<Object>& value);

    bool typeCheck(ObjectType type, const std::shared_ptr<Object>& value);
};


#endif // EVALUATOR_H
