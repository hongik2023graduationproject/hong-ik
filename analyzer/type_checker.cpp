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
        // `자기.필드 = 값` (parser가 name="자기.필드"로 만듦) — 필드 검사/동적 등록 (spec D5)
        static const std::string kSelfPrefix = "자기.";
        if (assign->name.rfind(kSelfPrefix, 0) == 0 && !currentClassName_.empty()) {
            std::string field = assign->name.substr(kSelfPrefix.size());
            auto valueType = inferExpression(assign->value);
            if (auto fieldType = lookupField(currentClassName_, field)) {
                if (!fieldType->isAssignableFrom(*valueType)) {
                    warn(currentLine_, "TC002",
                         "'" + fieldType->toKorean() + "' 타입 변수 '" + assign->name + "'에 '"
                             + valueType->toKorean() + "' 값을 대입할 수 없습니다.");
                }
            } else {
                // 동적 필드 등록 (부록 C — 양 런타임 허용). 첫 대입 값으로 타입 추론.
                classInfos_[currentClassName_].fields[field] = valueType;
            }
            return;
        }
        // TC002: 기존 타입 vs 새 값 타입.
        // 재대입은 좁힘을 해제하고 원(선언) 타입 기준으로 검사 (spec D3)
        for (auto& overlay : narrowOverlays_) {
            overlay.erase(assign->name);
        }
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
        std::map<std::string, std::shared_ptr<Type>> thenNarrow;
        std::map<std::string, std::shared_ptr<Type>> elseNarrow;
        collectNarrowings(ifStmt->condition, true, thenNarrow);
        collectNarrowings(ifStmt->condition, false, elseNarrow);
        narrowOverlays_.push_back(std::move(thenNarrow));
        checkStatement(ifStmt->consequence);
        narrowOverlays_.pop_back();
        narrowOverlays_.push_back(std::move(elseNarrow));
        checkStatement(ifStmt->then);
        narrowOverlays_.pop_back();
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
        // spec D5: 이름·생성자 arity 등록 + ClassInfo(필드/메서드 시그니처) 구성.
        // 생성자 미정의(constructorBody 없음) 시 부모 생성자 arity 상속 (런타임 실측 2026-06-11)
        int constructorArity = static_cast<int>(cls->constructorParams.size());
        auto parent = cls->parentName.empty() ? classTypes_.end() : classTypes_.find(cls->parentName);
        if (!cls->constructorBody && parent != classTypes_.end()) {
            constructorArity = parent->second->constructorArity;
        }
        auto classType = std::make_shared<ClassType>(cls->name, constructorArity);
        classTypes_[cls->name] = classType;
        declare(cls->name, classType);
        if (!cls->parentName.empty() && parent == classTypes_.end()) {
            warn(currentLine_, "TC006", "식별자 '" + cls->parentName + "'를 찾을 수 없습니다.");
        }

        ClassInfo info;
        info.name = cls->name;
        info.parentName = cls->parentName;
        info.constructorArity = constructorArity;
        for (size_t i = 0; i < cls->fieldNames.size() && i < cls->fieldTypes.size(); i++) {
            info.fields[cls->fieldNames[i]] = typeFromToken(cls->fieldTypes[i], false);
        }
        for (const auto& method : cls->methods) {
            if (!method) {
                continue;
            }
            std::vector<std::shared_ptr<Type>> params;
            std::vector<bool> paramHasDefault;
            std::vector<std::string> paramNames;
            for (size_t i = 0; i < method->parameters.size(); i++) {
                bool optional = i < method->parameterOptionals.size() && method->parameterOptionals[i];
                params.push_back(typeFromToken(
                    i < method->parameterTypes.size() ? method->parameterTypes[i] : nullptr, optional));
                paramHasDefault.push_back(i < method->defaultValues.size()
                                          && method->defaultValues[i] != nullptr);
                paramNames.push_back(method->parameters[i] ? method->parameters[i]->name : "");
            }
            auto funcType = std::make_shared<FunctionType>(
                params, typeFromToken(method->returnType, method->returnTypeOptional), paramHasDefault);
            funcType->paramNames = paramNames;
            info.methods[method->name] = funcType;
        }
        classInfos_[cls->name] = std::move(info);

        checkClassBody(*cls);  // 생성자/메서드 본문 검사 (spec D5)
        return;
    }

    // 그 외 (Import/Break/Continue 등): 검사 대상 없음.
}

// spec 2.3: top-down 검사. 본문 진입 직전 declare로 재귀만 허용, hoisting 없음.
void TypeChecker::checkFunctionStatement(FunctionStatement& fn) {
    checkFunctionLike(fn, true);
}

void TypeChecker::checkFunctionLike(FunctionStatement& fn, bool declareName) {
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

    if (declareName) {
        declare(fn.name, funcType);
    }

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

void TypeChecker::pushScope() {
    scopes_.emplace_back();
}

void TypeChecker::popScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void TypeChecker::declare(const std::string& name, std::shared_ptr<Type> type) {
    // 같은 이름의 재선언은 좁힘 무효화 (spec D3)
    for (auto& overlay : narrowOverlays_) {
        overlay.erase(name);
    }
    if (scopes_.empty()) {
        globalTypes_[name] = std::move(type);  // REPL 재선언은 덮어쓰기 허용 (spec Z13)
    } else {
        scopes_.back().vars[name] = std::move(type);
    }
}

std::shared_ptr<Type> TypeChecker::lookup(const std::string& name) {
    // 분기 좁힘 오버레이 우선 (spec D3)
    for (auto it = narrowOverlays_.rbegin(); it != narrowOverlays_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
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

void TypeChecker::warnUnknownMember(const std::string& className, const std::string& member) {
    warn(currentLine_, "TC201", "클래스 '" + className + "'에 '" + member + "' 멤버가 없습니다.");
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
            // 클래스 이름 표기 = 인스턴스 타입 (Phase B-2 spec D5). 미등록이면 AnyType.
            auto found = classTypes_.find(tok->text);
            base = (found != classTypes_.end()) ? instanceTypeOf(tok->text) : makeAny();
            break;
        }
        default: base = makeAny(); break;
        }
    }
    return optional ? makeOptional(std::move(base)) : base;
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

// 클래스 본문 검사 (spec D5). 시그니처는 ClassStatement 처리에서 이미 classInfos_에 등록됨 —
// 메서드 상호 참조가 자기. 경유로 가능 (부록 C 실측).
void TypeChecker::checkClassBody(ClassStatement& cls) {
    auto self = instanceTypeOf(cls.name);
    auto prevClass = currentClassName_;
    currentClassName_ = cls.name;

    // `부모` 키워드는 부모 InstanceType으로 — 본문에서의 TC006 오탐 방지
    std::shared_ptr<Type> parentType =
        cls.parentName.empty() ? nullptr : instanceTypeOf(cls.parentName);

    if (cls.constructorBody) {
        pushScope();
        declare("자기", self);
        if (parentType) {
            declare("부모", parentType);
        }
        for (size_t i = 0; i < cls.constructorParams.size(); i++) {
            if (!cls.constructorParams[i]) {
                continue;
            }
            declare(cls.constructorParams[i]->name,
                    typeFromToken(i < cls.constructorParamTypes.size() ? cls.constructorParamTypes[i]
                                                                       : nullptr,
                                  false));
        }
        checkStatement(cls.constructorBody);
        popScope();
    }

    for (const auto& method : cls.methods) {
        if (!method) {
            continue;
        }
        pushScope();
        declare("자기", self);
        if (parentType) {
            declare("부모", parentType);
        }
        checkFunctionLike(*method, false);
        popScope();
    }

    currentClassName_ = prevClass;
}

// ---- Phase B-2 클래스 헬퍼 (spec D5) ----

const TypeChecker::ClassInfo* TypeChecker::findClassInfo(const std::string& className) const {
    auto it = classInfos_.find(className);
    return it == classInfos_.end() ? nullptr : &it->second;
}

std::vector<std::string> TypeChecker::ancestorChainOf(const std::string& className) const {
    std::vector<std::string> chain;
    const ClassInfo* info = findClassInfo(className);
    while (info && !info->parentName.empty()) {
        if (std::find(chain.begin(), chain.end(), info->parentName) != chain.end()) {
            break;  // 순환 상속 가드
        }
        chain.push_back(info->parentName);
        info = findClassInfo(info->parentName);
    }
    return chain;
}

std::shared_ptr<Type> TypeChecker::instanceTypeOf(const std::string& className) const {
    return makeInstance(className, ancestorChainOf(className));
}

std::shared_ptr<Type> TypeChecker::lookupField(const std::string& className,
                                               const std::string& field) const {
    for (const ClassInfo* info = findClassInfo(className); info;
         info = info->parentName.empty() ? nullptr : findClassInfo(info->parentName)) {
        auto found = info->fields.find(field);
        if (found != info->fields.end()) {
            return found->second;
        }
    }
    return nullptr;
}

std::shared_ptr<FunctionType> TypeChecker::lookupMethod(const std::string& className,
                                                        const std::string& method) const {
    for (const ClassInfo* info = findClassInfo(className); info;
         info = info->parentName.empty() ? nullptr : findClassInfo(info->parentName)) {
        auto found = info->methods.find(method);
        if (found != info->methods.end()) {
            return found->second;
        }
    }
    return nullptr;
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
