// TypeChecker — 표현식 추론 (inferExpression 계열). spec 1.1.2/1.3/D2/D3/부록 A.
#include "type_checker.h"
#include "type_checker_util.h"

#include <climits>

using type_checker_util::isNumeric;
using type_checker_util::isPrimKind;
using type_checker_util::nodeLine;

std::shared_ptr<Type> TypeChecker::inferExpression(const std::shared_ptr<Expression>& expr) {
    if (!expr) {
        return makeAny();
    }
    if (long long line = nodeLine(expr.get()); line > 0) {
        currentLine_ = line;
    }

    // ---- 리터럴 (spec D2.2: 컬렉션은 원소 타입 무시, 평면 처리) ----
    if (dynamic_cast<IntegerLiteral*>(expr.get())) return makePrim(ObjectType::INTEGER);
    if (dynamic_cast<FloatLiteral*>(expr.get())) return makePrim(ObjectType::FLOAT);
    if (dynamic_cast<StringLiteral*>(expr.get())) return makePrim(ObjectType::STRING);
    if (dynamic_cast<BooleanLiteral*>(expr.get())) return makePrim(ObjectType::BOOLEAN);
    if (dynamic_cast<NullLiteral*>(expr.get())) return makePrim(ObjectType::NULL_OBJ);

    if (auto* array = dynamic_cast<ArrayLiteral*>(expr.get())) {
        for (const auto& elem : array->elements) {
            inferExpression(elem);  // 원소 타입은 버리고 내부 진단만 수집
        }
        return makePrim(ObjectType::ARRAY);
    }
    if (auto* hashmap = dynamic_cast<HashMapLiteral*>(expr.get())) {
        for (const auto& key : hashmap->keys) inferExpression(key);
        for (const auto& value : hashmap->values) inferExpression(value);
        return makePrim(ObjectType::HASH_MAP);
    }
    if (auto* tuple = dynamic_cast<TupleLiteral*>(expr.get())) {
        for (const auto& elem : tuple->elements) {
            inferExpression(elem);
        }
        return makePrim(ObjectType::TUPLE);
    }

    // ---- 식별자 (TC006) ----
    if (auto* ident = dynamic_cast<IdentifierExpression*>(expr.get())) {
        if (auto found = lookup(ident->name)) {
            return found;
        }
        warn(currentLine_, "TC006", "식별자 '" + ident->name + "'를 찾을 수 없습니다.");
        return makeNever();  // cascade 진단 차단 (spec 1.1.2)
    }

    // ---- 연산자/접근 ----
    if (auto* infix = dynamic_cast<InfixExpression*>(expr.get())) {
        return inferInfixExpression(*infix);
    }
    if (auto* prefix = dynamic_cast<PrefixExpression*>(expr.get())) {
        inferExpression(prefix->right);
        return makeAny();
    }
    if (auto* postfix = dynamic_cast<PostfixExpression*>(expr.get())) {
        inferExpression(postfix->left);
        return makeAny();
    }
    if (auto* call = dynamic_cast<CallExpression*>(expr.get())) {
        return inferCallExpression(*call);
    }
    if (auto* methodCall = dynamic_cast<MethodCallExpression*>(expr.get())) {
        auto objectType = inferExpression(methodCall->object);
        std::vector<std::shared_ptr<Type>> argTypes;
        argTypes.reserve(methodCall->arguments.size());
        for (const auto& arg : methodCall->arguments) {
            argTypes.push_back(inferExpression(arg));
        }
        if (auto* opt = dynamic_cast<OptionalType*>(objectType.get())) {
            warnUnresolvedOptional(*opt);  // TC501: 멤버 접근 좌항
            return makeAny();
        }
        auto* inst = dynamic_cast<InstanceType*>(objectType.get());
        if (!inst || !findClassInfo(inst->className)) {
            // Any/내장 타입 메서드 체이닝(evaluator methodMap) 등 — Phase B-2 침묵
            return makeAny();
        }
        auto method = lookupMethod(inst->className, methodCall->method);
        if (!method) {
            warnUnknownMember(inst->className, methodCall->method);
            return makeNever();
        }
        checkCallArguments(*method, methodCall->method, argTypes);
        return method->ret;
    }
    if (auto* member = dynamic_cast<MemberAccessExpression*>(expr.get())) {
        auto objectType = inferExpression(member->object);
        if (auto* opt = dynamic_cast<OptionalType*>(objectType.get())) {
            warnUnresolvedOptional(*opt);  // TC501: 멤버 접근 좌항 (inner로 진행)
            return makeAny();
        }
        if (auto* inst = dynamic_cast<InstanceType*>(objectType.get())) {
            if (auto fieldType = lookupField(inst->className, member->member)) {
                return fieldType;
            }
            if (lookupMethod(inst->className, member->member)) {
                return makeAny();  // 호출 없는 메서드 참조 — Phase B-2는 Any
            }
            if (findClassInfo(inst->className)) {  // 정보 없는 클래스는 침묵
                warnUnknownMember(inst->className, member->member);
                return makeNever();
            }
        }
        return makeAny();
    }
    if (auto* index = dynamic_cast<IndexExpression*>(expr.get())) {
        auto collectionType = inferExpression(index->name);
        if (auto* opt = dynamic_cast<OptionalType*>(collectionType.get())) {
            warnUnresolvedOptional(*opt);  // TC501: 인덱스 접근 좌항
        }
        inferExpression(index->index);
        return makeAny();
    }
    if (auto* slice = dynamic_cast<SliceExpression*>(expr.get())) {
        inferExpression(slice->object);
        inferExpression(slice->start);
        inferExpression(slice->end);
        return makeAny();
    }

    // Lambda/패턴 표현식 등: 분석 불가 처리
    return makeAny();
}

// spec 부록 A.2 — 실측(2026-06-11) 기반 결과 타입 추론. 진단(TC601)은 부록 A.1 규칙.
std::shared_ptr<Type> TypeChecker::inferInfixExpression(InfixExpression& infix) {
    auto left = inferExpression(infix.left);
    auto right = inferExpression(infix.right);
    if (dynamic_cast<NeverType*>(left.get()) || dynamic_cast<NeverType*>(right.get())) {
        return makeNever();  // cascade (spec 1.1.2)
    }
    const TokenType op = infix.token ? infix.token->type : TokenType::ILLEGAL;

    const bool numPair = isNumeric(*left) && isNumeric(*right);
    const bool intPair = isPrimKind(*left, ObjectType::INTEGER) && isPrimKind(*right, ObjectType::INTEGER);
    const bool strPair = isPrimKind(*left, ObjectType::STRING) && isPrimKind(*right, ObjectType::STRING);
    const bool boolPair = isPrimKind(*left, ObjectType::BOOLEAN) && isPrimKind(*right, ObjectType::BOOLEAN);
    // TC601은 실측 범위(PrimType 쌍)에서만 발화 — Function/Class/Builtin 등은 면제 (부록 A.1)
    const bool bothPrim = dynamic_cast<PrimType*>(left.get()) && dynamic_cast<PrimType*>(right.get());
    const std::string opText = infix.token ? infix.token->text : "?";

    // ==/!=: 항상 논리, TC501 면제 (null 체크 패턴 — spec 3).
    // 허용: 숫자쌍, 문자쌍, 논리쌍, 한쪽이 없음. 그 외 PrimType 쌍은 TC601 (예: 배열 == 배열).
    if (op == TokenType::EQUAL || op == TokenType::NOT_EQUAL) {
        if (bothPrim) {
            const bool nullSide = isPrimKind(*left, ObjectType::NULL_OBJ)
                                  || isPrimKind(*right, ObjectType::NULL_OBJ);
            if (!nullSide && !numPair && !strPair && !boolPair) {
                warnBinaryIncompatible(opText, *left, *right);
            }
        }
        return makePrim(ObjectType::BOOLEAN);
    }
    // Optional 피연산자: TC501 우선 (TC601 미발화), 결과 Any (Phase A 유지)
    if (auto* opt = dynamic_cast<OptionalType*>(left.get())) {
        warnUnresolvedOptional(*opt);
        return makeAny();
    }
    if (auto* opt = dynamic_cast<OptionalType*>(right.get())) {
        warnUnresolvedOptional(*opt);
        return makeAny();
    }

    switch (op) {
    case TokenType::PLUS:
        if (intPair) return makePrim(ObjectType::INTEGER);
        if (numPair) return makePrim(ObjectType::FLOAT);
        if (strPair) return makePrim(ObjectType::STRING);
        if (bothPrim) warnBinaryIncompatible(opText, *left, *right);
        return makeAny();
    case TokenType::MINUS:
    case TokenType::ASTERISK:
    case TokenType::SLASH:
    case TokenType::POWER:
        if (intPair) return makePrim(ObjectType::INTEGER);
        if (numPair) return makePrim(ObjectType::FLOAT);
        if (bothPrim) warnBinaryIncompatible(opText, *left, *right);
        return makeAny();
    case TokenType::PERCENT:
    case TokenType::BITWISE_AND:
    case TokenType::BITWISE_OR:
        if (intPair) return makePrim(ObjectType::INTEGER);
        if (bothPrim) warnBinaryIncompatible(opText, *left, *right);
        return makeAny();
    case TokenType::LESS_THAN:
    case TokenType::GREATER_THAN:
    case TokenType::LESS_EQUAL:
    case TokenType::GREATER_EQUAL:
        if (numPair) return makePrim(ObjectType::BOOLEAN);
        // 문자쌍은 런타임 불일치 (부록 B #3) — 진단 면제
        if (bothPrim && !strPair) warnBinaryIncompatible(opText, *left, *right);
        return makeAny();
    case TokenType::LOGICAL_AND:
    case TokenType::LOGICAL_OR:
        // 좌항만 검사 — 우항은 단락 평가로 값 의존 (부록 A.1, B #4)
        if (dynamic_cast<PrimType*>(left.get()) && !isPrimKind(*left, ObjectType::BOOLEAN)) {
            warnBinaryIncompatible(opText, *left, *right);
        }
        if (boolPair) return makePrim(ObjectType::BOOLEAN);
        return makeAny();  // 혼합은 VM이 우항 값을 그대로 반환 (부록 B #4)
    default:
        return makeAny();
    }
}

// spec 1.3 + plan Task 8: 호출 검사 (TC101/TC102)
std::shared_ptr<Type> TypeChecker::inferCallExpression(CallExpression& call) {
    std::string calleeName = "함수";
    if (auto* ident = dynamic_cast<IdentifierExpression*>(call.function.get())) {
        calleeName = ident->name;
    }
    auto calleeType = inferExpression(call.function);

    std::vector<std::shared_ptr<Type>> argTypes;
    argTypes.reserve(call.arguments.size());
    for (const auto& arg : call.arguments) {
        argTypes.push_back(inferExpression(arg));
    }
    const auto argCount = static_cast<long long>(argTypes.size());

    // 미선언(TC006 기발화) → 추가 진단 없이 차단
    if (dynamic_cast<NeverType*>(calleeType.get())) {
        return makeNever();
    }
    if (dynamic_cast<AnyType*>(calleeType.get())) {
        return makeAny();
    }

    if (auto* builtin = dynamic_cast<BuiltinFunctionType*>(calleeType.get())) {
        if (argCount < builtin->minArity || argCount > builtin->maxArity) {
            std::string expected = builtin->minArity == builtin->maxArity
                                       ? std::to_string(builtin->minArity) + "개의"
                                       : (builtin->maxArity == INT_MAX
                                              ? "최소 " + std::to_string(builtin->minArity) + "개의"
                                              : std::to_string(builtin->minArity) + "~"
                                                    + std::to_string(builtin->maxArity) + "개의");
            warn(currentLine_, "TC101",
                 "함수 '" + builtin->name + "'는 " + expected + " 매개변수를 받지만 "
                     + std::to_string(argCount) + "개가 전달되었습니다.");
        }
        // Phase A: 전 빌트인 skipArgTypeCheck=true (spec D2.1) — 인자 타입 검사 생략
        return builtin->ret;
    }

    if (auto* func = dynamic_cast<FunctionType*>(calleeType.get())) {
        checkCallArguments(*func, calleeName, argTypes);
        return func->ret;
    }

    if (auto* cls = dynamic_cast<ClassType*>(calleeType.get())) {
        // 생성자 호출: 인자 개수만 검사, 결과는 해당 클래스의 InstanceType (spec D5)
        if (argCount != cls->constructorArity) {
            warn(currentLine_, "TC101",
                 "함수 '" + cls->name + "'는 " + std::to_string(cls->constructorArity)
                     + "개의 매개변수를 받지만 " + std::to_string(argCount) + "개가 전달되었습니다.");
        }
        return instanceTypeOf(cls->name);
    }

    return makeAny();
}

// 사용자 함수/메서드 공용 인자 검사 (TC101/TC102) — spec 1.3 의사코드의 FunctionType 분기
void TypeChecker::checkCallArguments(const FunctionType& func, const std::string& calleeName,
                                     const std::vector<std::shared_ptr<Type>>& argTypes) {
    const auto argCount = static_cast<long long>(argTypes.size());
    long long required = 0;
    for (size_t i = 0; i < func.params.size(); i++) {
        if (i >= func.paramHasDefault.size() || !func.paramHasDefault[i]) {
            required++;
        }
    }
    const auto maxParams = static_cast<long long>(func.params.size());
    if (argCount < required || argCount > maxParams) {
        std::string expected = required == maxParams
                                   ? std::to_string(maxParams) + "개의"
                                   : std::to_string(required) + "~" + std::to_string(maxParams)
                                         + "개의";
        warn(currentLine_, "TC101",
             "함수 '" + calleeName + "'는 " + expected + " 매개변수를 받지만 "
                 + std::to_string(argCount) + "개가 전달되었습니다.");
        return;
    }
    for (long long i = 0; i < argCount; i++) {
        if (!func.params[i]->isAssignableFrom(*argTypes[i])) {
            std::string paramName = i < static_cast<long long>(func.paramNames.size())
                                        ? func.paramNames[i]
                                        : std::to_string(i + 1) + "번째";
            warn(currentLine_, "TC102",
                 "함수 '" + calleeName + "'의 매개변수 '" + paramName + "'는 '"
                     + func.params[i]->toKorean() + "' 타입이지만 '" + argTypes[i]->toKorean()
                     + "' 값이 전달되었습니다.");
        }
    }
}

// spec D3: 단순 IdentifierExpression과 NullLiteral의 ==/!= 비교만 인식. `&&`는 then측만
// 재귀 (else측은 부정이 분배되지 않으므로 단일 비교일 때만 좁힘). `||`/멤버 접근은 미지원.
void TypeChecker::collectNarrowings(const std::shared_ptr<Expression>& cond, bool forThen,
                                    std::map<std::string, std::shared_ptr<Type>>& out) {
    auto* infix = dynamic_cast<InfixExpression*>(cond.get());
    if (!infix || !infix->token) {
        return;
    }
    if (infix->token->type == TokenType::LOGICAL_AND) {
        if (forThen) {
            collectNarrowings(infix->left, true, out);
            collectNarrowings(infix->right, true, out);
        }
        return;
    }
    const bool isNeq = infix->token->type == TokenType::NOT_EQUAL;
    const bool isEq = infix->token->type == TokenType::EQUAL;
    if (!((forThen && isNeq) || (!forThen && isEq))) {
        return;
    }

    auto* identLeft = dynamic_cast<IdentifierExpression*>(infix->left.get());
    auto* identRight = dynamic_cast<IdentifierExpression*>(infix->right.get());
    auto* nullLeft = dynamic_cast<NullLiteral*>(infix->left.get());
    auto* nullRight = dynamic_cast<NullLiteral*>(infix->right.get());
    IdentifierExpression* ident = (identLeft && nullRight) ? identLeft
                                : (identRight && nullLeft) ? identRight
                                                           : nullptr;
    if (!ident) {
        return;
    }
    auto type = lookup(ident->name);
    if (!type) {
        return;
    }
    if (auto* opt = dynamic_cast<OptionalType*>(type.get())) {
        out[ident->name] = opt->inner;
    }
}

void TypeChecker::warnUnresolvedOptional(const OptionalType& opt) {
    warn(currentLine_, "TC501",
         "Optional 타입 '" + opt.toKorean()
             + "' 값에 대해 직접 연산할 수 없습니다. null 검사 후 사용하세요.");
}

void TypeChecker::warnBinaryIncompatible(const std::string& opText, const Type& left,
                                         const Type& right) {
    warn(currentLine_, "TC601",
         "연산자 '" + opText + "'를 '" + left.toKorean() + "'과(와) '" + right.toKorean()
             + "' 타입에 적용할 수 없습니다.");
}
