#ifndef CHECKED_INT_H
#define CHECKED_INT_H

#include <limits>
#include <optional>

// VM과 트리워킹이 공유하는 정수 연산 검사. 두 백엔드의 정수 의미를 한 곳에서 정한다.
namespace checked_int {

    // 정수 연산 결과가 long long 범위를 넘을 때의 런타임 오류 문구. 두 백엔드가 같은 문구를 쓴다.
    inline constexpr const char* kOverflowMessage = "정수 연산 결과가 범위를 넘었습니다.";

    // a / b (b != 0). LLONG_MIN / -1은 결과가 범위를 넘으므로 nullopt — x86에서는 하드웨어 예외로 프로세스가 죽는다.
    inline std::optional<long long> div(long long a, long long b) {
        if (a == std::numeric_limits<long long>::min() && b == -1) {
            return std::nullopt;
        }
        return a / b;
    }

    // a % b (b != 0). 나머지는 항상 범위 안이다. b == -1이면 0 — LLONG_MIN % -1도 하드웨어 예외 대신 0.
    inline long long mod(long long a, long long b) {
        if (b == -1) {
            return 0;
        }
        return a % b;
    }

    // signed long long 곱셈 오버플로 검사. __builtin_mul_overflow는 MSVC에 없어 직접 계산한다.
    inline bool mulOverflows(long long a, long long b) {
        if (a == 0 || b == 0) {
            return false;
        }
        constexpr long long kMax = std::numeric_limits<long long>::max();
        constexpr long long kMin = std::numeric_limits<long long>::min();
        // std::abs(kMin)은 UB이므로 별도 처리한다.
        if (a == kMin || b == kMin) {
            return true;
        }
        long long absA = a < 0 ? -a : a;
        long long absB = b < 0 ? -b : b;
        return absA > kMax / absB;
    }

    // base ** exp (exp >= 0)를 정수로 계산한다. 결과가 long long을 넘으면 nullopt — 호출측이 실수로 폴백한다.
    // 제곱을 거듭해 O(log exp)로 계산한다. base를 제곱하는 것은 남은 지수 비트가 있을 때뿐이라
    // 중간값의 크기가 최종값 이하이고, 넘침 판정은 최종값이 범위를 넘는지와 같다.
    inline std::optional<long long> pow(long long base, long long exp) {
        long long result = 1;
        while (exp > 0) {
            if (exp & 1) {
                if (mulOverflows(result, base)) {
                    return std::nullopt;
                }
                result *= base;
            }
            exp >>= 1;
            if (exp > 0) {
                if (mulOverflows(base, base)) {
                    return std::nullopt;
                }
                base *= base;
            }
        }
        return result;
    }

} // namespace checked_int

#endif // CHECKED_INT_H
