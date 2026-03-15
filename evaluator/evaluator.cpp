#include "evaluator.h"

#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "../environment/environment.h"
#include "../exception/exception.h"
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
        {"절대값", make_shared<Abs>()},
        {"제곱근", make_shared<Sqrt>()},
        {"최대", make_shared<Max>()},
        {"최소", make_shared<Min>()},
        {"난수", make_shared<Random>()},
        {"분리", make_shared<Split>()},
        {"대문자", make_shared<ToUpper>()},
        {"소문자", make_shared<ToLower>()},
        {"치환", make_shared<Replace>()},
        {"자르기", make_shared<Trim>()},
        {"정렬", make_shared<Sort>()},
        {"뒤집기", make_shared<Reverse>()},
        {"찾기", make_shared<Find>()},
        {"조각", make_shared<Slice>()},
    };
}

Evaluator::~Evaluator() {
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
    default: throw RuntimeException("알 수 없는 복합 대입 연산자입니다.", current_line);
    }
}

long long Evaluator::getLineFromNode(Node* node) {
    // 토큰을 가진 Expression들에서 줄 번호 추출
    if (auto* e = dynamic_cast<InfixExpression*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<PrefixExpression*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<IntegerLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<FloatLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<BooleanLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<StringLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<NullLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<ArrayLiteral*>(node)) return e->token ? e->token->line : 0;
    // Statement들에서 토큰 기반 줄 번호 추출
    if (auto* s = dynamic_cast<InitializationStatement*>(node)) return s->type ? s->type->line : 0;
    if (auto* s = dynamic_cast<CompoundAssignmentStatement*>(node)) return s->op ? s->op->line : 0;
    if (auto* s = dynamic_cast<FunctionStatement*>(node)) return s->returnType ? s->returnType->line : 0;
    return 0;
}

string Evaluator::interpolateString(const string& str, Environment* environment) {
    string result;
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '{') {
            size_t end = str.find('}', i);
            if (end == string::npos) {
                result += str[i];
                i++;
                continue;
            }
            string varName = str.substr(i + 1, end - i - 1);
            auto value = environment->Get(varName);
            if (value != nullptr) {
                result += value->ToString();
            } else if (builtins.contains(varName)) {
                result += "내장함수:" + varName;
            } else {
                result += "{" + varName + "}";
            }
            i = end + 1;
        } else {
            result += str[i];
            i++;
        }
    }
    return result;
}

shared_ptr<Object> Evaluator::eval(Node* node, Environment* environment) {
    // 노드에서 줄 번호 추출 (0이 아닌 경우에만 갱신)
    long long nodeLine = getLineFromNode(node);
    if (nodeLine > 0) current_line = nodeLine;

    // program
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
        current_line = initialization_statement->type ? initialization_statement->type->line : current_line;
        auto value = eval(initialization_statement->value.get(), environment);
        if (environment->Get(initialization_statement->name) != nullptr) {
            throw RuntimeException("이미 선언된 변수명입니다: " + initialization_statement->name, current_line);
        }

        if (dynamic_cast<Null*>(value.get())) {
            environment->Set(initialization_statement->name, value);
            return value;
        }

        if (initialization_statement->type->type == TokenType::IDENTIFIER) {
            auto classDef = environment->Get(initialization_statement->type->text);
            if (classDef && dynamic_cast<ClassDef*>(classDef.get())) {
                auto* inst = dynamic_cast<Instance*>(value.get());
                if (!inst || inst->classDef->name != initialization_statement->type->text) {
                    throw RuntimeException("선언에서 자료형과 값의 타입이 일치하지 않습니다.", current_line);
                }
                environment->Set(initialization_statement->name, value);
                return value;
            }
        }

        if (!typeCheck(initialization_statement->type.get(), value)) {
            throw RuntimeException("선언에서 자료형과 값의 타입이 일치하지 않습니다.", current_line);
        }
        environment->Set(initialization_statement->name, value);
        return value;
    }
    if (auto* assignment_statement = dynamic_cast<AssignmentStatement*>(node)) {
        if (assignment_statement->name.substr(0, std::string("자기.").size()) == "자기.") {
            string fieldName = assignment_statement->name.substr(std::string("자기.").size());
            auto selfObj = environment->Get("자기");
            if (!selfObj) {
                throw RuntimeException("'자기'가 현재 스코프에 존재하지 않습니다.", current_line);
            }
            auto* instance = dynamic_cast<Instance*>(selfObj.get());
            if (!instance) {
                throw RuntimeException("'자기'가 인스턴스가 아닙니다.", current_line);
            }
            auto value = eval(assignment_statement->value.get(), environment);
            instance->fields->Set(fieldName, value);
            return value;
        }

        auto before = environment->Get(assignment_statement->name);
        if (before == nullptr) {
            throw RuntimeException("선언되지 않은 변수명입니다: " + assignment_statement->name, current_line);
        }

        auto value = eval(assignment_statement->value.get(), environment);

        if (!dynamic_cast<Null*>(value.get()) && !typeCheck(before->type, value)) {
            throw RuntimeException("값의 형식이 변수의 형식과 일치하지 않습니다.", current_line);
        }

        environment->Update(assignment_statement->name, value);
        return value;
    }
    if (auto* compound_statement = dynamic_cast<CompoundAssignmentStatement*>(node)) {
        current_line = compound_statement->op ? compound_statement->op->line : current_line;
        auto before = environment->Get(compound_statement->name);
        if (before == nullptr) {
            throw RuntimeException("선언되지 않은 변수명입니다: " + compound_statement->name, current_line);
        }

        auto rhs = eval(compound_statement->value.get(), environment);

        TokenType arithmeticOp = compoundToArithmeticOp(compound_statement->op->type);
        Token opToken{arithmeticOp, compound_statement->op->text, compound_statement->op->line};
        auto result = evalInfixExpression(&opToken, before, rhs);

        if (!typeCheck(before->type, result)) {
            throw RuntimeException("값의 형식이 변수의 형식과 일치하지 않습니다.", current_line);
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
                throw RuntimeException("반복문의 조건은 참/거짓 값이어야 합니다.", current_line);
            }
            if (!boolean->value) break;

            auto loopEnv = make_shared<Environment>(environment->shared_from_this());
            result = eval(while_statement->body.get(), loopEnv.get());

            if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
            if (dynamic_cast<ReturnValue*>(result.get())) return result;
        }
        return result;
    }
    if (auto* foreach_statement = dynamic_cast<ForEachStatement*>(node)) {
        auto iterable = eval(foreach_statement->iterable.get(), environment);
        shared_ptr<Object> result = nullptr;

        if (auto* array = dynamic_cast<Array*>(iterable.get())) {
            for (auto& element : array->elements) {
                if (!typeCheck(foreach_statement->elementType.get(), element)) {
                    throw RuntimeException("각각 반복문에서 원소의 타입이 일치하지 않습니다.", current_line);
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
                    throw RuntimeException("각각 반복문에서 원소의 타입이 일치하지 않습니다.", current_line);
                }
                auto loopEnv = make_shared<Environment>(environment->shared_from_this());
                loopEnv->Set(foreach_statement->elementName, ch);
                result = eval(foreach_statement->body.get(), loopEnv.get());
                if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
                if (dynamic_cast<ReturnValue*>(result.get())) return result;
            }
        } else {
            throw RuntimeException("각각 반복문은 배열 또는 문자열만 지원합니다.", current_line);
        }
        return result;
    }
    if (auto* trycatch = dynamic_cast<TryCatchStatement*>(node)) {
        try {
            return eval(trycatch->tryBody.get(), environment);
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
    if (auto* match_statement = dynamic_cast<MatchStatement*>(node)) {
        auto subject = eval(match_statement->subject.get(), environment);
        string subjectStr = subject->ToString();

        for (size_t i = 0; i < match_statement->caseValues.size(); i++) {
            auto caseVal = eval(match_statement->caseValues[i].get(), environment);
            if (caseVal->ToString() == subjectStr && caseVal->type == subject->type) {
                auto caseEnv = make_shared<Environment>(environment->shared_from_this());
                return eval(match_statement->caseBodies[i].get(), caseEnv.get());
            }
        }
        if (match_statement->defaultBody) {
            auto defaultEnv = make_shared<Environment>(environment->shared_from_this());
            return eval(match_statement->defaultBody.get(), defaultEnv.get());
        }
        return nullptr;
    }
    if (auto* class_statement = dynamic_cast<ClassStatement*>(node)) {
        auto classDef = make_shared<ClassDef>();
        classDef->name = class_statement->name;
        classDef->fieldTypes = class_statement->fieldTypes;
        classDef->fieldNames = class_statement->fieldNames;
        classDef->constructorParamTypes = class_statement->constructorParamTypes;
        classDef->constructorParams = class_statement->constructorParams;
        classDef->constructorBody = class_statement->constructorBody;
        classDef->env = environment->shared_from_this();

        for (auto& methodStmt : class_statement->methods) {
            auto fn = make_shared<Function>();
            fn->parameterTypes = methodStmt->parameterTypes;
            fn->parameters = methodStmt->parameters;
            fn->body = methodStmt->body;
            fn->returnType = methodStmt->returnType;
            fn->env = environment->shared_from_this();
            classDef->methods.push_back(fn);
            classDef->methodNames.push_back(methodStmt->name);
        }

        environment->Set(class_statement->name, classDef);
        return classDef;
    }

    // expression
    if (auto* prefix_expression = dynamic_cast<PrefixExpression*>(node)) {
        current_line = prefix_expression->token->line;
        auto right = eval(prefix_expression->right.get(), environment);
        return evalPrefixExpression(prefix_expression->token.get(), right);
    }
    if (auto* infix_expression = dynamic_cast<InfixExpression*>(node)) {
        current_line = infix_expression->token->line;
        auto left  = eval(infix_expression->left.get(), environment);
        auto right = eval(infix_expression->right.get(), environment);
        return evalInfixExpression(infix_expression->token.get(), left, right);
    }
    if (auto* identifier_expression = dynamic_cast<IdentifierExpression*>(node)) {
        auto value = environment->Get(identifier_expression->name);
        if (value != nullptr) return value;

        if (builtins.contains(identifier_expression->name)) {
            return builtins[identifier_expression->name];
        }

        throw RuntimeException("'" + identifier_expression->name + "' 존재하지 않는 식별자입니다.", current_line);
    }
    if (auto* call_expression = dynamic_cast<CallExpression*>(node)) {
        auto function = eval(call_expression->function.get(), environment);

        vector<shared_ptr<Object>> arguments;
        for (auto& argument : call_expression->arguments) {
            arguments.push_back(eval(argument.get(), environment));
        }

        if (auto* classDef = dynamic_cast<ClassDef*>(function.get())) {
            return instantiateClass(classDef, arguments, environment);
        }

        return applyFunction(function, arguments);
    }
    if (auto* member_access = dynamic_cast<MemberAccessExpression*>(node)) {
        auto obj = eval(member_access->object.get(), environment);
        return evalMemberAccess(obj, member_access->member);
    }
    if (auto* method_call = dynamic_cast<MethodCallExpression*>(node)) {
        auto obj = eval(method_call->object.get(), environment);
        vector<shared_ptr<Object>> args;
        for (auto& arg : method_call->arguments) {
            args.push_back(eval(arg.get(), environment));
        }
        return evalMethodCall(obj, method_call->method, args, environment);
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
        string value = string_literal->value;
        if (value.find('{') != string::npos) {
            value = interpolateString(value, environment);
        }
        return make_shared<String>(value);
    }
    if (dynamic_cast<NullLiteral*>(node)) {
        return make_shared<Null>();
    }
    if (auto* tuple_literal = dynamic_cast<TupleLiteral*>(node)) {
        auto tuple = make_shared<Tuple>();
        for (auto& elem : tuple_literal->elements) {
            tuple->elements.push_back(eval(elem.get(), environment));
        }
        return tuple;
    }
    if (auto* array_literal = dynamic_cast<ArrayLiteral*>(node)) {
        auto array = make_shared<Array>();
        for (auto& element : array_literal->elements) {
            array->elements.push_back(eval(element.get(), environment));
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
                throw RuntimeException("사전의 키는 문자열이어야 합니다.", current_line);
            }
            hashmap->pairs[str_key->value] = value;
        }
        return hashmap;
    }

    throw RuntimeException(node->String() + " 알 수 없는 구문입니다.", current_line);
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
    bool leftNull = dynamic_cast<Null*>(left.get()) != nullptr;
    bool rightNull = dynamic_cast<Null*>(right.get()) != nullptr;
    if (leftNull || rightNull) {
        if (token->type == TokenType::EQUAL) {
            return make_shared<Boolean>(leftNull && rightNull);
        }
        if (token->type == TokenType::NOT_EQUAL) {
            return make_shared<Boolean>(!(leftNull && rightNull));
        }
        throw RuntimeException("없음(null) 값에 대해서는 == 와 != 비교만 지원합니다.", token->line);
    }

    if (left->type == ObjectType::INTEGER && right->type == ObjectType::INTEGER) {
        return evalIntegerInfixExpression(token, left, right);
    }
    if (left->type == ObjectType::FLOAT && right->type == ObjectType::FLOAT) {
        auto* lf = dynamic_cast<Float*>(left.get());
        auto* rf = dynamic_cast<Float*>(right.get());
        return evalFloatInfixExpression(token, lf->value, rf->value);
    }
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

    throw RuntimeException("좌항과 우항의 타입을 연산할 수 없습니다.", token->line);
}

shared_ptr<Object> Evaluator::evalIntegerInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    auto* left_integer  = dynamic_cast<Integer*>(left.get());
    auto* right_integer = dynamic_cast<Integer*>(right.get());
    long long lv = left_integer->value;
    long long rv = right_integer->value;

    if (token->type == TokenType::PLUS) return make_shared<Integer>(lv + rv);
    if (token->type == TokenType::MINUS) return make_shared<Integer>(lv - rv);
    if (token->type == TokenType::ASTERISK) return make_shared<Integer>(lv * rv);
    if (token->type == TokenType::SLASH) {
        if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", token->line);
        return make_shared<Integer>(lv / rv);
    }
    if (token->type == TokenType::PERCENT) {
        if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", token->line);
        return make_shared<Integer>(lv % rv);
    }
    if (token->type == TokenType::BITWISE_AND) return make_shared<Integer>(lv & rv);
    if (token->type == TokenType::BITWISE_OR) return make_shared<Integer>(lv | rv);
    if (token->type == TokenType::EQUAL) return make_shared<Boolean>(lv == rv);
    if (token->type == TokenType::NOT_EQUAL) return make_shared<Boolean>(lv != rv);
    if (token->type == TokenType::LESS_THAN) return make_shared<Boolean>(lv < rv);
    if (token->type == TokenType::GREATER_THAN) return make_shared<Boolean>(lv > rv);
    if (token->type == TokenType::LESS_EQUAL) return make_shared<Boolean>(lv <= rv);
    if (token->type == TokenType::GREATER_EQUAL) return make_shared<Boolean>(lv >= rv);

    throw RuntimeException("정수 연산을 지원하지 않는 연산자입니다.", token->line);
}

shared_ptr<Object> Evaluator::evalFloatInfixExpression(Token* token, double lv, double rv) {
    if (token->type == TokenType::PLUS) return make_shared<Float>(lv + rv);
    if (token->type == TokenType::MINUS) return make_shared<Float>(lv - rv);
    if (token->type == TokenType::ASTERISK) return make_shared<Float>(lv * rv);
    if (token->type == TokenType::SLASH) {
        if (rv == 0.0) throw RuntimeException("0으로 나눌 수 없습니다.", token->line);
        return make_shared<Float>(lv / rv);
    }
    if (token->type == TokenType::EQUAL) return make_shared<Boolean>(lv == rv);
    if (token->type == TokenType::NOT_EQUAL) return make_shared<Boolean>(lv != rv);
    if (token->type == TokenType::LESS_THAN) return make_shared<Boolean>(lv < rv);
    if (token->type == TokenType::GREATER_THAN) return make_shared<Boolean>(lv > rv);
    if (token->type == TokenType::LESS_EQUAL) return make_shared<Boolean>(lv <= rv);
    if (token->type == TokenType::GREATER_EQUAL) return make_shared<Boolean>(lv >= rv);

    throw RuntimeException("실수 연산을 지원하지 않는 연산자입니다.", token->line);
}

shared_ptr<Object> Evaluator::evalBooleanInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    auto* lb = dynamic_cast<Boolean*>(left.get());
    auto* rb = dynamic_cast<Boolean*>(right.get());

    if (token->type == TokenType::LOGICAL_AND) return make_shared<Boolean>(lb->value && rb->value);
    if (token->type == TokenType::LOGICAL_OR) return make_shared<Boolean>(lb->value || rb->value);
    if (token->type == TokenType::EQUAL) return make_shared<Boolean>(lb->value == rb->value);
    if (token->type == TokenType::NOT_EQUAL) return make_shared<Boolean>(lb->value != rb->value);

    throw RuntimeException("논리 연산을 지원하지 않는 연산자입니다.", token->line);
}

shared_ptr<Object> Evaluator::evalStringInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    auto* ls = dynamic_cast<String*>(left.get());
    auto* rs = dynamic_cast<String*>(right.get());

    if (token->type == TokenType::PLUS) return make_shared<String>(ls->value + rs->value);
    if (token->type == TokenType::EQUAL) return make_shared<Boolean>(ls->value == rs->value);
    if (token->type == TokenType::NOT_EQUAL) return make_shared<Boolean>(ls->value != rs->value);

    throw RuntimeException("문자열 연산을 지원하지 않는 연산자입니다.", token->line);
}

shared_ptr<Object> Evaluator::evalIndexExpression(shared_ptr<Object> left, shared_ptr<Object> index) {
    if (left->type == ObjectType::ARRAY && index->type == ObjectType::INTEGER)
        return evalArrayIndexExpression(left, index);
    if (left->type == ObjectType::STRING && index->type == ObjectType::INTEGER)
        return evalStringIndexExpression(left, index);
    if (left->type == ObjectType::HASH_MAP && index->type == ObjectType::STRING)
        return evalHashMapIndexExpression(left, index);
    if (left->type == ObjectType::TUPLE && index->type == ObjectType::INTEGER) {
        auto* tuple = dynamic_cast<Tuple*>(left.get());
        auto* idx = dynamic_cast<Integer*>(index.get());
        if (idx->value < 0 || idx->value >= static_cast<long long>(tuple->elements.size())) {
            throw RuntimeException("튜플의 범위 밖 인덱스입니다.", current_line);
        }
        return tuple->elements[idx->value];
    }
    throw RuntimeException("인덱스 연산이 지원되지 않는 형식입니다.", current_line);
}

shared_ptr<Object> Evaluator::evalArrayIndexExpression(shared_ptr<Object> array, shared_ptr<Object> index) {
    auto* arr = dynamic_cast<Array*>(array.get());
    auto* idx = dynamic_cast<Integer*>(index.get());
    if (0 <= idx->value && idx->value < static_cast<long long>(arr->elements.size())) {
        return arr->elements[idx->value];
    }
    throw RuntimeException("배열의 범위 밖 값이 인덱스로 입력되었습니다. (인덱스: "
                           + to_string(idx->value) + ", 길이: " + to_string(arr->elements.size()) + ")", current_line);
}

shared_ptr<Object> Evaluator::evalStringIndexExpression(shared_ptr<Object> str, shared_ptr<Object> index) {
    auto* s = dynamic_cast<String*>(str.get());
    auto* idx = dynamic_cast<Integer*>(index.get());
    if (idx->value < 0 || idx->value >= static_cast<long long>(s->value.size())) {
        throw RuntimeException("문자열의 범위 밖 값이 인덱스로 입력되었습니다.", current_line);
    }
    return make_shared<String>(string(1, s->value[idx->value]));
}

shared_ptr<Object> Evaluator::evalHashMapIndexExpression(shared_ptr<Object> hashmap, shared_ptr<Object> key) {
    auto* m = dynamic_cast<HashMap*>(hashmap.get());
    auto* k = dynamic_cast<String*>(key.get());
    auto it = m->pairs.find(k->value);
    if (it == m->pairs.end()) {
        throw RuntimeException("사전에 키 '" + k->value + "'이(가) 존재하지 않습니다.", current_line);
    }
    return it->second;
}

shared_ptr<Object> Evaluator::evalPrefixExpression(Token* token, shared_ptr<Object> right) {
    if (token->type == TokenType::MINUS) return evalMinusPrefixExpression(right);
    if (token->type == TokenType::BANG) return evalBangPrefixExpression(right);
    throw RuntimeException("지원하지 않는 전위 연산자입니다.", token->line);
}

shared_ptr<Object> Evaluator::evalMinusPrefixExpression(shared_ptr<Object> right) {
    if (auto integer = dynamic_cast<Integer*>(right.get())) return make_shared<Integer>(-integer->value);
    if (auto float_obj = dynamic_cast<Float*>(right.get())) return make_shared<Float>(-float_obj->value);
    throw RuntimeException("'-' 전위 연산자가 지원되지 않는 타입입니다.", current_line);
}

shared_ptr<Object> Evaluator::evalBangPrefixExpression(shared_ptr<Object> right) {
    if (auto boolean = dynamic_cast<Boolean*>(right.get())) return make_shared<Boolean>(!boolean->value);
    throw RuntimeException("'!' 전위 연산자가 지원되지 않는 타입입니다.", current_line);
}

shared_ptr<Object> Evaluator::applyFunction(shared_ptr<Object> function, std::vector<shared_ptr<Object>> arguments) {
    if (auto* fn = dynamic_cast<Function*>(function.get())) {
        if (fn->parameterTypes.size() != arguments.size()) {
            throw RuntimeException("함수가 필요한 인자 개수와 입력된 인자 개수가 다릅니다. (필요: "
                                   + to_string(fn->parameterTypes.size()) + ", 입력: "
                                   + to_string(arguments.size()) + ")", current_line);
        }

        for (size_t i = 0; i < fn->parameterTypes.size(); i++) {
            if (!typeCheck(fn->parameterTypes[i].get(), arguments[i])) {
                throw RuntimeException("함수 인자 타입 오류: " + to_string(i + 1) + "번째 인자", current_line);
            }
        }

        auto extended_env = extendFunctionEnvironment(fn, arguments);
        auto evaluated = eval(fn->body.get(), extended_env.get());

        if ((fn->returnType == nullptr && evaluated == nullptr)
            || typeCheck(fn->returnType.get(), evaluated)) {
            return unwrapReturnValue(evaluated);
        }
        throw RuntimeException("함수 반환 타입과 실제 반환의 타입이 일치하지 않습니다.", current_line);
    }

    if (auto* builtin_object = dynamic_cast<Builtin*>(function.get())) {
        auto evaluated = builtin_object->function(arguments);
        if (builtin_object->returnType != nullptr && evaluated != nullptr) {
            if (typeCheck(builtin_object->returnType.get(), evaluated)) return evaluated;
            throw RuntimeException("함수 반환 타입과 실제 반환의 타입이 일치하지 않습니다.", current_line);
        }
        return evaluated;
    }

    throw RuntimeException("함수가 존재하지 않습니다.", current_line);
}

shared_ptr<Environment> Evaluator::extendFunctionEnvironment(Function* function, std::vector<shared_ptr<Object>> arguments) {
    auto env = make_shared<Environment>(function->env);
    for (size_t i = 0; i < function->parameters.size(); i++) {
        env->Set(function->parameters[i]->name, arguments[i]);
    }
    return env;
}

shared_ptr<Object> Evaluator::unwrapReturnValue(shared_ptr<Object> object) {
    if (auto* rv = dynamic_cast<ReturnValue*>(object.get())) return rv->value;
    return object;
}

bool Evaluator::typeCheck(Token* type, const shared_ptr<Object>& value) {
    if (type == nullptr) return false;
    if (value == nullptr) return false;
    if (dynamic_cast<Null*>(value.get())) return true;

    auto* rv = dynamic_cast<ReturnValue*>(value.get());
    Object* target = rv ? rv->value.get() : value.get();
    if (target == nullptr) return false;
    if (dynamic_cast<Null*>(target)) return true;

    if (type->type == TokenType::정수) return dynamic_cast<Integer*>(target) != nullptr;
    if (type->type == TokenType::실수) return dynamic_cast<Float*>(target) != nullptr;
    if (type->type == TokenType::문자) return dynamic_cast<String*>(target) != nullptr;
    if (type->type == TokenType::논리) return dynamic_cast<Boolean*>(target) != nullptr;
    if (type->type == TokenType::배열) return dynamic_cast<Array*>(target) != nullptr;
    if (type->type == TokenType::사전) return dynamic_cast<HashMap*>(target) != nullptr;

    return true;
}

bool Evaluator::typeCheck(ObjectType type, const shared_ptr<Object>& value) {
    if (dynamic_cast<Null*>(value.get())) return true;
    return type == value->type;
}

shared_ptr<Object> Evaluator::evalImport(const string& filename, Environment* environment) {
    if (importedFiles.contains(filename)) return nullptr;
    importedFiles.insert(filename);

    ifstream file(filename);
    if (!file.is_open()) {
        throw RuntimeException("파일을 열 수 없습니다: " + filename, current_line);
    }

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

    for (int i = 0; i < indent; i++) {
        allTokens.push_back(make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
    }
    if (allTokens.empty()) return nullptr;

    Parser importParser;
    auto program = importParser.Parsing(allTokens);

    shared_ptr<Object> result = nullptr;
    for (const auto& statement : program->statements) {
        result = eval(statement.get(), environment);
        if (dynamic_cast<ReturnValue*>(result.get())) return unwrapReturnValue(result);
    }
    return result;
}

// ===== 클래스 관련 =====

shared_ptr<Object> Evaluator::instantiateClass(ClassDef* classDef, vector<shared_ptr<Object>> arguments, Environment* environment) {
    auto instance = make_shared<Instance>();
    instance->classDef = shared_ptr<ClassDef>(shared_ptr<ClassDef>{}, classDef);
    instance->fields = make_shared<Environment>();

    for (size_t i = 0; i < classDef->fieldNames.size(); i++) {
        instance->fields->Set(classDef->fieldNames[i], make_shared<Null>());
    }

    if (classDef->constructorBody) {
        if (classDef->constructorParamTypes.size() != arguments.size()) {
            throw RuntimeException("생성자가 필요한 인자 개수와 입력된 인자 개수가 다릅니다. (필요: "
                                   + to_string(classDef->constructorParamTypes.size()) + ", 입력: "
                                   + to_string(arguments.size()) + ")", current_line);
        }
        for (size_t i = 0; i < classDef->constructorParamTypes.size(); i++) {
            if (!typeCheck(classDef->constructorParamTypes[i].get(), arguments[i])) {
                throw RuntimeException("생성자 인자 타입 오류: " + to_string(i + 1) + "번째 인자", current_line);
            }
        }

        auto constructorEnv = make_shared<Environment>(environment->shared_from_this());
        constructorEnv->Set("자기", instance);
        for (size_t i = 0; i < classDef->constructorParams.size(); i++) {
            constructorEnv->Set(classDef->constructorParams[i]->name, arguments[i]);
        }
        eval(classDef->constructorBody.get(), constructorEnv.get());
    }

    return instance;
}

shared_ptr<Object> Evaluator::evalMemberAccess(shared_ptr<Object> obj, const string& member) {
    if (auto* instance = dynamic_cast<Instance*>(obj.get())) {
        auto value = instance->fields->Get(member);
        if (value != nullptr) return value;
        throw RuntimeException("인스턴스에 '" + member + "' 필드가 존재하지 않습니다.", current_line);
    }
    throw RuntimeException("멤버 접근은 인스턴스에서만 사용할 수 있습니다.", current_line);
}

shared_ptr<Object> Evaluator::evalMethodCall(shared_ptr<Object> obj, const string& method,
                                              vector<shared_ptr<Object>> arguments, Environment* environment) {
    if (auto* instance = dynamic_cast<Instance*>(obj.get())) {
        for (size_t i = 0; i < instance->classDef->methodNames.size(); i++) {
            if (instance->classDef->methodNames[i] == method) {
                auto* fn = instance->classDef->methods[i].get();

                if (fn->parameterTypes.size() != arguments.size()) {
                    throw RuntimeException("메서드 '" + method + "'가 필요한 인자 개수와 입력된 인자 개수가 다릅니다.", current_line);
                }
                for (size_t j = 0; j < fn->parameterTypes.size(); j++) {
                    if (!typeCheck(fn->parameterTypes[j].get(), arguments[j])) {
                        throw RuntimeException("메서드 '" + method + "' 인자 타입 오류: " + to_string(j + 1) + "번째 인자", current_line);
                    }
                }

                auto methodEnv = make_shared<Environment>(fn->env);
                methodEnv->Set("자기", obj);
                for (size_t j = 0; j < fn->parameters.size(); j++) {
                    methodEnv->Set(fn->parameters[j]->name, arguments[j]);
                }

                auto result = eval(fn->body.get(), methodEnv.get());
                if (fn->returnType == nullptr && result == nullptr) return nullptr;
                if (typeCheck(fn->returnType.get(), result)) return unwrapReturnValue(result);
                throw RuntimeException("메서드 반환 타입과 실제 반환의 타입이 일치하지 않습니다.", current_line);
            }
        }
        throw RuntimeException("인스턴스에 '" + method + "' 메서드가 존재하지 않습니다.", current_line);
    }
    throw RuntimeException("메서드 호출은 인스턴스에서만 사용할 수 있습니다.", current_line);
}
