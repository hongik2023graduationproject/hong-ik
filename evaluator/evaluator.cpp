#include "evaluator.h"

#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "../environment/environment.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

using namespace std;

Evaluator::Evaluator() {
    environment = make_shared<Environment>();
    builtins = {
        {"길이", make_shared<Length>()},
        {"출력", make_shared<Print>()},
        {"추가", make_shared<Push>()},
        {"타입", make_shared<TypeOf>()},
        {"정수변환", make_shared<ToInteger>()},
        {"실수변환", make_shared<ToFloat>()},
        {"문자변환", make_shared<ToString_>()},
        {"입력", make_shared<Input>()},
        {"키목록", make_shared<Keys>()},
        {"포함", make_shared<Contains>()},
        {"설정", make_shared<MapSet>()},
        {"삭제", make_shared<Remove>()},
        {"파일읽기", make_shared<FileRead>()},
        {"파일쓰기", make_shared<FileWrite>()},
    };
}

Evaluator::~Evaluator() {
    // 순환 참조 해제: env→store→Function→env 사이클을 끊는다
    environment->store.clear();
}

shared_ptr<Object> Evaluator::Evaluate(shared_ptr<Program> program) {
    return eval(program.get(), environment.get());
}

TokenType Evaluator::compoundToArithmeticOp(TokenType compoundOp) {
    switch (compoundOp) {
    case TokenType::PLUS_ASSIGN: return TokenType::PLUS;
    case TokenType::MINUS_ASSIGN: return TokenType::MINUS;
    case TokenType::ASTERISK_ASSIGN: return TokenType::ASTERISK;
    case TokenType::SLASH_ASSIGN: return TokenType::SLASH;
    case TokenType::PERCENT_ASSIGN: return TokenType::PERCENT;
    default: throw runtime_error("알 수 없는 복합 대입 연산자입니다.");
    }
}

shared_ptr<Object> Evaluator::eval(Node* node, Environment* environment) { // program
    if (auto* program = dynamic_cast<Program*>(node)) {
        return evalProgram(shared_ptr<Program>(shared_ptr<Program>{}, program), environment);
    }


    // statements
    if (auto* import_statement = dynamic_cast<ImportStatement*>(node)) {
        return evalImport(import_statement->filename, environment);
    }
    if (auto* expression_statement = dynamic_cast<ExpressionStatement*>(node)) {
        return eval(expression_statement->expression.get(), environment);
    }
    if (auto* initialization_statement = dynamic_cast<InitializationStatement*>(node)) {
        auto value = eval(initialization_statement->value.get(), environment);
        if (environment->Get(initialization_statement->name) != nullptr) {
            throw runtime_error("이미 선언된 변수명입니다.");
        }

        if (!typeCheck(initialization_statement->type.get(), value)) {
            throw runtime_error("선언에서 자료형과 값의 타입이 일치하지 않습니다.");
        }
        environment->Set(initialization_statement->name, value);

        return value;
    }
    if (auto* assignment_statement = dynamic_cast<AssignmentStatement*>(node)) {
        auto before = environment->Get(assignment_statement->name);
        if (before == nullptr) {
            throw runtime_error("선언되지 않은 변수명입니다.");
        }

        auto value = eval(assignment_statement->value.get(), environment);

        if (!typeCheck(before->type, value)) {
            throw runtime_error("값의 형식이 변수의 형식과 일치하지 않습니다.");
        }

        environment->Update(assignment_statement->name, value);
        return value;
    }
    if (auto* compound_statement = dynamic_cast<CompoundAssignmentStatement*>(node)) {
        auto before = environment->Get(compound_statement->name);
        if (before == nullptr) {
            throw runtime_error("선언되지 않은 변수명입니다.");
        }

        auto rhs = eval(compound_statement->value.get(), environment);

        // 산술 연산자로 변환하여 계산
        TokenType arithmeticOp = compoundToArithmeticOp(compound_statement->op->type);
        Token opToken{arithmeticOp, compound_statement->op->text, compound_statement->op->line};
        auto result = evalInfixExpression(&opToken, before, rhs);

        if (!typeCheck(before->type, result)) {
            throw runtime_error("값의 형식이 변수의 형식과 일치하지 않습니다.");
        }

        environment->Update(compound_statement->name, result);
        return result;
    }
    if (auto* block_statement = dynamic_cast<BlockStatement*>(node)) {
        return evalBlockStatement(block_statement->statements, environment);
    }
    if (auto* if_statement = dynamic_cast<IfStatement*>(node)) {
        auto condition = eval(if_statement->condition.get(), environment);

        if (auto* boolean = dynamic_cast<Boolean*>(condition.get())) {
            if (boolean->value) {
                return eval(if_statement->consequence.get(), environment);
            } else if (if_statement->then != nullptr) {

                return eval(if_statement->then.get(), environment);
            }
        }
        return nullptr;
    }
    if (auto* while_statement = dynamic_cast<WhileStatement*>(node)) {
        shared_ptr<Object> result = nullptr;
        while (true) {
            auto condition = eval(while_statement->condition.get(), environment);
            auto* boolean = dynamic_cast<Boolean*>(condition.get());
            if (!boolean) {
                throw runtime_error("반복문의 조건은 참/거짓 값이어야 합니다.");
            }
            if (!boolean->value) {
                break;
            }
            // 매 반복마다 새 스코프 생성 (루프 내 변수 선언이 반복마다 초기화됨)
            auto loopEnv = make_shared<Environment>(environment->shared_from_this());
            result = eval(while_statement->body.get(), loopEnv.get());

            // break 처리
            if (dynamic_cast<BreakSignal*>(result.get())) {
                return nullptr;
            }
            // return 전파
            if (dynamic_cast<ReturnValue*>(result.get())) {
                return result;
            }
        }
        return result;
    }
    if (auto* foreach_statement = dynamic_cast<ForEachStatement*>(node)) {
        auto iterable = eval(foreach_statement->iterable.get(), environment);
        shared_ptr<Object> result = nullptr;

        if (auto* array = dynamic_cast<Array*>(iterable.get())) {
            for (auto& element : array->elements) {
                if (!typeCheck(foreach_statement->elementType.get(), element)) {
                    throw runtime_error("각각 반복문에서 원소의 타입이 일치하지 않습니다.");
                }
                auto loopEnv = make_shared<Environment>(environment->shared_from_this());
                loopEnv->Set(foreach_statement->elementName, element);
                result = eval(foreach_statement->body.get(), loopEnv.get());
                if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
                if (dynamic_cast<ReturnValue*>(result.get())) return result;
            }
        } else if (auto* str = dynamic_cast<String*>(iterable.get())) {
            for (size_t i = 0; i < str->value.size(); i++) {
                auto ch = make_shared<String>(string(1, str->value[i]));
                if (!typeCheck(foreach_statement->elementType.get(), ch)) {
                    throw runtime_error("각각 반복문에서 원소의 타입이 일치하지 않습니다.");
                }
                auto loopEnv = make_shared<Environment>(environment->shared_from_this());
                loopEnv->Set(foreach_statement->elementName, ch);
                result = eval(foreach_statement->body.get(), loopEnv.get());
                if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
                if (dynamic_cast<ReturnValue*>(result.get())) return result;
            }
        } else {
            throw runtime_error("각각 반복문은 배열 또는 문자열만 지원합니다.");
        }
        return result;
    }
    if (auto* trycatch = dynamic_cast<TryCatchStatement*>(node)) {
        try {
            auto result = eval(trycatch->tryBody.get(), environment);
            return result;
        } catch (const std::exception& e) {
            auto catchEnv = make_shared<Environment>(environment->shared_from_this());
            catchEnv->Set(trycatch->errorName, make_shared<String>(string(e.what())));
            return eval(trycatch->catchBody.get(), catchEnv.get());
        }
    }
    if (dynamic_cast<BreakStatement*>(node)) {
        return make_shared<BreakSignal>();
    }
    if (auto* return_statement = dynamic_cast<ReturnStatement*>(node)) {
        auto returnValue  = make_shared<ReturnValue>();
        auto value        = eval(return_statement->expression.get(), environment);
        returnValue->value = value;
        return returnValue;
    }
    if (auto* function_statement = dynamic_cast<FunctionStatement*>(node)) {
        auto function = make_shared<Function>();
        function->body = function_statement->body;

        function->parameterTypes = function_statement->parameterTypes;
        function->parameters     = function_statement->parameters;
        function->env            = environment->shared_from_this();
        function->returnType     = function_statement->returnType;

        environment->Set(function_statement->name, function);
        return function;
    }


    // expression
    if (auto* prefix_expression = dynamic_cast<PrefixExpression*>(node)) {
        auto right = eval(prefix_expression->right.get(), environment);
        return evalPrefixExpression(prefix_expression->token.get(), right);
    }
    if (auto* infix_expression = dynamic_cast<InfixExpression*>(node)) {
        auto left  = eval(infix_expression->left.get(), environment);
        auto right = eval(infix_expression->right.get(), environment);
        return evalInfixExpression(infix_expression->token.get(), left, right);
    }
    if (auto* identifier_expression = dynamic_cast<IdentifierExpression*>(node)) {
        auto value = environment->Get(identifier_expression->name);
        if (value != nullptr) {
            return value;
        }

        if (builtins.contains(identifier_expression->name)) {
            return builtins[identifier_expression->name];
        }

        throw runtime_error(identifier_expression->String() + " 존재하지 않는 식별자입니다.");
    }
    if (auto* call_expression = dynamic_cast<CallExpression*>(node)) {
        auto function = eval(call_expression->function.get(), environment);

        vector<shared_ptr<Object>> arguments;
        for (auto& argument : call_expression->arguments) {
            auto value = eval(argument.get(), environment);
            arguments.push_back(value);
        }

        return applyFunction(function, arguments);
    }
    if (auto* index_expression = dynamic_cast<IndexExpression*>(node)) {
        auto left  = eval(index_expression->name.get(), environment);
        auto index = eval(index_expression->index.get(), environment);
        return evalIndexExpression(left, index);
    }

    if (auto* integer_literal = dynamic_cast<IntegerLiteral*>(node)) {
        return make_shared<Integer>(integer_literal->value);
    }
    if (auto* float_literal = dynamic_cast<FloatLiteral*>(node)) {
        return make_shared<Float>(float_literal->value);
    }
    if (auto* boolean_literal = dynamic_cast<BooleanLiteral*>(node)) {
        return make_shared<Boolean>(boolean_literal->value);
    }
    if (auto* string_literal = dynamic_cast<StringLiteral*>(node)) {
        return make_shared<String>(string_literal->value);
    }
    if (auto* array_literal = dynamic_cast<ArrayLiteral*>(node)) {
        auto array = make_shared<Array>();
        for (auto& element : array_literal->elements) {
            auto value = eval(element.get(), environment);
            array->elements.push_back(value);
        }
        return array;
    }
    if (auto* hashmap_literal = dynamic_cast<HashMapLiteral*>(node)) {
        auto hashmap = make_shared<HashMap>();
        for (size_t i = 0; i < hashmap_literal->keys.size(); i++) {
            auto key = eval(hashmap_literal->keys[i].get(), environment);
            auto value = eval(hashmap_literal->values[i].get(), environment);
            auto* str_key = dynamic_cast<String*>(key.get());
            if (!str_key) {
                throw runtime_error("사전의 키는 문자열이어야 합니다.");
            }
            hashmap->pairs[str_key->value] = value;
        }
        return hashmap;
    }

    throw invalid_argument(node->String() + "알 수 없는 구문입니다.");
}

shared_ptr<Object> Evaluator::evalProgram(const shared_ptr<Program>& program, Environment* environment) {
    shared_ptr<Object> result = nullptr;

    for (const auto& statement : program->statements) {
        result = eval(statement.get(), environment);

        if (dynamic_cast<ReturnValue*>(result.get())) {
            return unwrapReturnValue(result);
        }
    }

    return result;
}

shared_ptr<Object> Evaluator::evalBlockStatement(const std::vector<std::shared_ptr<Statement>>& statements, Environment* environment) {
    shared_ptr<Object> result = nullptr;

    for (const auto& statement : statements) {
        result = eval(statement.get(), environment);

        if (result != nullptr) {
            if (dynamic_cast<ReturnValue*>(result.get()) || dynamic_cast<BreakSignal*>(result.get())) {
                return result;
            }
        }
    }

    return result;
}

shared_ptr<Object> Evaluator::evalInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    if (left->type == ObjectType::INTEGER && right->type == ObjectType::INTEGER) {
        return evalIntegerInfixExpression(token, left, right);
    }
    if (left->type == ObjectType::FLOAT && right->type == ObjectType::FLOAT) {
        auto* lf = dynamic_cast<Float*>(left.get());
        auto* rf = dynamic_cast<Float*>(right.get());
        return evalFloatInfixExpression(token, lf->value, rf->value);
    }
    // 정수-실수 혼합 연산: 정수를 실수로 승격
    if (left->type == ObjectType::INTEGER && right->type == ObjectType::FLOAT) {
        auto* li = dynamic_cast<Integer*>(left.get());
        auto* rf = dynamic_cast<Float*>(right.get());
        return evalFloatInfixExpression(token, static_cast<double>(li->value), rf->value);
    }
    if (left->type == ObjectType::FLOAT && right->type == ObjectType::INTEGER) {
        auto* lf = dynamic_cast<Float*>(left.get());
        auto* ri = dynamic_cast<Integer*>(right.get());
        return evalFloatInfixExpression(token, lf->value, static_cast<double>(ri->value));
    }
    if (left->type == ObjectType::BOOLEAN && right->type == ObjectType::BOOLEAN) {
        return evalBooleanInfixExpression(token, left, right);
    }
    if (left->type == ObjectType::STRING && right->type == ObjectType::STRING) {
        return evalStringInfixExpression(token, left, right);
    }

    throw runtime_error("좌항과 우항의 타입을 연산할 수 없습니다.");
}

shared_ptr<Object> Evaluator::evalIntegerInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    auto* left_integer  = dynamic_cast<Integer*>(left.get());
    auto* right_integer = dynamic_cast<Integer*>(right.get());

    long long left_value  = left_integer->value;
    long long right_value = right_integer->value;

    if (token->type == TokenType::PLUS) {
        return make_shared<Integer>(left_value + right_value);
    }
    if (token->type == TokenType::MINUS) {
        return make_shared<Integer>(left_value - right_value);
    }
    if (token->type == TokenType::ASTERISK) {
        return make_shared<Integer>(left_value * right_value);
    }
    if (token->type == TokenType::SLASH) {
        if (right_value == 0) {
            throw runtime_error("0으로 나눌 수 없습니다.");
        }
        return make_shared<Integer>(left_value / right_value);
    }
    if (token->type == TokenType::PERCENT) {
        if (right_value == 0) {
            throw runtime_error("0으로 나눌 수 없습니다.");
        }
        return make_shared<Integer>(left_value % right_value);
    }
    if (token->type == TokenType::EQUAL) {
        return make_shared<Boolean>(left_value == right_value);
    }
    if (token->type == TokenType::NOT_EQUAL) {
        return make_shared<Boolean>(left_value != right_value);
    }
    if (token->type == TokenType::LESS_THAN) {
        return make_shared<Boolean>(left_value < right_value);
    }
    if (token->type == TokenType::GREATER_THAN) {
        return make_shared<Boolean>(left_value > right_value);
    }
    if (token->type == TokenType::LESS_EQUAL) {
        return make_shared<Boolean>(left_value <= right_value);
    }
    if (token->type == TokenType::GREATER_EQUAL) {
        return make_shared<Boolean>(left_value >= right_value);
    }

    throw runtime_error("정수 연산을 지원하지 않는 연산자입니다.");
}

shared_ptr<Object> Evaluator::evalFloatInfixExpression(Token* token, double left_val, double right_val) {
    if (token->type == TokenType::PLUS) {
        return make_shared<Float>(left_val + right_val);
    }
    if (token->type == TokenType::MINUS) {
        return make_shared<Float>(left_val - right_val);
    }
    if (token->type == TokenType::ASTERISK) {
        return make_shared<Float>(left_val * right_val);
    }
    if (token->type == TokenType::SLASH) {
        if (right_val == 0.0) {
            throw runtime_error("0으로 나눌 수 없습니다.");
        }
        return make_shared<Float>(left_val / right_val);
    }
    if (token->type == TokenType::EQUAL) {
        return make_shared<Boolean>(left_val == right_val);
    }
    if (token->type == TokenType::NOT_EQUAL) {
        return make_shared<Boolean>(left_val != right_val);
    }
    if (token->type == TokenType::LESS_THAN) {
        return make_shared<Boolean>(left_val < right_val);
    }
    if (token->type == TokenType::GREATER_THAN) {
        return make_shared<Boolean>(left_val > right_val);
    }
    if (token->type == TokenType::LESS_EQUAL) {
        return make_shared<Boolean>(left_val <= right_val);
    }
    if (token->type == TokenType::GREATER_EQUAL) {
        return make_shared<Boolean>(left_val >= right_val);
    }

    throw runtime_error("실수 연산을 지원하지 않는 연산자입니다.");
}

shared_ptr<Object> Evaluator::evalBooleanInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    auto* left_boolean  = dynamic_cast<Boolean*>(left.get());
    auto* right_boolean = dynamic_cast<Boolean*>(right.get());

    if (token->type == TokenType::LOGICAL_AND) {
        return make_shared<Boolean>(left_boolean->value && right_boolean->value);
    }
    if (token->type == TokenType::LOGICAL_OR) {
        return make_shared<Boolean>(left_boolean->value || right_boolean->value);
    }
    if (token->type == TokenType::EQUAL) {
        return make_shared<Boolean>(left_boolean->value == right_boolean->value);
    }
    if (token->type == TokenType::NOT_EQUAL) {
        return make_shared<Boolean>(left_boolean->value != right_boolean->value);
    }

    throw runtime_error("논리 연산을 지원하지 않는 연산자입니다.");
}

shared_ptr<Object> Evaluator::evalStringInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    auto* left_string  = dynamic_cast<String*>(left.get());
    auto* right_string = dynamic_cast<String*>(right.get());

    if (token->type == TokenType::PLUS) {
        return make_shared<String>(left_string->value + right_string->value);
    }
    if (token->type == TokenType::EQUAL) {
        return make_shared<Boolean>(left_string->value == right_string->value);
    }
    if (token->type == TokenType::NOT_EQUAL) {
        return make_shared<Boolean>(left_string->value != right_string->value);
    }

    throw runtime_error("문자열 연산을 지원하지 않는 연산자입니다.");
}

shared_ptr<Object> Evaluator::evalIndexExpression(shared_ptr<Object> left, shared_ptr<Object> index) {
    if (left->type == ObjectType::ARRAY && index->type == ObjectType::INTEGER) {
        return evalArrayIndexExpression(left, index);
    }
    if (left->type == ObjectType::STRING && index->type == ObjectType::INTEGER) {
        return evalStringIndexExpression(left, index);
    }
    if (left->type == ObjectType::HASH_MAP && index->type == ObjectType::STRING) {
        return evalHashMapIndexExpression(left, index);
    }
    throw runtime_error("인덱스 연산이 지원되지 않는 형식입니다.");
}

shared_ptr<Object> Evaluator::evalArrayIndexExpression(shared_ptr<Object> array, shared_ptr<Object> index) {
    auto* array_object = dynamic_cast<Array*>(array.get());
    auto* index_object = dynamic_cast<Integer*>(index.get());
    if (0 <= index_object->value && index_object->value < static_cast<long long>(array_object->elements.size())) {
        return array_object->elements[index_object->value];
    }

    throw runtime_error("배열의 범위 밖 값이 인덱스로 입력되었습니다.");
}

shared_ptr<Object> Evaluator::evalStringIndexExpression(shared_ptr<Object> str, shared_ptr<Object> index) {
    auto* str_object = dynamic_cast<String*>(str.get());
    auto* index_object = dynamic_cast<Integer*>(index.get());

    if (index_object->value < 0 || index_object->value >= static_cast<long long>(str_object->value.size())) {
        throw runtime_error("문자열의 범위 밖 값이 인덱스로 입력되었습니다.");
    }

    return make_shared<String>(string(1, str_object->value[index_object->value]));
}

shared_ptr<Object> Evaluator::evalHashMapIndexExpression(shared_ptr<Object> hashmap, shared_ptr<Object> key) {
    auto* map_object = dynamic_cast<HashMap*>(hashmap.get());
    auto* key_object = dynamic_cast<String*>(key.get());
    auto it = map_object->pairs.find(key_object->value);
    if (it == map_object->pairs.end()) {
        throw runtime_error("사전에 키 '" + key_object->value + "'이(가) 존재하지 않습니다.");
    }
    return it->second;
}

shared_ptr<Object> Evaluator::evalPrefixExpression(Token* token, shared_ptr<Object> right) {
    if (token->type == TokenType::MINUS) {
        return evalMinusPrefixExpression(right);
    }
    if (token->type == TokenType::BANG) {
        return evalBangPrefixExpression(right);
    }

    throw runtime_error("지원하지 않는 전위 연산자입니다.");
}

shared_ptr<Object> Evaluator::evalMinusPrefixExpression(shared_ptr<Object> right) {
    if (auto integer = dynamic_cast<Integer*>(right.get())) {
        return make_shared<Integer>(-integer->value);
    }
    if (auto float_obj = dynamic_cast<Float*>(right.get())) {
        return make_shared<Float>(-float_obj->value);
    }

    throw runtime_error("'-' 전위 연산자가 지원되지 않는 타입입니다.");
}

shared_ptr<Object> Evaluator::evalBangPrefixExpression(shared_ptr<Object> right) {
    if (auto boolean = dynamic_cast<Boolean*>(right.get())) {
        return make_shared<Boolean>(!boolean->value);
    }

    throw runtime_error("'!' 전위 연산자가 지원되지 않는 타입입니다.");
}

shared_ptr<Object> Evaluator::applyFunction(shared_ptr<Object> function, std::vector<shared_ptr<Object>> arguments) {
    if (auto* function_object = dynamic_cast<Function*>(function.get())) {
        if (function_object->parameterTypes.size() != arguments.size()) {
            throw runtime_error("함수가 필요한 인자 개수와 입력된 인자 개수가 다릅니다."
                                + std::to_string(function_object->parameterTypes.size()) + ' '
                                + std::to_string(arguments.size()));
        }

        for (size_t i = 0; i < function_object->parameterTypes.size(); i++) {
            if (!typeCheck(function_object->parameterTypes[i].get(), arguments[i])) {
                throw runtime_error("함수 인자 타입 오류");
            }
        }

        // 함수 내부에서 사용할 env 확장, arguments 세팅
        auto extended_env = extendFunctionEnvironment(function_object, arguments);

        // 함수 실행
        auto evaluated = eval(function_object->body.get(), extended_env.get());

        if ((function_object->returnType == nullptr && evaluated == nullptr)
            || typeCheck(function_object->returnType.get(), evaluated)) {
            return unwrapReturnValue(evaluated);
        }
        throw runtime_error("함수 반환 타입과 실제 반환의 타입이 일치하지 않습니다.");
    }

    if (auto* builtin_object = dynamic_cast<Builtin*>(function.get())) {
        auto evaluated = builtin_object->function(arguments);
        if (builtin_object->returnType != nullptr && evaluated != nullptr) {
            if (typeCheck(builtin_object->returnType.get(), evaluated)) {
                return evaluated;
            }

            throw runtime_error("함수 반환 타입과 실제 반환의 타입이 일치하지 않습니다.");
        }
        return evaluated;
    }

    throw runtime_error("함수가 존재하지 않습니다.");
}

shared_ptr<Environment> Evaluator::extendFunctionEnvironment(Function* function, std::vector<shared_ptr<Object>> arguments) {
    auto env = make_shared<Environment>(function->env);

    for (size_t i = 0; i < function->parameters.size(); i++) {
        env->Set(function->parameters[i]->name, arguments[i]);
    }

    return env;
}

shared_ptr<Object> Evaluator::unwrapReturnValue(shared_ptr<Object> object) {
    if (auto* return_value = dynamic_cast<ReturnValue*>(object.get())) {
        return return_value->value;
    }

    return object;
}

// 현재 버전에서 타입 체크는 기본형에 한해서만 제공한다.
bool Evaluator::typeCheck(Token* type, const shared_ptr<Object>& value) {
    if (type == nullptr) return false;
    if (value == nullptr) return false;

    // ReturnValue인 경우 내부 값으로 체크
    auto* rv = dynamic_cast<ReturnValue*>(value.get());
    Object* check_target = rv ? rv->value.get() : value.get();
    if (check_target == nullptr) return false;

    if (type->type == TokenType::정수) {
        return dynamic_cast<Integer*>(check_target) != nullptr;
    }
    if (type->type == TokenType::실수) {
        return dynamic_cast<Float*>(check_target) != nullptr;
    }
    if (type->type == TokenType::문자) {
        return dynamic_cast<String*>(check_target) != nullptr;
    }
    if (type->type == TokenType::논리) {
        return dynamic_cast<Boolean*>(check_target) != nullptr;
    }
    if (type->type == TokenType::배열) {
        return dynamic_cast<Array*>(check_target) != nullptr;
    }
    if (type->type == TokenType::사전) {
        return dynamic_cast<HashMap*>(check_target) != nullptr;
    }

    return true;
}

bool Evaluator::typeCheck(ObjectType type, const shared_ptr<Object>& value) {
    if (type != value->type) {
        return false;
    }
    return true;
}

shared_ptr<Object> Evaluator::evalImport(const string& filename, Environment* environment) {
    // 순환 참조 방지
    if (importedFiles.contains(filename)) {
        return nullptr;
    }
    importedFiles.insert(filename);

    // 파일 읽기
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("파일을 열 수 없습니다: " + filename);
    }

    // 줄 단위로 읽어서 토큰화
    Lexer importLexer;
    vector<shared_ptr<Token>> allTokens;
    string line_str;
    int indent = 0;

    while (getline(file, line_str)) {
        line_str += "\n";
        auto utf8 = Utf8Converter::Convert(line_str);
        auto newTokens = importLexer.Tokenize(utf8);
        allTokens.insert(allTokens.end(), newTokens.begin(), newTokens.end());

        for (auto& token : newTokens) {
            if (token->type == TokenType::START_BLOCK) indent++;
            if (token->type == TokenType::END_BLOCK) indent--;
        }
    }

    // 남은 블록 닫기
    for (int i = 0; i < indent; i++) {
        allTokens.push_back(make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
    }

    if (allTokens.empty()) {
        return nullptr;
    }

    // 파싱
    Parser importParser;
    auto program = importParser.Parsing(allTokens);

    // 현재 환경에서 평가 (정의가 공유됨)
    shared_ptr<Object> result = nullptr;
    for (const auto& statement : program->statements) {
        result = eval(statement.get(), environment);
        if (dynamic_cast<ReturnValue*>(result.get())) {
            return unwrapReturnValue(result);
        }
    }

    return result;
}
