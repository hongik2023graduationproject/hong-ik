#include "type_checker.h"

#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "../ast/statements.h"
#include "../object/builtin_signatures.h"

#include <algorithm>
#include <climits>

namespace {

// 토큰을 보유한 노드에서 줄 번호 추출 (evaluator nodeLine 준용)
long long nodeLine(Node* node) {
    if (node && node->line > 0) return node->line;  // D1: 파서 스탬프 우선
    if (auto* e = dynamic_cast<InfixExpression*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<PrefixExpression*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<PostfixExpression*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<IntegerLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<FloatLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<BooleanLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<StringLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<NullLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* e = dynamic_cast<ArrayLiteral*>(node)) return e->token ? e->token->line : 0;
    if (auto* s = dynamic_cast<InitializationStatement*>(node)) return s->type ? s->type->line : 0;
    if (auto* s = dynamic_cast<CompoundAssignmentStatement*>(node)) return s->op ? s->op->line : 0;
    return 0;
}

bool isPrimKind(const Type& t, ObjectType k) {
    auto* p = dynamic_cast<const PrimType*>(&t);
    return p && p->kind == k;
}

bool isNumeric(const Type& t) {
    return isPrimKind(t, ObjectType::INTEGER) || isPrimKind(t, ObjectType::FLOAT);
}

} // namespace

bool TypeChecker::Result::hasErrors() const {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [](const TypeDiagnostic& d) { return d.severity == Severity::ERROR; });
}

TypeChecker::TypeChecker() {
    registerBuiltins();
}

TypeChecker::Result TypeChecker::check(const std::shared_ptr<Program>& program) {
    diagnostics_.clear();
    if (program) {
        for (const auto& stmt : program->statements) {
            checkStatement(stmt);
        }
    }
    return Result{diagnostics_};
}

void TypeChecker::clearDiagnostics() {
    diagnostics_.clear();
}

void TypeChecker::checkStatement(const std::shared_ptr<Statement>& stmt) {
    if (!stmt) {
        return;
    }
    if (long long line = nodeLine(stmt.get()); line > 0) {
        currentLine_ = line;
    }

    if (auto* init = dynamic_cast<InitializationStatement*>(stmt.get())) {
        // TC001: 선언 타입 vs 초기값 타입
        auto declared = typeFromToken(init->type, init->isOptional);
        auto valueType = inferExpression(init->value);
        if (!declared->isAssignableFrom(*valueType)) {
            warn(currentLine_, "TC001",
                 "'" + declared->toKorean() + "' 타입 변수 '" + init->name + "'에 '"
                     + valueType->toKorean() + "' 값을 대입할 수 없습니다.");
        }
        // 검사 통과/실패 무관하게 표기 타입으로 등록 (spec 2.1)
        declare(init->name, declared);
        return;
    }

    if (auto* assign = dynamic_cast<AssignmentStatement*>(stmt.get())) {
        // TC002: 기존 타입 vs 새 값 타입
        auto valueType = inferExpression(assign->value);
        auto existing = lookup(assign->name);
        if (!existing) {
            warn(currentLine_, "TC006", "식별자 '" + assign->name + "'를 찾을 수 없습니다.");
        } else if (!existing->isAssignableFrom(*valueType)) {
            warn(currentLine_, "TC002",
                 "'" + existing->toKorean() + "' 타입 변수 '" + assign->name + "'에 '"
                     + valueType->toKorean() + "' 값을 대입할 수 없습니다.");
        }
        return;
    }

    if (auto* compound = dynamic_cast<CompoundAssignmentStatement*>(stmt.get())) {
        // 이항 연산자 일반 타입 호환성은 Phase B — 선언 여부만 확인
        auto valueType = inferExpression(compound->value);
        (void)valueType;
        if (!lookup(compound->name)) {
            warn(currentLine_, "TC006", "식별자 '" + compound->name + "'를 찾을 수 없습니다.");
        }
        return;
    }

    if (auto* exprStmt = dynamic_cast<ExpressionStatement*>(stmt.get())) {
        inferExpression(exprStmt->expression);
        return;
    }

    if (auto* block = dynamic_cast<BlockStatement*>(stmt.get())) {
        for (const auto& inner : block->statements) {
            checkStatement(inner);
        }
        return;
    }

    if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
        inferExpression(ifStmt->condition);
        checkStatement(ifStmt->consequence);
        checkStatement(ifStmt->then);
        return;
    }

    if (auto* whileStmt = dynamic_cast<WhileStatement*>(stmt.get())) {
        inferExpression(whileStmt->condition);
        checkStatement(whileStmt->body);
        return;
    }

    // ---- Z2 AST 노드 6종 (spec D6.5) ----

    if (auto* forEach = dynamic_cast<ForEachStatement*>(stmt.get())) {
        pushScope();
        declare(forEach->elementName, typeFromToken(forEach->elementType, false));
        inferExpression(forEach->iterable);
        checkStatement(forEach->body);
        popScope();
        return;
    }

    if (auto* forRange = dynamic_cast<ForRangeStatement*>(stmt.get())) {
        pushScope();
        declare(forRange->varName, typeFromToken(forRange->varType, false));
        // TC301: 두 런타임 모두 거부하는 경계 타입만 (spec D4 — 실수는 불일치라 면제)
        auto checkBound = [this](const std::shared_ptr<Type>& t) {
            if (isPrimKind(*t, ObjectType::STRING) || isPrimKind(*t, ObjectType::BOOLEAN)
                || isPrimKind(*t, ObjectType::NULL_OBJ) || isPrimKind(*t, ObjectType::ARRAY)
                || isPrimKind(*t, ObjectType::HASH_MAP)) {
                warn(currentLine_, "TC301",
                     "반복 범위는 정수여야 합니다. '" + t->toKorean() + "' 타입은 사용할 수 없습니다.");
            }
        };
        checkBound(inferExpression(forRange->startExpr));
        checkBound(inferExpression(forRange->endExpr));
        checkStatement(forRange->body);
        popScope();
        return;
    }

    if (auto* tryCatch = dynamic_cast<TryCatchStatement*>(stmt.get())) {
        checkStatement(tryCatch->tryBody);
        pushScope();
        declare(tryCatch->errorName, makeAny());
        checkStatement(tryCatch->catchBody);
        popScope();
        return;
    }

    if (auto* match = dynamic_cast<MatchStatement*>(stmt.get())) {
        inferExpression(match->subject);
        for (size_t i = 0; i < match->caseValues.size(); i++) {
            inferExpression(match->caseValues[i]);
            if (i < match->caseGuards.size() && match->caseGuards[i]) {
                inferExpression(match->caseGuards[i]);
            }
            if (i < match->caseBodies.size()) {
                checkStatement(match->caseBodies[i]);
            }
        }
        checkStatement(match->defaultBody);
        return;
    }

    if (auto* yield = dynamic_cast<YieldStatement*>(stmt.get())) {
        inferExpression(yield->expression);
        return;
    }

    if (auto* indexAssign = dynamic_cast<IndexAssignmentStatement*>(stmt.get())) {
        // 타입 일치 검사 안 함 (spec D6.5) — 미선언 식별자 진단을 위한 traverse만
        inferExpression(indexAssign->collection);
        inferExpression(indexAssign->index);
        inferExpression(indexAssign->value);
        return;
    }

    if (auto* fn = dynamic_cast<FunctionStatement*>(stmt.get())) {
        checkFunctionStatement(*fn);
        return;
    }

    if (auto* ret = dynamic_cast<ReturnStatement*>(stmt.get())) {
        auto valueType = inferExpression(ret->expression);
        // 글로벌 스코프의 리턴은 무시 (currentReturnType_ 없음)
        if (currentReturnType_ && !currentReturnType_->isAssignableFrom(*valueType)) {
            warn(currentLine_, "TC103",
                 "함수 '" + currentFunctionName_ + "'의 반환 타입 '" + currentReturnType_->toKorean()
                     + "'에 '" + valueType->toKorean() + "' 값을 반환할 수 없습니다.");
        }
        return;
    }

    if (auto* cls = dynamic_cast<ClassStatement*>(stmt.get())) {
        // spec D6: 이름·생성자 arity만 등록, 본문(필드/생성자/메서드) 미진입 (Phase B)
        auto classType = std::make_shared<ClassType>(
            cls->name, static_cast<int>(cls->constructorParams.size()));
        classTypes_[cls->name] = classType;
        declare(cls->name, classType);
        if (!cls->parentName.empty() && classTypes_.find(cls->parentName) == classTypes_.end()) {
            warn(currentLine_, "TC006", "식별자 '" + cls->parentName + "'를 찾을 수 없습니다.");
        }
        return;
    }

    // 그 외 (Import/Break/Continue 등): 검사 대상 없음.
}

// spec 2.3: top-down 검사. 본문 진입 직전 declare로 재귀만 허용, hoisting 없음.
void TypeChecker::checkFunctionStatement(FunctionStatement& fn) {
    std::vector<std::shared_ptr<Type>> params;
    std::vector<bool> paramHasDefault;
    std::vector<std::string> paramNames;
    for (size_t i = 0; i < fn.parameters.size(); i++) {
        bool optional = i < fn.parameterOptionals.size() && fn.parameterOptionals[i];
        params.push_back(typeFromToken(i < fn.parameterTypes.size() ? fn.parameterTypes[i] : nullptr,
                                       optional));
        paramHasDefault.push_back(i < fn.defaultValues.size() && fn.defaultValues[i] != nullptr);
        paramNames.push_back(fn.parameters[i] ? fn.parameters[i]->name : "");
    }
    auto ret = typeFromToken(fn.returnType, fn.returnTypeOptional);
    auto funcType = std::make_shared<FunctionType>(params, ret, paramHasDefault);
    funcType->paramNames = paramNames;

    declare(fn.name, funcType);

    pushScope();
    for (size_t i = 0; i < paramNames.size(); i++) {
        declare(paramNames[i], params[i]);
    }
    for (const auto& defaultValue : fn.defaultValues) {
        inferExpression(defaultValue);
    }
    auto prevReturnType = currentReturnType_;
    auto prevFunctionName = currentFunctionName_;
    currentReturnType_ = ret;
    currentFunctionName_ = fn.name;
    checkStatement(fn.body);
    currentReturnType_ = prevReturnType;
    currentFunctionName_ = prevFunctionName;
    popScope();
}

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
        inferExpression(methodCall->object);
        for (const auto& arg : methodCall->arguments) {
            inferExpression(arg);
        }
        return makeAny();
    }
    if (auto* member = dynamic_cast<MemberAccessExpression*>(expr.get())) {
        auto objectType = inferExpression(member->object);
        if (auto* opt = dynamic_cast<OptionalType*>(objectType.get())) {
            warnUnresolvedOptional(*opt);  // TC501: 멤버 접근 좌항 (inner로 진행)
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
        long long required = 0;
        for (size_t i = 0; i < func->params.size(); i++) {
            if (i >= func->paramHasDefault.size() || !func->paramHasDefault[i]) {
                required++;
            }
        }
        const auto maxParams = static_cast<long long>(func->params.size());
        if (argCount < required || argCount > maxParams) {
            std::string expected = required == maxParams
                                       ? std::to_string(maxParams) + "개의"
                                       : std::to_string(required) + "~" + std::to_string(maxParams)
                                             + "개의";
            warn(currentLine_, "TC101",
                 "함수 '" + calleeName + "'는 " + expected + " 매개변수를 받지만 "
                     + std::to_string(argCount) + "개가 전달되었습니다.");
        } else {
            for (long long i = 0; i < argCount; i++) {
                if (!func->params[i]->isAssignableFrom(*argTypes[i])) {
                    std::string paramName = i < static_cast<long long>(func->paramNames.size())
                                                ? func->paramNames[i]
                                                : std::to_string(i + 1) + "번째";
                    warn(currentLine_, "TC102",
                         "함수 '" + calleeName + "'의 매개변수 '" + paramName + "'는 '"
                             + func->params[i]->toKorean() + "' 타입이지만 '"
                             + argTypes[i]->toKorean() + "' 값이 전달되었습니다.");
                }
            }
        }
        return func->ret;
    }

    if (auto* cls = dynamic_cast<ClassType*>(calleeType.get())) {
        // 생성자 호출: 인자 개수만 검사, 결과는 AnyType (spec D6 — Phase B에서 정밀화)
        if (argCount != cls->constructorArity) {
            warn(currentLine_, "TC101",
                 "함수 '" + cls->name + "'는 " + std::to_string(cls->constructorArity)
                     + "개의 매개변수를 받지만 " + std::to_string(argCount) + "개가 전달되었습니다.");
        }
        return makeAny();
    }

    return makeAny();
}

void TypeChecker::pushScope() {
    scopes_.emplace_back();
}

void TypeChecker::popScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void TypeChecker::declare(const std::string& name, std::shared_ptr<Type> type) {
    if (scopes_.empty()) {
        globalTypes_[name] = std::move(type);  // REPL 재선언은 덮어쓰기 허용 (spec Z13)
    } else {
        scopes_.back().vars[name] = std::move(type);
    }
}

std::shared_ptr<Type> TypeChecker::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end()) {
            return found->second;
        }
    }
    auto global = globalTypes_.find(name);
    if (global != globalTypes_.end()) {
        return global->second;
    }
    return nullptr;
}

void TypeChecker::error(long long line, const std::string& code, const std::string& msg) {
    diagnostics_.push_back(TypeDiagnostic{line, 0, Severity::ERROR, code, msg});
}

void TypeChecker::warn(long long line, const std::string& code, const std::string& msg) {
    diagnostics_.push_back(TypeDiagnostic{line, 0, Severity::WARNING, code, msg});
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

std::shared_ptr<Type> TypeChecker::typeFromToken(const std::shared_ptr<Token>& tok, bool optional) {
    std::shared_ptr<Type> base;
    if (!tok) {
        base = makeAny();
    } else {
        switch (tok->type) {
        case TokenType::정수: base = makePrim(ObjectType::INTEGER); break;
        case TokenType::실수: base = makePrim(ObjectType::FLOAT); break;
        case TokenType::문자: base = makePrim(ObjectType::STRING); break;
        case TokenType::논리: base = makePrim(ObjectType::BOOLEAN); break;
        case TokenType::배열: base = makePrim(ObjectType::ARRAY); break;
        case TokenType::사전: base = makePrim(ObjectType::HASH_MAP); break;
        case TokenType::IDENTIFIER: {
            // 클래스 이름 표기. 미등록이면 AnyType (Task 10에서 선언 등록 시 해소).
            auto found = classTypes_.find(tok->text);
            base = (found != classTypes_.end()) ? std::shared_ptr<Type>(found->second) : makeAny();
            break;
        }
        default: base = makeAny(); break;
        }
    }
    return optional ? makeOptional(std::move(base)) : base;
}

void TypeChecker::registerBuiltins() {
    for (const auto& sig : kBuiltinSignatures) {
        auto ret = sig.retIsAny ? makeAny() : makePrim(sig.retKind);
        globalTypes_[sig.name] =
            makeBuiltin(sig.name, sig.minArity, sig.maxArity, sig.skipArgTypeCheck, std::move(ret));
    }
    // 고차 함수 특수 연산자 — BuiltinRegistry 밖에서 evaluator/VM이 직접 처리하지만
    // CallExpression의 식별자로 파싱되므로 TC006 오탐 방지를 위해 등록 (evaluator.cpp 실측 arity).
    globalTypes_["매핑"] = makeBuiltin("매핑", 2, 2, true, makePrim(ObjectType::ARRAY));
    globalTypes_["걸러내기"] = makeBuiltin("걸러내기", 2, 2, true, makePrim(ObjectType::ARRAY));
    globalTypes_["줄이기"] = makeBuiltin("줄이기", 3, 3, true, makeAny());
}
