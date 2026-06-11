// TypeChecker — 진입점·문장 검사·스코프·진단 기록.
// 표현식 추론은 type_checker_expr.cpp, 클래스 처리는 type_checker_class.cpp 참조.
#include "type_checker.h"
#include "type_checker_util.h"

#include "../object/builtin_signatures.h"

#include <algorithm>

using type_checker_util::isPrimKind;
using type_checker_util::nodeLine;

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
        checkClassStatement(*cls);  // type_checker_class.cpp (spec D5)
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
