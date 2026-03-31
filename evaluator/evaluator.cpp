#include "evaluator.h"

#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "../environment/environment.h"
#include "../exception/exception.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include "../util/type_utils.h"
#include "../util/utf8_utils.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

using namespace std;

Evaluator::Evaluator(IOContext* ioCtx, ExecutionLimiter* limiter)
    : ioCtx(ioCtx), limiter(limiter) {
    environment = make_shared<Environment>();
    builtins = {
        {"길이", make_shared<Length>()},
        {"출력", make_shared<Print>(ioCtx)},
        {"추가", make_shared<Push>()},
        {"타입", make_shared<TypeOf>()},
        {"정수변환", make_shared<ToInteger>()},
        {"실수변환", make_shared<ToFloat>()},
        {"문자변환", make_shared<ToString_>()},
        {"입력", make_shared<Input>(ioCtx)},
        {"키목록", make_shared<Keys>()},
        {"포함", make_shared<Contains>()},
        {"설정", make_shared<MapSet>()},
        {"삭제", make_shared<Remove>()},
        {"파일읽기", make_shared<FileRead>(ioCtx)},
        {"파일쓰기", make_shared<FileWrite>(ioCtx)},
        {"절대값", make_shared<Abs>()},
        {"제곱근", make_shared<Sqrt>()},
        {"최대", make_shared<Max>()},
        {"최소", make_shared<Min>()},
        {"난수", make_shared<Random>()},
        {"사인", make_shared<Sin>()},
        {"코사인", make_shared<Cos>()},
        {"탄젠트", make_shared<Tan>()},
        {"로그", make_shared<Log>()},
        {"자연로그", make_shared<Ln>()},
        {"거듭제곱", make_shared<Power>()},
        {"파이", make_shared<Pi>()},
        {"자연수e", make_shared<EulerE>()},
        {"반올림", make_shared<Round>()},
        {"올림", make_shared<Ceil>()},
        {"내림", make_shared<Floor>()},
        {"분리", make_shared<Split>()},
        {"대문자", make_shared<ToUpper>()},
        {"소문자", make_shared<ToLower>()},
        {"치환", make_shared<Replace>()},
        {"자르기", make_shared<Trim>()},
        {"시작확인", make_shared<StartsWith>()},
        {"끝확인", make_shared<EndsWith>()},
        {"반복", make_shared<Repeat>()},
        {"채우기", make_shared<Pad>()},
        {"부분문자", make_shared<Substring>()},
        {"정렬", make_shared<Sort>()},
        {"뒤집기", make_shared<Reverse>()},
        {"찾기", make_shared<Find>()},
        {"조각", make_shared<Slice>()},
        {"JSON_파싱", make_shared<JsonParse>()},
        {"JSON_직렬화", make_shared<JsonSerialize>()},
    };
}

// 소멸자에서 환경을 명시적으로 정리하여 순환 참조(Environment ↔ Function)로 인한
// shared_ptr 메모리 누수를 줄임. 근본적 해결은 weak_ptr 또는 GC가 필요함.
Evaluator::~Evaluator() {
    environment->store.clear();
}

void Evaluator::checkLimits() {
    if (limiter && limiter->isTimeoutExceeded()) {
        throw RuntimeException("실행 시간 제한을 초과했습니다.", current_line);
    }
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
    if (auto* e = dynamic_cast<PostfixExpression*>(node)) return e->token ? e->token->line : 0;
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
    // 샌드박스 제한 체크 (시간 초과)
    checkLimits();

    // 재귀 깊이 제한 확인
    if (++recursionDepth > MAX_RECURSION_DEPTH) {
        recursionDepth--;
        throw RuntimeException("최대 재귀 깊이(" + to_string(MAX_RECURSION_DEPTH) + ")를 초과했습니다.", current_line);
    }
    struct RecursionGuard {
        int& depth;
        ~RecursionGuard() { depth--; }
    } guard{recursionDepth};

    // 노드에서 줄 번호 추출 (0이 아닌 경우에만 갱신)
    long long nodeLine = getLineFromNode(node);
    if (nodeLine > 0) current_line = nodeLine;

    // program
    if (auto* program = dynamic_cast<Program*>(node)) {
        // 비소유 shared_ptr: Program은 호출자가 소유하며 eval 호출 동안 유효함
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
            if (!initialization_statement->isOptional) {
                throw RuntimeException("선택적 타입이 아닌 변수에 '없음'을 대입할 수 없습니다.", current_line);
            }
            environment->Set(initialization_statement->name, value);
            if (initialization_statement->isOptional) {
                environment->SetOptional(initialization_statement->name);
            }
            // 선언된 타입 저장
            ObjectType declType = tokenTypeToObjectType(initialization_statement->type->type);
            if (declType != ObjectType::NULL_OBJ) {
                environment->SetType(initialization_statement->name, declType);
            }
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
                if (initialization_statement->isOptional) {
                    environment->SetOptional(initialization_statement->name);
                }
                return value;
            }
        }

        if (!typeCheck(initialization_statement->type.get(), value)) {
            throw RuntimeException("선언에서 자료형과 값의 타입이 일치하지 않습니다.", current_line);
        }
        environment->Set(initialization_statement->name, value);
        if (initialization_statement->isOptional) {
            environment->SetOptional(initialization_statement->name);
        }
        // 선언된 타입 저장
        ObjectType declType = tokenTypeToObjectType(initialization_statement->type->type);
        if (declType != ObjectType::NULL_OBJ) {
            environment->SetType(initialization_statement->name, declType);
        }
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

        if (dynamic_cast<Null*>(value.get())) {
            if (!environment->IsOptional(assignment_statement->name)) {
                throw RuntimeException("선택적 타입이 아닌 변수에 '없음'을 대입할 수 없습니다.", current_line);
            }
        } else if (environment->HasType(assignment_statement->name)) {
            // 선언된 타입이 있으면 그 타입으로 체크 (없음→값 대입 시에도 정확한 타입 체크)
            ObjectType declaredType = environment->GetType(assignment_statement->name);
            if (value->type != declaredType) {
                throw RuntimeException("값의 형식이 변수의 형식과 일치하지 않습니다.", current_line);
            }
        } else if (dynamic_cast<Null*>(before.get())) {
            // Optional 변수가 현재 '없음'인 경우, 선언 타입 없으면 새 값 대입 허용
        } else if (!typeCheck(before->type, value)) {
            throw RuntimeException("값의 형식이 변수의 형식과 일치하지 않습니다.", current_line);
        }

        environment->Update(assignment_statement->name, value);
        return value;
    }
    if (auto* idx_assign = dynamic_cast<IndexAssignmentStatement*>(node)) {
        auto collection = eval(idx_assign->collection.get(), environment);
        auto index = eval(idx_assign->index.get(), environment);
        auto value = eval(idx_assign->value.get(), environment);

        if (auto* arr = dynamic_cast<Array*>(collection.get())) {
            auto* idx = dynamic_cast<Integer*>(index.get());
            if (!idx) throw RuntimeException("배열 인덱스는 정수여야 합니다.", current_line);
            long long actualIdx = idx->value;
            if (actualIdx < 0) actualIdx += static_cast<long long>(arr->elements.size());
            if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
                throw RuntimeException("배열의 범위 밖 인덱스입니다.", current_line);
            arr->elements[actualIdx] = value;
        } else if (auto* hm = dynamic_cast<HashMap*>(collection.get())) {
            auto* key = dynamic_cast<String*>(index.get());
            if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", current_line);
            hm->pairs[key->value] = value;
        } else {
            throw RuntimeException("인덱스 대입이 지원되지 않는 형식입니다.", current_line);
        }
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
            if (limiter) limiter->incrementLoopCounter();

            auto condition = eval(while_statement->condition.get(), environment);
            auto* boolean = dynamic_cast<Boolean*>(condition.get());
            if (!boolean) {
                throw RuntimeException("반복문의 조건은 참/거짓 값이어야 합니다.", current_line);
            }
            if (!boolean->value) break;

            auto loopEnv = make_shared<Environment>(environment->shared_from_this());
            result = eval(while_statement->body.get(), loopEnv.get());

            if (dynamic_cast<ContinueSignal*>(result.get())) continue;
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
                if (limiter) limiter->incrementLoopCounter();
                if (!typeCheck(foreach_statement->elementType.get(), element)) {
                    throw RuntimeException("각각 반복문에서 원소의 타입이 일치하지 않습니다.", current_line);
                }
                auto loopEnv = make_shared<Environment>(environment->shared_from_this());
                loopEnv->Set(foreach_statement->elementName, element);
                result = eval(foreach_statement->body.get(), loopEnv.get());
                if (dynamic_cast<ContinueSignal*>(result.get())) continue;
                if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
                if (dynamic_cast<ReturnValue*>(result.get())) return result;
            }
        } else if (auto* str = dynamic_cast<String*>(iterable.get())) {
            auto codePoints = utf8::toCodePoints(str->value);
            for (size_t i = 0; i < codePoints.size(); i++) {
                if (limiter) limiter->incrementLoopCounter();
                auto ch = make_shared<String>(codePoints[i]);
                if (!typeCheck(foreach_statement->elementType.get(), ch)) {
                    throw RuntimeException("각각 반복문에서 원소의 타입이 일치하지 않습니다.", current_line);
                }
                auto loopEnv = make_shared<Environment>(environment->shared_from_this());
                loopEnv->Set(foreach_statement->elementName, ch);
                result = eval(foreach_statement->body.get(), loopEnv.get());
                if (dynamic_cast<ContinueSignal*>(result.get())) continue;
                if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
                if (dynamic_cast<ReturnValue*>(result.get())) return result;
            }
        } else if (auto* gen = dynamic_cast<GeneratorObject*>(iterable.get())) {
            while (gen->hasNext()) {
                if (limiter) limiter->incrementLoopCounter();
                auto element = gen->next();
                if (!typeCheck(foreach_statement->elementType.get(), element)) {
                    throw RuntimeException("각각 반복문에서 원소의 타입이 일치하지 않습니다.", current_line);
                }
                auto loopEnv = make_shared<Environment>(environment->shared_from_this());
                loopEnv->Set(foreach_statement->elementName, element);
                result = eval(foreach_statement->body.get(), loopEnv.get());
                if (dynamic_cast<ContinueSignal*>(result.get())) continue;
                if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
                if (dynamic_cast<ReturnValue*>(result.get())) return result;
            }
        } else {
            throw RuntimeException("각각 반복문은 배열, 문자열 또는 제너레이터만 지원합니다.", current_line);
        }
        return result;
    }
    if (auto* for_range = dynamic_cast<ForRangeStatement*>(node)) {
        auto startVal = eval(for_range->startExpr.get(), environment);
        auto endVal = eval(for_range->endExpr.get(), environment);
        auto* startInt = dynamic_cast<Integer*>(startVal.get());
        auto* endInt = dynamic_cast<Integer*>(endVal.get());
        if (!startInt || !endInt) {
            throw RuntimeException("반복 범위의 시작과 끝은 정수이어야 합니다.", current_line);
        }
        shared_ptr<Object> result = nullptr;
        for (long long i = startInt->value; i < endInt->value; i++) {
            if (limiter) limiter->incrementLoopCounter();
            auto loopEnv = make_shared<Environment>(environment->shared_from_this());
            loopEnv->Set(for_range->varName, make_shared<Integer>(i));
            result = eval(for_range->body.get(), loopEnv.get());
            if (dynamic_cast<ContinueSignal*>(result.get())) continue;
            if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
            if (dynamic_cast<ReturnValue*>(result.get())) return result;
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
    if (dynamic_cast<ContinueStatement*>(node)) {
        return make_shared<ContinueSignal>();
    }
    if (auto* return_statement = dynamic_cast<ReturnStatement*>(node)) {
        auto returnValue  = make_shared<ReturnValue>();
        auto value        = eval(return_statement->expression.get(), environment);
        returnValue->value = value;
        return returnValue;
    }
    if (dynamic_cast<YieldStatement*>(node)) {
        throw RuntimeException("'생산'은 제너레이터 함수 내에서만 사용할 수 있습니다.", current_line);
    }
    if (auto* function_statement = dynamic_cast<FunctionStatement*>(node)) {
        auto function = make_shared<Function>();
        function->body = function_statement->body;
        function->parameterTypes = function_statement->parameterTypes;
        function->parameters     = function_statement->parameters;
        function->defaultValues  = function_statement->defaultValues;
        function->parameterOptionals = function_statement->parameterOptionals;
        function->env            = environment->shared_from_this();
        function->returnType     = function_statement->returnType;
        function->returnTypeOptional = function_statement->returnTypeOptional;
        environment->Set(function_statement->name, function);
        return function;
    }
    if (auto* match_statement = dynamic_cast<MatchStatement*>(node)) {
        auto subject = eval(match_statement->subject.get(), environment);
        string subjectStr = subject->ToString();

        for (size_t i = 0; i < match_statement->caseValues.size(); i++) {
            bool matched = false;
            auto* caseExpr = match_statement->caseValues[i].get();

            // 범위 패턴: 경우 1~5:
            if (auto* rangePattern = dynamic_cast<RangePatternExpression*>(caseExpr)) {
                auto startVal = eval(rangePattern->start.get(), environment);
                auto endVal = eval(rangePattern->end.get(), environment);
                // 정수 범위
                if (subject->type == ObjectType::INTEGER &&
                    startVal->type == ObjectType::INTEGER &&
                    endVal->type == ObjectType::INTEGER) {
                    auto subjectInt = dynamic_cast<Integer*>(subject.get())->value;
                    auto startInt = dynamic_cast<Integer*>(startVal.get())->value;
                    auto endInt = dynamic_cast<Integer*>(endVal.get())->value;
                    matched = (subjectInt >= startInt && subjectInt <= endInt);
                }
                // 실수 범위
                else if ((subject->type == ObjectType::FLOAT || subject->type == ObjectType::INTEGER) &&
                         (startVal->type == ObjectType::FLOAT || startVal->type == ObjectType::INTEGER) &&
                         (endVal->type == ObjectType::FLOAT || endVal->type == ObjectType::INTEGER)) {
                    double subjectNum = subject->type == ObjectType::FLOAT
                        ? dynamic_cast<Float*>(subject.get())->value
                        : static_cast<double>(dynamic_cast<Integer*>(subject.get())->value);
                    double startNum = startVal->type == ObjectType::FLOAT
                        ? dynamic_cast<Float*>(startVal.get())->value
                        : static_cast<double>(dynamic_cast<Integer*>(startVal.get())->value);
                    double endNum = endVal->type == ObjectType::FLOAT
                        ? dynamic_cast<Float*>(endVal.get())->value
                        : static_cast<double>(dynamic_cast<Integer*>(endVal.get())->value);
                    matched = (subjectNum >= startNum && subjectNum <= endNum);
                }
            }
            // 타입 패턴: 경우 정수:, 경우 문자: 등
            else if (auto* typePattern = dynamic_cast<TypePatternExpression*>(caseExpr)) {
                auto tokenType = typePattern->typeToken->type;
                if (tokenType == TokenType::정수) matched = (subject->type == ObjectType::INTEGER);
                else if (tokenType == TokenType::실수) matched = (subject->type == ObjectType::FLOAT);
                else if (tokenType == TokenType::문자) matched = (subject->type == ObjectType::STRING);
                else if (tokenType == TokenType::논리) matched = (subject->type == ObjectType::BOOLEAN);
                else if (tokenType == TokenType::배열) matched = (subject->type == ObjectType::ARRAY);
                else if (tokenType == TokenType::사전) matched = (subject->type == ObjectType::HASH_MAP);
            }
            // 일반 값 매칭
            else {
                auto caseVal = eval(caseExpr, environment);
                matched = (caseVal->ToString() == subjectStr && caseVal->type == subject->type);
            }

            // 조건 가드 확인
            if (matched && i < match_statement->caseGuards.size() && match_statement->caseGuards[i]) {
                auto caseEnv = make_shared<Environment>(environment->shared_from_this());
                auto guardResult = eval(match_statement->caseGuards[i].get(), caseEnv.get());
                if (guardResult->type != ObjectType::BOOLEAN ||
                    !dynamic_cast<Boolean*>(guardResult.get())->value) {
                    matched = false;
                }
            }

            if (matched) {
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
        classDef->env = environment->shared_from_this();

        // 상속 처리
        if (!class_statement->parentName.empty()) {
            auto parentObj = environment->Get(class_statement->parentName);
            if (!parentObj) {
                throw RuntimeException("부모 클래스 '" + class_statement->parentName + "'을(를) 찾을 수 없습니다.", current_line);
            }
            auto parentDef = dynamic_pointer_cast<ClassDef>(parentObj);
            if (!parentDef) {
                throw RuntimeException("'" + class_statement->parentName + "'은(는) 클래스가 아닙니다.", current_line);
            }
            classDef->parent = parentDef;

            // 부모 필드 복사
            classDef->fieldTypes = parentDef->fieldTypes;
            classDef->fieldNames = parentDef->fieldNames;
            // 부모 메서드 복사
            classDef->methods = parentDef->methods;
            classDef->methodNames = parentDef->methodNames;
            // 부모 생성자 기본값
            if (!parentDef->constructorParams.empty() && class_statement->constructorParams.empty()) {
                classDef->constructorParamTypes = parentDef->constructorParamTypes;
                classDef->constructorParams = parentDef->constructorParams;
                classDef->constructorBody = parentDef->constructorBody;
            }
        }

        // 자식 필드 추가 (중복 없이)
        for (size_t i = 0; i < class_statement->fieldNames.size(); i++) {
            bool found = false;
            for (size_t j = 0; j < classDef->fieldNames.size(); j++) {
                if (classDef->fieldNames[j] == class_statement->fieldNames[i]) {
                    classDef->fieldTypes[j] = class_statement->fieldTypes[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                classDef->fieldTypes.push_back(class_statement->fieldTypes[i]);
                classDef->fieldNames.push_back(class_statement->fieldNames[i]);
            }
        }

        // 자식 생성자 오버라이드
        if (class_statement->constructorBody) {
            classDef->constructorParamTypes = class_statement->constructorParamTypes;
            classDef->constructorParams = class_statement->constructorParams;
            classDef->constructorBody = class_statement->constructorBody;
        }

        // 자식 메서드 추가/오버라이드
        for (auto& methodStmt : class_statement->methods) {
            auto fn = make_shared<Function>();
            fn->parameterTypes = methodStmt->parameterTypes;
            fn->parameters = methodStmt->parameters;
            fn->defaultValues = methodStmt->defaultValues;
            fn->body = methodStmt->body;
            fn->returnType = methodStmt->returnType;
            fn->env = environment->shared_from_this();

            bool overridden = false;
            for (size_t i = 0; i < classDef->methodNames.size(); i++) {
                if (classDef->methodNames[i] == methodStmt->name) {
                    classDef->methods[i] = fn;
                    overridden = true;
                    break;
                }
            }
            if (!overridden) {
                classDef->methods.push_back(fn);
                classDef->methodNames.push_back(methodStmt->name);
            }
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

        // 단축 평가: && 와 || 연산자는 좌항 결과에 따라 우항을 평가하지 않을 수 있음
        if (infix_expression->token->type == TokenType::LOGICAL_AND) {
            auto left = eval(infix_expression->left.get(), environment);
            auto* lb = dynamic_cast<Boolean*>(left.get());
            if (!lb) throw RuntimeException("논리곱(&&)의 좌항은 논리 값이어야 합니다.", current_line);
            if (!lb->value) return make_shared<Boolean>(false);
            auto right = eval(infix_expression->right.get(), environment);
            auto* rb = dynamic_cast<Boolean*>(right.get());
            if (!rb) throw RuntimeException("논리곱(&&)의 우항은 논리 값이어야 합니다.", current_line);
            return make_shared<Boolean>(rb->value);
        }
        if (infix_expression->token->type == TokenType::LOGICAL_OR) {
            auto left = eval(infix_expression->left.get(), environment);
            auto* lb = dynamic_cast<Boolean*>(left.get());
            if (!lb) throw RuntimeException("논리합(||)의 좌항은 논리 값이어야 합니다.", current_line);
            if (lb->value) return make_shared<Boolean>(true);
            auto right = eval(infix_expression->right.get(), environment);
            auto* rb = dynamic_cast<Boolean*>(right.get());
            if (!rb) throw RuntimeException("논리합(||)의 우항은 논리 값이어야 합니다.", current_line);
            return make_shared<Boolean>(rb->value);
        }

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
        // 고차 함수 체크 (매핑, 걸러내기, 줄이기)
        if (auto* ident = dynamic_cast<IdentifierExpression*>(call_expression->function.get())) {
            if (ident->name == "매핑" || ident->name == "걸러내기" || ident->name == "줄이기") {
                vector<shared_ptr<Object>> arguments;
                for (auto& argument : call_expression->arguments) {
                    arguments.push_back(eval(argument.get(), environment));
                }
                return evalHigherOrderCall(ident->name, arguments, environment);
            }
        }

        auto function = eval(call_expression->function.get(), environment);

        vector<shared_ptr<Object>> arguments;
        for (auto& argument : call_expression->arguments) {
            arguments.push_back(eval(argument.get(), environment));
        }

        if (dynamic_cast<ClassDef*>(function.get())) {
            return instantiateClass(function, arguments, environment);
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
    if (auto* lambda_expression = dynamic_cast<LambdaExpression*>(node)) {
        auto function = make_shared<Function>();
        function->parameterTypes = lambda_expression->parameterTypes;
        function->parameters = lambda_expression->parameters;
        function->env = environment->shared_from_this();
        function->returnType = lambda_expression->returnType;
        // Wrap the body expression in a return statement inside a block
        auto returnStmt = make_shared<ReturnStatement>();
        returnStmt->expression = lambda_expression->body;
        auto block = make_shared<BlockStatement>();
        block->statements.push_back(returnStmt);
        function->body = block;
        return function;
    }
    if (auto* postfix_expression = dynamic_cast<PostfixExpression*>(node)) {
        auto* ident = dynamic_cast<IdentifierExpression*>(postfix_expression->left.get());
        if (!ident) {
            throw RuntimeException("증감 연산자는 변수에만 사용할 수 있습니다.", current_line);
        }
        auto before = environment->Get(ident->name);
        if (!before) {
            throw RuntimeException("선언되지 않은 변수명입니다: " + ident->name, current_line);
        }
        auto* intVal = dynamic_cast<Integer*>(before.get());
        if (!intVal) {
            throw RuntimeException("증감 연산자는 정수 변수에만 사용할 수 있습니다.", current_line);
        }
        long long delta = (postfix_expression->token->type == TokenType::INCREMENT) ? 1 : -1;
        auto newVal = make_shared<Integer>(intVal->value + delta);
        environment->Update(ident->name, newVal);
        return before; // 후위 연산: 변경 전 값 반환
    }
    if (auto* index_expression = dynamic_cast<IndexExpression*>(node)) {
        auto left  = eval(index_expression->name.get(), environment);
        auto index = eval(index_expression->index.get(), environment);
        return evalIndexExpression(left, index);
    }
    if (auto* slice_expression = dynamic_cast<SliceExpression*>(node)) {
        auto object = eval(slice_expression->object.get(), environment);
        shared_ptr<Object> start = slice_expression->start ? eval(slice_expression->start.get(), environment) : nullptr;
        shared_ptr<Object> end = slice_expression->end ? eval(slice_expression->end.get(), environment) : nullptr;
        return evalSliceExpression(object, start, end);
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
            if (dynamic_cast<ReturnValue*>(result.get()) || dynamic_cast<BreakSignal*>(result.get()) || dynamic_cast<ContinueSignal*>(result.get())) {
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
        if (lf && rf) return evalFloatInfixExpression(token, lf->value, rf->value);
    }
    if (left->type == ObjectType::INTEGER && right->type == ObjectType::FLOAT) {
        auto* li = dynamic_cast<Integer*>(left.get());
        auto* rf = dynamic_cast<Float*>(right.get());
        if (li && rf) return evalFloatInfixExpression(token, static_cast<double>(li->value), rf->value);
    }
    if (left->type == ObjectType::FLOAT && right->type == ObjectType::INTEGER) {
        auto* lf = dynamic_cast<Float*>(left.get());
        auto* ri = dynamic_cast<Integer*>(right.get());
        if (lf && ri) return evalFloatInfixExpression(token, lf->value, static_cast<double>(ri->value));
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
    if (!left_integer || !right_integer) throw RuntimeException("정수 연산의 피연산자가 올바르지 않습니다.", token->line);
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
    if (token->type == TokenType::POWER) {
        long long result = 1;
        long long base = lv;
        long long exp = rv;
        if (exp < 0) return make_shared<Float>(std::pow(static_cast<double>(base), static_cast<double>(exp)));
        for (long long i = 0; i < exp; i++) result *= base;
        return make_shared<Integer>(result);
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
    if (token->type == TokenType::POWER) return make_shared<Float>(std::pow(lv, rv));
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
    if (!lb || !rb) throw RuntimeException("논리 연산의 피연산자가 올바르지 않습니다.", token->line);

    if (token->type == TokenType::LOGICAL_AND) return make_shared<Boolean>(lb->value && rb->value);
    if (token->type == TokenType::LOGICAL_OR) return make_shared<Boolean>(lb->value || rb->value);
    if (token->type == TokenType::EQUAL) return make_shared<Boolean>(lb->value == rb->value);
    if (token->type == TokenType::NOT_EQUAL) return make_shared<Boolean>(lb->value != rb->value);

    throw RuntimeException("논리 연산을 지원하지 않는 연산자입니다.", token->line);
}

shared_ptr<Object> Evaluator::evalStringInfixExpression(Token* token, shared_ptr<Object> left, shared_ptr<Object> right) {
    auto* ls = dynamic_cast<String*>(left.get());
    auto* rs = dynamic_cast<String*>(right.get());
    if (!ls || !rs) throw RuntimeException("문자열 연산의 피연산자가 올바르지 않습니다.", token->line);

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
    long long actualIdx = idx->value;
    if (actualIdx < 0) {
        actualIdx = static_cast<long long>(arr->elements.size()) + actualIdx;
    }
    if (0 <= actualIdx && actualIdx < static_cast<long long>(arr->elements.size())) {
        return arr->elements[actualIdx];
    }
    throw RuntimeException("배열의 범위 밖 값이 인덱스로 입력되었습니다. (인덱스: "
                           + to_string(idx->value) + ", 길이: " + to_string(arr->elements.size()) + ")", current_line);
}

shared_ptr<Object> Evaluator::evalStringIndexExpression(shared_ptr<Object> str, shared_ptr<Object> index) {
    auto* s = dynamic_cast<String*>(str.get());
    auto* idx = dynamic_cast<Integer*>(index.get());
    long long len = static_cast<long long>(utf8::codePointCount(s->value));
    long long actualIdx = idx->value;
    if (actualIdx < 0) {
        actualIdx = len + actualIdx;
    }
    if (actualIdx < 0 || actualIdx >= len) {
        throw RuntimeException("문자열의 범위 밖 값이 인덱스로 입력되었습니다.", current_line);
    }
    return make_shared<String>(utf8::nthCodePoint(s->value, static_cast<size_t>(actualIdx)));
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

shared_ptr<Object> Evaluator::evalSliceExpression(shared_ptr<Object> object, shared_ptr<Object> startObj, shared_ptr<Object> endObj) {
    if (object->type == ObjectType::ARRAY) {
        auto* arr = dynamic_cast<Array*>(object.get());
        long long len = static_cast<long long>(arr->elements.size());

        long long s = 0;
        long long e = len;

        if (startObj) {
            auto* startInt = dynamic_cast<Integer*>(startObj.get());
            if (!startInt) throw RuntimeException("슬라이스 시작 인덱스는 정수여야 합니다.", current_line);
            s = startInt->value;
            if (s < 0) s = len + s;
            if (s < 0) s = 0;
        }
        if (endObj) {
            auto* endInt = dynamic_cast<Integer*>(endObj.get());
            if (!endInt) throw RuntimeException("슬라이스 끝 인덱스는 정수여야 합니다.", current_line);
            e = endInt->value;
            if (e < 0) e = len + e;
            if (e > len) e = len;
        }
        if (s >= e) return make_shared<Array>();

        auto result = make_shared<Array>();
        for (long long i = s; i < e; i++) {
            result->elements.push_back(arr->elements[i]);
        }
        return result;
    }
    if (object->type == ObjectType::STRING) {
        auto* str = dynamic_cast<String*>(object.get());
        long long len = static_cast<long long>(utf8::codePointCount(str->value));

        long long s = 0;
        long long e = len;

        if (startObj) {
            auto* startInt = dynamic_cast<Integer*>(startObj.get());
            if (!startInt) throw RuntimeException("슬라이스 시작 인덱스는 정수여야 합니다.", current_line);
            s = startInt->value;
            if (s < 0) s = len + s;
            if (s < 0) s = 0;
        }
        if (endObj) {
            auto* endInt = dynamic_cast<Integer*>(endObj.get());
            if (!endInt) throw RuntimeException("슬라이스 끝 인덱스는 정수여야 합니다.", current_line);
            e = endInt->value;
            if (e < 0) e = len + e;
            if (e > len) e = len;
        }
        if (s >= e) return make_shared<String>("");

        return make_shared<String>(utf8::substringCodePoints(str->value, static_cast<size_t>(s), static_cast<size_t>(e)));
    }
    throw RuntimeException("슬라이스 연산이 지원되지 않는 형식입니다.", current_line);
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
        // 기본값으로 부족한 인자 채우기
        if (arguments.size() < fn->parameterTypes.size() && !fn->defaultValues.empty()) {
            for (size_t i = arguments.size(); i < fn->parameterTypes.size(); i++) {
                if (i < fn->defaultValues.size() && fn->defaultValues[i] != nullptr) {
                    auto defaultVal = eval(fn->defaultValues[i].get(), fn->env.get());
                    arguments.push_back(defaultVal);
                }
            }
        }

        if (fn->parameterTypes.size() != arguments.size()) {
            throw RuntimeException("함수가 필요한 인자 개수와 입력된 인자 개수가 다릅니다. (필요: "
                                   + to_string(fn->parameterTypes.size()) + ", 입력: "
                                   + to_string(arguments.size()) + ")", current_line);
        }

        for (size_t i = 0; i < fn->parameterTypes.size(); i++) {
            bool isParamOptional = i < fn->parameterOptionals.size() && fn->parameterOptionals[i];
            if (dynamic_cast<Null*>(arguments[i].get())) {
                if (!isParamOptional) {
                    throw RuntimeException("함수 인자 타입 오류: " + to_string(i + 1) + "번째 인자 (선택적 타입이 아닌 매개변수에 '없음' 전달)", current_line);
                }
            } else if (!typeCheck(fn->parameterTypes[i].get(), arguments[i])) {
                throw RuntimeException("함수 인자 타입 오류: " + to_string(i + 1) + "번째 인자", current_line);
            }
        }

        auto extended_env = extendFunctionEnvironment(fn, arguments);

        // 제너레이터 함수 감지: body에 생산(yield) 문이 있으면 GeneratorObject 반환
        if (containsYield(fn->body.get())) {
            auto generator = make_shared<GeneratorObject>();
            evalGeneratorBody(fn->body.get(), extended_env.get(), generator->values);
            return generator;
        }

        auto evaluated = eval(fn->body.get(), extended_env.get());

        if (fn->returnType == nullptr && evaluated == nullptr) {
            return unwrapReturnValue(evaluated);
        }
        if (fn->returnTypeOptional && evaluated != nullptr && dynamic_cast<Null*>(unwrapReturnValue(evaluated).get())) {
            return unwrapReturnValue(evaluated);
        }
        if (typeCheck(fn->returnType.get(), evaluated)) {
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
        if (i < function->parameterOptionals.size() && function->parameterOptionals[i]) {
            env->SetOptional(function->parameters[i]->name);
        }
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
    if (type->type == TokenType::함수) return dynamic_cast<Function*>(target) != nullptr || dynamic_cast<Builtin*>(target) != nullptr;

    // 클래스 타입 확인: IDENTIFIER인 경우 인스턴스의 클래스 이름과 비교
    if (type->type == TokenType::IDENTIFIER) {
        auto* inst = dynamic_cast<Instance*>(target);
        if (inst && inst->classDef) {
            return inst->classDef->name == type->text;
        }
        return false;
    }

    return true;
}

bool Evaluator::typeCheck(ObjectType type, const shared_ptr<Object>& value) {
    if (dynamic_cast<Null*>(value.get())) return true;
    return type == value->type;
}

bool Evaluator::containsYield(BlockStatement* block) {
    if (!block) return false;
    for (auto& stmt : block->statements) {
        if (dynamic_cast<YieldStatement*>(stmt.get())) return true;
        if (auto* whileStmt = dynamic_cast<WhileStatement*>(stmt.get())) {
            if (containsYield(whileStmt->body.get())) return true;
        }
        if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
            if (containsYield(ifStmt->consequence.get())) return true;
            if (containsYield(ifStmt->then.get())) return true;
        }
        if (auto* forEachStmt = dynamic_cast<ForEachStatement*>(stmt.get())) {
            if (containsYield(forEachStmt->body.get())) return true;
        }
        if (auto* forRangeStmt = dynamic_cast<ForRangeStatement*>(stmt.get())) {
            if (containsYield(forRangeStmt->body.get())) return true;
        }
    }
    return false;
}

shared_ptr<Object> Evaluator::evalGeneratorBody(Node* node, Environment* environment, vector<shared_ptr<Object>>& yieldValues) {
    checkLimits();

    if (++recursionDepth > MAX_RECURSION_DEPTH) {
        recursionDepth--;
        throw RuntimeException("최대 재귀 깊이(" + to_string(MAX_RECURSION_DEPTH) + ")를 초과했습니다.", current_line);
    }
    struct RecursionGuard {
        int& depth;
        ~RecursionGuard() { depth--; }
    } guard{recursionDepth};

    long long nodeLine = getLineFromNode(node);
    if (nodeLine > 0) current_line = nodeLine;

    if (auto* block_statement = dynamic_cast<BlockStatement*>(node)) {
        return evalGeneratorBlockStatement(block_statement->statements, environment, yieldValues);
    }
    if (auto* yield_statement = dynamic_cast<YieldStatement*>(node)) {
        auto value = eval(yield_statement->expression.get(), environment);
        yieldValues.push_back(value);
        return nullptr;
    }
    if (auto* if_statement = dynamic_cast<IfStatement*>(node)) {
        auto condition = eval(if_statement->condition.get(), environment);
        if (auto* boolean = dynamic_cast<Boolean*>(condition.get())) {
            if (boolean->value) {
                return evalGeneratorBody(if_statement->consequence.get(), environment, yieldValues);
            } else if (if_statement->then != nullptr) {
                return evalGeneratorBody(if_statement->then.get(), environment, yieldValues);
            }
        }
        return nullptr;
    }
    if (auto* while_statement = dynamic_cast<WhileStatement*>(node)) {
        shared_ptr<Object> result = nullptr;
        while (true) {
            if (limiter) limiter->incrementLoopCounter();
            auto condition = eval(while_statement->condition.get(), environment);
            auto* boolean = dynamic_cast<Boolean*>(condition.get());
            if (!boolean) {
                throw RuntimeException("반복문의 조건은 참/거짓 값이어야 합니다.", current_line);
            }
            if (!boolean->value) break;
            auto loopEnv = make_shared<Environment>(environment->shared_from_this());
            result = evalGeneratorBody(while_statement->body.get(), loopEnv.get(), yieldValues);
            if (dynamic_cast<ContinueSignal*>(result.get())) continue;
            if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
            if (dynamic_cast<ReturnValue*>(result.get())) return result;
        }
        return result;
    }
    if (auto* for_range = dynamic_cast<ForRangeStatement*>(node)) {
        auto startVal = eval(for_range->startExpr.get(), environment);
        auto endVal = eval(for_range->endExpr.get(), environment);
        auto* startInt = dynamic_cast<Integer*>(startVal.get());
        auto* endInt = dynamic_cast<Integer*>(endVal.get());
        if (!startInt || !endInt) {
            throw RuntimeException("반복 범위의 시작과 끝은 정수이어야 합니다.", current_line);
        }
        shared_ptr<Object> result = nullptr;
        for (long long i = startInt->value; i < endInt->value; i++) {
            if (limiter) limiter->incrementLoopCounter();
            auto loopEnv = make_shared<Environment>(environment->shared_from_this());
            loopEnv->Set(for_range->varName, make_shared<Integer>(i));
            result = evalGeneratorBody(for_range->body.get(), loopEnv.get(), yieldValues);
            if (dynamic_cast<ContinueSignal*>(result.get())) continue;
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
                if (limiter) limiter->incrementLoopCounter();
                auto loopEnv = make_shared<Environment>(environment->shared_from_this());
                loopEnv->Set(foreach_statement->elementName, element);
                result = evalGeneratorBody(foreach_statement->body.get(), loopEnv.get(), yieldValues);
                if (dynamic_cast<ContinueSignal*>(result.get())) continue;
                if (dynamic_cast<BreakSignal*>(result.get())) return nullptr;
                if (dynamic_cast<ReturnValue*>(result.get())) return result;
            }
        }
        return result;
    }
    // For all other nodes, delegate to normal eval
    return eval(node, environment);
}

shared_ptr<Object> Evaluator::evalGeneratorBlockStatement(const vector<shared_ptr<Statement>>& statements, Environment* environment, vector<shared_ptr<Object>>& yieldValues) {
    shared_ptr<Object> result = nullptr;
    for (const auto& statement : statements) {
        result = evalGeneratorBody(statement.get(), environment, yieldValues);
        if (result != nullptr) {
            if (dynamic_cast<ReturnValue*>(result.get()) || dynamic_cast<BreakSignal*>(result.get()) || dynamic_cast<ContinueSignal*>(result.get())) {
                return result;
            }
        }
    }
    return result;
}

shared_ptr<Object> Evaluator::evalImport(const string& filename, Environment* environment) {
    if (importedFiles.contains(filename)) return nullptr;
    importedFiles.insert(filename);

    string fileContent;

    // IOContext가 있으면 콜백을 사용 (WASM 환경에서 MemoryFileSystem 연동)
    if (ioCtx && ioCtx->fileRead) {
        try {
            fileContent = ioCtx->fileRead(filename);
        } catch (...) {
            throw RuntimeException("파일을 열 수 없습니다: " + filename, current_line);
        }
    } else {
        // 기본 파일시스템 I/O
        // 경로 정규화: 동일 파일의 중복 임포트를 방지
        string canonicalPath = filename;
        try {
            auto p = std::filesystem::weakly_canonical(filename);
            canonicalPath = p.string();
        } catch (...) {
            // 경로 정규화 실패 시 원본 경로 사용
        }

        // 정규화된 경로로 중복 체크
        if (canonicalPath != filename && importedFiles.contains(canonicalPath)) return nullptr;
        importedFiles.insert(canonicalPath);

        ifstream file(filename);
        if (!file.is_open()) {
            throw RuntimeException("파일을 열 수 없습니다: " + filename, current_line);
        }

        ostringstream oss;
        oss << file.rdbuf();
        fileContent = oss.str();
    }

    // 파일 내용을 토큰화
    Lexer importLexer;
    vector<shared_ptr<Token>> allTokens;
    int indent = 0;

    // 줄 단위로 토큰화 (Lexer가 줄 단위 indent 추적을 하므로)
    istringstream stream(fileContent);
    string line_str;
    while (getline(stream, line_str)) {
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

shared_ptr<Object> Evaluator::instantiateClass(shared_ptr<Object> classDefObj, vector<shared_ptr<Object>> arguments, Environment* environment) {
    auto classDefPtr = dynamic_pointer_cast<ClassDef>(classDefObj);
    auto* classDef = classDefPtr.get();
    auto instance = make_shared<Instance>();
    instance->classDef = classDefPtr;
    instance->fields = make_shared<Environment>();

    // 부모 클래스 체인의 필드도 초기화
    auto parentDef = classDef->parent;
    while (parentDef) {
        for (size_t i = 0; i < parentDef->fieldNames.size(); i++) {
            instance->fields->Set(parentDef->fieldNames[i], make_shared<Null>());
        }
        parentDef = parentDef->parent;
    }
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
        size_t paramCount = min(classDef->constructorParams.size(), arguments.size());
        for (size_t i = 0; i < paramCount; i++) {
            constructorEnv->Set(classDef->constructorParams[i]->name, arguments[i]);
        }
        eval(classDef->constructorBody.get(), constructorEnv.get());
    }

    return instance;
}

shared_ptr<Object> Evaluator::evalMemberAccess(shared_ptr<Object> obj, const string& member) {
    // 내장 타입의 길이 프로퍼티 지원
    if (member == "길이") {
        if (auto* str = dynamic_cast<String*>(obj.get())) {
            return make_shared<Integer>(static_cast<long long>(str->value.size()));
        }
        if (auto* arr = dynamic_cast<Array*>(obj.get())) {
            return make_shared<Integer>(static_cast<long long>(arr->elements.size()));
        }
        if (auto* hm = dynamic_cast<HashMap*>(obj.get())) {
            return make_shared<Integer>(static_cast<long long>(hm->pairs.size()));
        }
    }

    if (auto* instance = dynamic_cast<Instance*>(obj.get())) {
        auto value = instance->fields->Get(member);
        if (value != nullptr) return value;
        // 부모 클래스 체인에서 필드 존재 여부 확인 (상속)
        // 부모 필드는 instantiateClass에서 이미 instance->fields에 초기화됨
        auto parentDef = instance->classDef->parent;
        while (parentDef) {
            for (size_t i = 0; i < parentDef->fieldNames.size(); i++) {
                if (parentDef->fieldNames[i] == member) {
                    auto val = instance->fields->Get(member);
                    return val ? val : make_shared<Null>();
                }
            }
            parentDef = parentDef->parent;
        }
        throw RuntimeException("인스턴스에 '" + member + "' 필드가 존재하지 않습니다.", current_line);
    }
    throw RuntimeException("멤버 접근은 인스턴스에서만 사용할 수 있습니다.", current_line);
}

shared_ptr<Object> Evaluator::evalMethodCall(shared_ptr<Object> obj, const string& method,
                                              vector<shared_ptr<Object>> arguments, Environment* environment) {
    // 내장 타입에 대한 메서드 호출 지원
    // String 메서드
    if (dynamic_cast<String*>(obj.get()) || dynamic_cast<Array*>(obj.get()) || dynamic_cast<HashMap*>(obj.get())) {
        // 매핑 테이블: 메서드명 -> 내장함수명
        static const map<string, string> methodMap = {
            {"길이", "길이"},
            {"대문자", "대문자"},
            {"소문자", "소문자"},
            {"자르기", "자르기"},
            {"분리", "분리"},
            {"치환", "치환"},
            {"포함", "포함"},
            {"찾기", "찾기"},
            {"뒤집기", "뒤집기"},
            {"정렬", "정렬"},
            {"추가", "추가"},
            {"삭제", "삭제"},
            {"조각", "조각"},
            {"키목록", "키목록"},
            {"설정", "설정"},
            {"타입", "타입"},
        };
        auto it = methodMap.find(method);
        if (it != methodMap.end()) {
            auto builtinIt = builtins.find(it->second);
            if (builtinIt != builtins.end()) {
                // obj를 첫 번째 인자로 삽입
                vector<shared_ptr<Object>> allArgs;
                allArgs.push_back(obj);
                allArgs.insert(allArgs.end(), arguments.begin(), arguments.end());
                return builtinIt->second->function(allArgs);
            }
        }
    }

    if (auto* instance = dynamic_cast<Instance*>(obj.get())) {
        // 현재 클래스에서 메서드 검색 (상속된 메서드 포함 - 이미 classDef에 복사됨)
        for (size_t i = 0; i < instance->classDef->methodNames.size(); i++) {
            if (instance->classDef->methodNames[i] == method) {
                auto* fn = instance->classDef->methods[i].get();

                // 기본값으로 부족한 인자 채우기
                if (arguments.size() < fn->parameterTypes.size() && !fn->defaultValues.empty()) {
                    for (size_t j = arguments.size(); j < fn->parameterTypes.size(); j++) {
                        if (j < fn->defaultValues.size() && fn->defaultValues[j] != nullptr) {
                            auto defaultVal = eval(fn->defaultValues[j].get(), fn->env.get());
                            arguments.push_back(defaultVal);
                        }
                    }
                }

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

                // 부모 참조 설정 (상속 시)
                if (instance->classDef->parent) {
                    // 부모 인스턴스 역할: 같은 인스턴스지만 부모 클래스 정의를 사용
                    auto parentInstance = make_shared<Instance>();
                    parentInstance->classDef = instance->classDef->parent;
                    parentInstance->fields = instance->fields;
                    methodEnv->Set("부모", parentInstance);
                }

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

// 고차 함수 구현
shared_ptr<Object> Evaluator::evalHigherOrderCall(const string& name,
                                                   vector<shared_ptr<Object>> arguments, Environment* environment) {
    if (name == "매핑") {
        if (arguments.size() != 2) throw RuntimeException("매핑 함수는 인자를 2개 받습니다 (배열, 함수).", current_line);
        auto* arr = dynamic_cast<Array*>(arguments[0].get());
        if (!arr) throw RuntimeException("매핑 함수의 첫 번째 인자는 배열이어야 합니다.", current_line);
        auto result = make_shared<Array>();
        for (auto& elem : arr->elements) {
            result->elements.push_back(applyFunction(arguments[1], {elem}));
        }
        return result;
    }
    if (name == "걸러내기") {
        if (arguments.size() != 2) throw RuntimeException("걸러내기 함수는 인자를 2개 받습니다 (배열, 함수).", current_line);
        auto* arr = dynamic_cast<Array*>(arguments[0].get());
        if (!arr) throw RuntimeException("걸러내기 함수의 첫 번째 인자는 배열이어야 합니다.", current_line);
        auto result = make_shared<Array>();
        for (auto& elem : arr->elements) {
            auto cond = applyFunction(arguments[1], {elem});
            auto* boolVal = dynamic_cast<Boolean*>(cond.get());
            if (boolVal && boolVal->value) {
                result->elements.push_back(elem);
            }
        }
        return result;
    }
    if (name == "줄이기") {
        if (arguments.size() != 3) throw RuntimeException("줄이기 함수는 인자를 3개 받습니다 (배열, 함수, 초기값).", current_line);
        auto* arr = dynamic_cast<Array*>(arguments[0].get());
        if (!arr) throw RuntimeException("줄이기 함수의 첫 번째 인자는 배열이어야 합니다.", current_line);
        auto accumulator = arguments[2];
        for (auto& elem : arr->elements) {
            accumulator = applyFunction(arguments[1], {accumulator, elem});
        }
        return accumulator;
    }
    throw RuntimeException("알 수 없는 고차 함수: " + name, current_line);
}
