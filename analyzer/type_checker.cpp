#include "type_checker.h"

#include "../object/builtin_signatures.h"

#include <algorithm>

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
    // Task 6~10에서 노드별 검사 추가. 골격 단계에서는 진단 없음.
    (void)stmt;
}

std::shared_ptr<Type> TypeChecker::inferExpression(const std::shared_ptr<Expression>& expr) {
    // Task 6~9에서 노드별 추론 추가. 골격 단계에서는 분석 불가 처리.
    (void)expr;
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
