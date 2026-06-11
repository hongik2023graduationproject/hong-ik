#include "object/builtin_registry.h"
#include "object/builtin_signatures.h"
#include "util/type.h"
#include <gtest/gtest.h>
#include <string>
#include <unordered_set>

// Phase A 정적 타입 시스템 — Type 계층 단위 테스트
// spec: docs/superpowers/specs/2026-05-19-static-type-system-design.md (1.1)

TEST(TypeTest, AssignabilityRules) {
    auto integer = makePrim(ObjectType::INTEGER);
    auto floating = makePrim(ObjectType::FLOAT);
    auto str = makePrim(ObjectType::STRING);
    auto nullObj = makePrim(ObjectType::NULL_OBJ);
    auto optInteger = makeOptional(makePrim(ObjectType::INTEGER));
    auto any = makeAny();
    auto never = makeNever();

    // T <- T 동일 OK
    EXPECT_TRUE(integer->isAssignableFrom(*integer));

    // T <- U (다른 Prim) 거부
    EXPECT_FALSE(integer->isAssignableFrom(*str));

    // 실수 <- 정수 자동 승격 거부 (spec 1.1, Task 0 결과 박제)
    EXPECT_FALSE(floating->isAssignableFrom(*integer));

    // T? <- T 좁히기 OK
    EXPECT_TRUE(optInteger->isAssignableFrom(*integer));

    // T? <- 없음 OK
    EXPECT_TRUE(optInteger->isAssignableFrom(*nullObj));

    // T <- 없음 거부
    EXPECT_FALSE(integer->isAssignableFrom(*nullObj));

    // any <- * / * <- any 항상 OK
    EXPECT_TRUE(any->isAssignableFrom(*integer));
    EXPECT_TRUE(any->isAssignableFrom(*optInteger));
    EXPECT_TRUE(integer->isAssignableFrom(*any));
    EXPECT_TRUE(optInteger->isAssignableFrom(*any));

    // never 양방향 통과 (cascade 차단)
    EXPECT_TRUE(never->isAssignableFrom(*integer));
    EXPECT_TRUE(integer->isAssignableFrom(*never));
    EXPECT_TRUE(optInteger->isAssignableFrom(*never));
}

// plan Task 3 게이트: 시그니처 테이블 ↔ BuiltinRegistry 1:1 매칭.
// `반복`은 lexer 키워드 충돌로 호출 불가능한 dead builtin — 레지스트리에는
// 있지만 시그니처 테이블에서는 의도적으로 제외 (spec D2.1).
TEST(BuiltinSignaturesTest, MatchesRegistry) {
    const auto& registryNames = BuiltinRegistry::names();

    std::unordered_set<std::string> tableNames;
    for (const auto& sig : kBuiltinSignatures) {
        EXPECT_TRUE(tableNames.insert(sig.name).second) << "시그니처 중복: " << sig.name;
        EXPECT_TRUE(registryNames.count(sig.name)) << "레지스트리에 없는 시그니처: " << sig.name;
        EXPECT_LE(sig.minArity, sig.maxArity) << sig.name;
        EXPECT_GE(sig.minArity, 0) << sig.name;
    }

    for (const auto& name : registryNames) {
        if (name == "반복") continue;  // dead builtin (spec D2.1)
        EXPECT_TRUE(tableNames.count(name)) << "시그니처 누락: " << name;
    }

    EXPECT_EQ(tableNames.size(), registryNames.size() - 1);  // 반복 1건 제외
}
