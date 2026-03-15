#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "../ast/program.h"
#include "../environment/environment.h"
#include "../object/built_in.h"
#include "../object/object.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

class Evaluator {
public:
    Evaluator();
    ~Evaluator();

    std::shared_ptr<Object> Evaluate(std::shared_ptr<Program> program);

private:
    std::shared_ptr<Environment> environment;


    std::map<std::string, std::shared_ptr<Builtin>> builtins;

    std::shared_ptr<Object> evalProgram(const std::shared_ptr<Program>& program, Environment* environment);

    std::shared_ptr<Object> eval(Node* statement, Environment* environment);

    std::shared_ptr<Object> evalBlockStatement(const std::vector<std::shared_ptr<Statement>>& statements, Environment* environment);

    std::shared_ptr<Object> evalInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalIntegerInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalFloatInfixExpression(Token* token, double left_val, double right_val);

    std::shared_ptr<Object> evalBooleanInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalStringInfixExpression(Token* token, std::shared_ptr<Object> left, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalPrefixExpression(Token* token, std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalMinusPrefixExpression(std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalBangPrefixExpression(std::shared_ptr<Object> right);

    std::shared_ptr<Object> evalIndexExpression(std::shared_ptr<Object> left, std::shared_ptr<Object> index);

    std::shared_ptr<Object> evalArrayIndexExpression(std::shared_ptr<Object> array, std::shared_ptr<Object> index);

    std::shared_ptr<Object> evalStringIndexExpression(std::shared_ptr<Object> str, std::shared_ptr<Object> index);

    std::shared_ptr<Object> evalHashMapIndexExpression(std::shared_ptr<Object> hashmap, std::shared_ptr<Object> key);

    std::shared_ptr<Object> applyFunction(std::shared_ptr<Object> function, std::vector<std::shared_ptr<Object>> arguments);

    std::shared_ptr<Environment> extendFunctionEnvironment(Function* function, std::vector<std::shared_ptr<Object>> arguments);

    std::shared_ptr<Object> unwrapReturnValue(std::shared_ptr<Object> object);

    bool typeCheck(Token* type, const std::shared_ptr<Object>& value);

    bool typeCheck(ObjectType type, const std::shared_ptr<Object>& value);

    TokenType compoundToArithmeticOp(TokenType compoundOp);

    // 현재 실행 중인 줄 번호 (에러 메시지용)
    long long current_line = 0;

    // 노드에서 줄 번호를 추출
    long long getLineFromNode(Node* node);

    std::set<std::string> importedFiles;

    std::shared_ptr<Object> evalImport(const std::string& filename, Environment* environment);

    // 문자열 보간
    std::string interpolateString(const std::string& str, Environment* environment);

    // 클래스 관련
    std::shared_ptr<Object> instantiateClass(ClassDef* classDef, std::vector<std::shared_ptr<Object>> arguments, Environment* environment);

    std::shared_ptr<Object> evalMemberAccess(std::shared_ptr<Object> obj, const std::string& member);

    std::shared_ptr<Object> evalMethodCall(std::shared_ptr<Object> obj, const std::string& method,
                                            std::vector<std::shared_ptr<Object>> arguments, Environment* environment);
};


#endif // EVALUATOR_H
