#include "type_checker.h"

#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "../ast/statements.h"
#include "../object/builtin_signatures.h"

#include <algorithm>

namespace {

// 토큰을 보유한 노드에서 줄 번호 추출 (evaluator nodeLine 준용)
long long nodeLine(Node* node) {
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
        inferExpression(forRange->startExpr);
        inferExpression(forRange->endExpr);
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

    // FunctionStatement/ReturnStatement: Task 8, ClassStatement: Task 10.
    // 그 외 (Import/Break/Continue 등): 검사 대상 없음.
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

    // ---- 연산자/접근 (결과 타입은 Task 9에서 정밀화) ----
    if (auto* infix = dynamic_cast<InfixExpression*>(expr.get())) {
        inferExpression(infix->left);
        inferExpression(infix->right);
        return makeAny();
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
        // 호출 대상 검사는 Task 8 — 인자 traverse만
        for (const auto& arg : call->arguments) {
            inferExpression(arg);
        }
        return makeAny();
    }
    if (auto* methodCall = dynamic_cast<MethodCallExpression*>(expr.get())) {
        inferExpression(methodCall->object);
        for (const auto& arg : methodCall->arguments) {
            inferExpression(arg);
        }
        return makeAny();
    }
    if (auto* member = dynamic_cast<MemberAccessExpression*>(expr.get())) {
        inferExpression(member->object);
        return makeAny();
    }
    if (auto* index = dynamic_cast<IndexExpression*>(expr.get())) {
        inferExpression(index->name);
        inferExpression(index->index);
        return makeAny();
    }
    if (auto* slice = dynamic_cast<SliceExpression*>(expr.get())) {
        inferExpression(slice->object);
        inferExpression(slice->start);
        inferExpression(slice->end);
        return makeAny();
    }

    // IdentifierExpression(Task 7), Lambda/패턴 표현식 등: 분석 불가 처리
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
}
