#include "util/type.h"
#include <gtest/gtest.h>

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
