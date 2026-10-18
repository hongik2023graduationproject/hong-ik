#include "util/checked_int.h"
#include <gtest/gtest.h>
#include <limits>
#include <optional>

namespace {
    // 이전 구현(지수만큼 선형 반복). checked_int::pow가 끝나는 모든 입력에서 같은 결과를 내는지 비교한다.
    std::optional<long long> linearPow(long long base, long long exp) {
        long long result = 1;
        for (long long i = 0; i < exp; i++) {
            if (checked_int::mulOverflows(result, base)) {
                return std::nullopt;
            }
            result *= base;
        }
        return result;
    }
} // namespace

TEST(CheckedIntTest, PowMatchesLinearReferenceOnSmallRange) {
    for (long long base = -20; base <= 20; base++) {
        for (long long exp = 0; exp <= 70; exp++) {
            EXPECT_EQ(checked_int::pow(base, exp), linearPow(base, exp)) << base << " ** " << exp;
        }
    }
}

TEST(CheckedIntTest, PowMatchesLinearReferenceAtInt64Edges) {
    constexpr long long kMax = std::numeric_limits<long long>::max();
    constexpr long long kMin = std::numeric_limits<long long>::min();
    for (long long base : {kMax, kMin, kMin + 1, 3037000499LL, 3037000500LL, -3037000499LL, -3037000500LL, 2097151LL,
             2097152LL}) {
        for (long long exp = 0; exp <= 4; exp++) {
            EXPECT_EQ(checked_int::pow(base, exp), linearPow(base, exp)) << base << " ** " << exp;
        }
    }
}

TEST(CheckedIntTest, PowHandlesHugeExponentsWithoutLooping) {
    constexpr long long kHuge = 100000000000LL;
    EXPECT_EQ(checked_int::pow(0, kHuge), 0);
    EXPECT_EQ(checked_int::pow(1, kHuge), 1);
    EXPECT_EQ(checked_int::pow(-1, kHuge), 1);
    EXPECT_EQ(checked_int::pow(-1, kHuge + 1), -1);
    EXPECT_EQ(checked_int::pow(2, kHuge), std::nullopt);
    EXPECT_EQ(checked_int::pow(-2, kHuge + 1), std::nullopt);
    EXPECT_EQ(checked_int::pow(5, 0), 1);
}
