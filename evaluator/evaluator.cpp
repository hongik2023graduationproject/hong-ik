#include "evaluator.h"

#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "../environment/environment.h"
#include "iostream"

using namespace std;

Evaluator::Evaluator() {
    environment = new Environment();
}

Object* Evaluator::Evaluate(Program* program) {
    return eval(program, environment);
}

// map을 사용해여 최적화 할 것
Object* Evaluator::eval(Node* node, Environment* environment) { // program
    if (auto* program = dynamic_cast<Program*>(node)) {
        return evalProgram(program, environment);
    }


    // statements
    if (auto* expression_statement = dynamic_cast<ExpressionStatement*>(node)) {
        return eval(expression_statement->expression, environment);
    }
    if (auto* initialization_statement = dynamic_cast<InitializationStatement*>(node)) {
        Object* value = eval(initialization_statement->value, environment);
        if (environment->Get(initialization_statement->name) != nullptr) {
            throw runtime_error("이미 선언된 변수명입니다.");
        }

        if (!typeCheck(initialization_statement->type, value)) {
            throw runtime_error("선언에서 자료형과 값의 타입이 일치하지 않습니다.");
        }
        environment->Set(initialization_statement->name, value);

        return value;
    }
    if (auto* assignment_statement = dynamic_cast<AssignmentStatement*>(node)) {
        Object* before = environment->Get(assignment_statement->name);
        if (before == nullptr) {
            throw runtime_error("선언되지 않은 변수명입니다.");
        }

        Object* value = eval(assignment_statement->value, environment);

        if (typeCheck(before->type, value)) {
            throw runtime_error("값의 형식이 변수의 형식과 일치하지 않습니다.");
        }

        environment->Set(assignment_statement->name, value);
        return value;
    }
    if (auto* block_statement = dynamic_cast<BlockStatement*>(node)) {
        return evalBlockStatement(block_statement->statements, environment);
    }
    if (auto* if_statement = dynamic_cast<IfStatement*>(node)) {
        auto* condition  = eval(if_statement->condition, environment);
        Boolean* boolean = dynamic_cast<Boolean*>(condition);
        if (boolean && boolean->value) {
            return eval(if_statement->consequence, environment);
        }
        return nullptr;
    }
    if (auto* function_statement = dynamic_cast<FunctionStatement*>(node)) {
        auto* function       = new Function;
        function->body       = function_statement->body;
        function->parameters = function_statement->parameters;
        function->env        = environment;
        function->type       = ObjectType::FUNCTION;

        environment->Set(function_statement->name, function);
        return function;
    }
    if (auto* return_statement = dynamic_cast<ReturnStatement*>(node)) {
        auto* returnValue  = new ReturnValue;
        Object* value      = eval(return_statement->expression, environment);
        returnValue->value = value;
        return returnValue;
    }


    // expression
    if (auto* prefix_expression = dynamic_cast<PrefixExpression*>(node)) {
        Object* right = eval(prefix_expression->right, environment);
        return evalPrefixExpression(prefix_expression->token, right);
    }
    if (auto* infix_expression = dynamic_cast<InfixExpression*>(node)) {
        Object* left  = eval(infix_expression->left, environment);
        Object* right = eval(infix_expression->right, environment);
        return evalInfixExpression(infix_expression->token, left, right);
    }
    if (auto* identifier_expression = dynamic_cast<IdentifierExpression*>(node)) {
        Object* value = environment->Get(identifier_expression->name);
        if (value != nullptr) {
            return value;
        }
        // built in function 추가 예정
        if (builtins.find(identifier_expression->name) != builtins.end()) {
            return builtins[identifier_expression->name];
        }

        // ERROR
        // throw
    }
    if (auto* call_expression = dynamic_cast<CallExpression*>(node)) {
        // 함수명이 env에 존재하는 지 확인하는 작업
        Object* function = eval(call_expression->function, environment);
        if (function == nullptr) {
            // error
        }

        // arguments를 eval
        vector<Object*> arguments;
        for (auto argument : call_expression->arguments) {
            Object* value = eval(argument, environment);
            // TODO: 타입 검사도 해야 함
            // TODO: 에러 처리 필요할 가능성 농후
            arguments.push_back(value);
        }

        return applyFunction(function, arguments);
    }
    if (auto* index_expression = dynamic_cast<IndexExpression*>(node)) {
        auto* left = eval(index_expression->name, environment);
        // ERROR 처리?
        auto* index = eval(index_expression->index, environment);
        // ERROR 처리?
        return evalIndexExpression(left, index);
    }

    if (auto* integer_literal = dynamic_cast<IntegerLiteral*>(node)) {
        auto* integer = new Integer(integer_literal->value);
        return integer;
    }
    if (auto* boolean_literal = dynamic_cast<BooleanLiteral*>(node)) {
        auto* boolean = new Boolean(boolean_literal->value);
        return boolean;
    }
    if (auto* string_literal = dynamic_cast<StringLiteral*>(node)) {
        auto* string = new String(string_literal->value);
        return string;
    }
    if (auto* array_literal = dynamic_cast<ArrayLiteral*>(node)) {
        auto* array = new Array();
        array->type = ObjectType::ARRAY;
        for (auto element : array_literal->elements) {
            Object* value = eval(element, environment);
            array->elements.push_back(value);
        }
        return array;
    }

    throw invalid_argument("알 수 없는 구문입니다.");
}

Object* Evaluator::evalProgram(const Program* program, Environment* environment) {
    Object* result = nullptr;

    for (const auto statement : program->statements) {
        result = eval(statement, environment);

        if (auto* return_value = dynamic_cast<ReturnValue*>(result)) {
            return return_value->value;
        }
    }

    return result;
}

Object* Evaluator::evalBlockStatement(std::vector<Statement*> statements, Environment* environment) {
    // nullptr를 반환할 수 있는 여지가 있음
    Object* result = nullptr;

    for (const auto statement : statements) {
        result = eval(statement, environment);

        if (const auto* return_value = dynamic_cast<ReturnValue*>(result)) {
            return result;
        }
    }

    return result;
}

Object* Evaluator::evalInfixExpression(Token* token, Object* left, Object* right) {
    if (left->type == ObjectType::INTEGER && right->type == ObjectType::INTEGER) {
        return evalIntegerInfixExpression(token, left, right);
    }
    if (left->type == ObjectType::BOOLEAN && right->type == ObjectType::BOOLEAN) {
        return evalBooleanInfixExpression(token, left, right);
    }
    if (left->type == ObjectType::STRING && right->type == ObjectType::STRING) {
        return evalStringInfixExpression(token, left, right);
    }
}

Object* Evaluator::evalIntegerInfixExpression(Token* token, Object* left, Object* right) {
    auto* left_integer  = dynamic_cast<Integer*>(left);
    auto* right_integer = dynamic_cast<Integer*>(right);

    long long left_value  = left_integer->value;
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
    if (token->type == TokenType::EQUAL) {
        return new Boolean(left_value == right_value);
    }
    if (token->type == TokenType::NOT_EQUAL) {
        return new Boolean(left_value != right_value);
    }

    // TODO: 에러 처리 (지원하지 않는 연산자)
}

Object* Evaluator::evalBooleanInfixExpression(Token* token, Object* left, Object* right) {
    auto* left_boolean  = dynamic_cast<Boolean*>(left);
    auto* right_boolean = dynamic_cast<Boolean*>(right);

    if (token->type == TokenType::LOGICAL_AND) {
        return new Boolean(left_boolean->value && right_boolean->value);
    }
    if (token->type == TokenType::LOGICAL_OR) {
        return new Boolean(left_boolean->value || right_boolean->value);
    }

    // TODO: 에러 처리 (지원하지 않는 연산자)
}

Object* Evaluator::evalStringInfixExpression(Token* token, Object* left, Object* right) {
    auto* left_string  = dynamic_cast<String*>(left);
    auto* right_string = dynamic_cast<String*>(right);

    if (token->type == TokenType::PLUS) {
        return new String(left_string->value + right_string->value);
    }
    if (token->type == TokenType::EQUAL) {
        return new Boolean(left_string->value == right_string->value);
    }
    if (token->type == TokenType::NOT_EQUAL) {
        return new Boolean(left_string->value != right_string->value);
    }

    // TODO: 에러 처리 (지원하지 않는 연산자)
}

Object* Evaluator::evalIndexExpression(Object* left, Object* index) {
    if (left->type == ObjectType::ARRAY && index->type == ObjectType::INTEGER) {
        return evalArrayIndexExpression(left, index);
    }
    // ERROR
}

Object* Evaluator::evalArrayIndexExpression(Object* array, Object* index) {
    auto* array_object = dynamic_cast<Array*>(array);
    auto* index_object = dynamic_cast<Integer*>(index);
    if (0 <= index_object->value && index_object->value < array_object->elements.size()) {
        return array_object->elements[index_object->value];
    }

    // ERROR
}

Object* Evaluator::evalPrefixExpression(Token* token, Object* right) {
    if (token->type == TokenType::MINUS) {
        return evalMinusPrefixExpression(right);
    }
    if (token->type == TokenType::BANG) {
        return evalBangPrefixExpression(right);
    }
}

Object* Evaluator::evalMinusPrefixExpression(Object* right) {
    if (auto integer = dynamic_cast<Integer*>(right)) {
        return new Integer(-integer->value);
    }
}

Object* Evaluator::evalBangPrefixExpression(Object* right) {
    if (auto boolean = dynamic_cast<Boolean*>(right)) {
        return new Boolean(!boolean->value);
    }
}

Object* Evaluator::applyFunction(Object* function, std::vector<Object*> arguments) {
    if (auto* function_object = dynamic_cast<Function*>(function)) {
        if (function_object == nullptr) {
            throw std::invalid_argument("Function is not a function");
        }

        // 함수 내부에서 사용할 env 확장하는 과정

        Environment* extended_env = extendFunctionEnvironment(function_object, arguments);

        Object* evaluated = eval(function_object->body, extended_env);

        // TODO: return type 검사를 하지 않고 있다. 또한 리턴 타입이 존재하는
        // 지도 검사해야 함
        return unwarpReturnValue(evaluated);
    }

    if (auto* builtin_object = dynamic_cast<Builtin*>(function)) {
        return builtin_object->function(arguments);
    }

    // TODO: ERROR
}

Environment* Evaluator::extendFunctionEnvironment(Function* function, std::vector<Object*> arguments) {
    environment        = new Environment();
    environment->outer = function->env;

    for (int i = 0; i < function->parameters.size(); i++) {
        // TODO: String() 함수가 아니라 실제 변수 명이 들어가도록 수정해야 함
        environment->Set(function->parameters[i]->String(), arguments[i]);
    }

    return environment;
}

// 지금 expression statement가 존재하고 있어 return value가 없더라고 마지막에
// eval되어 결과 값으로 나온 Object가 자동으로 return 하게 끔 하고 있다. 추후에
// expression statement가 삭제되면 수정할 필요가 있다
Object* Evaluator::unwarpReturnValue(Object* object) {
    if (auto* return_value = dynamic_cast<ReturnValue*>(object)) {
        return return_value->value;
    }

    return object;
}

Object* Evaluator::length(std::vector<Object*> argumenmts) {
    return new Integer(argumenmts.size());
}


// 현재 버전에서 타입 체크는 기본형에 한해서만 제공한다.
bool Evaluator::typeCheck(Token* type, Object* value) {
    if (type->type == TokenType::정수) {
        return dynamic_cast<Integer*>(value) != nullptr;
    }
    if (type->type == TokenType::문자) {
        return dynamic_cast<String*>(value) != nullptr;
    }

    // 모든 케이스가 구현되지 않았음으로 해당되지 않은 경우에 한해서는 임시로 true 리턴
    return true;
}

bool Evaluator::typeCheck(ObjectType type, Object* value) {
    if (type != value->type)
        return false;
    return true;
}
