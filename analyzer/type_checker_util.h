#ifndef TYPE_CHECKER_UTIL_H
#define TYPE_CHECKER_UTIL_H

#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "../ast/statements.h"
#include "../util/type.h"

// type_checker_*.cpp 공용 헬퍼 (TypeChecker 비공개 구현 디테일 — 외부 사용 금지)
namespace type_checker_util {

// 토큰을 보유한 노드에서 줄 번호 추출 (evaluator nodeLine 준용)
inline long long nodeLine(Node* node) {
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

inline bool isPrimKind(const Type& t, ObjectType k) {
    auto* p = dynamic_cast<const PrimType*>(&t);
    return p && p->kind == k;
}

inline bool isNumeric(const Type& t) {
    return isPrimKind(t, ObjectType::INTEGER) || isPrimKind(t, ObjectType::FLOAT);
}

} // namespace type_checker_util

#endif // TYPE_CHECKER_UTIL_H
