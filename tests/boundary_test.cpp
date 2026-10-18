// 정수 경계·중첩 깊이·재귀 한도에서 지금 정상으로 동작하는 결과를 두 백엔드(VM, 트리워킹)에서 고정한다.
// 이후 오버플로·깊이 제한 변경이 정상 프로그램의 결과를 바꾸지 않는지 확인하는 안전망이다.
#include "benchmarks/runner.h"
#include "object/object.h"
#include <gtest/gtest.h>
#include <limits>
#include <string>

namespace {
    struct BothResults {
        std::shared_ptr<Object> vm;
        std::shared_ptr<Object> eval;
    };

    BothResults runBoth(const std::string& source) {
        auto sp = bench::prepare(source);
        return {bench::runVm(sp), bench::runEval(sp)};
    }

    void expectIntegerOnBoth(const std::string& source, long long expected) {
        auto r = runBoth(source);
        auto* vm   = dynamic_cast<Integer*>(r.vm.get());
        auto* eval = dynamic_cast<Integer*>(r.eval.get());
        ASSERT_NE(vm, nullptr) << "VM 결과가 정수가 아님: " << source;
        ASSERT_NE(eval, nullptr) << "트리워킹 결과가 정수가 아님: " << source;
        EXPECT_EQ(vm->value, expected) << source;
        EXPECT_EQ(eval->value, expected) << source;
    }

    void expectFloatOnBoth(const std::string& source) {
        auto r = runBoth(source);
        EXPECT_NE(dynamic_cast<Float*>(r.vm.get()), nullptr) << "VM 결과가 실수가 아님: " << source;
        EXPECT_NE(dynamic_cast<Float*>(r.eval.get()), nullptr) << "트리워킹 결과가 실수가 아님: " << source;
    }

    void expectRuntimeErrorOnBoth(const std::string& source, const std::string& message) {
        auto sp = bench::prepare(source);
        for (bool vm : {true, false}) {
            try {
                vm ? bench::runVm(sp) : bench::runEval(sp);
                ADD_FAILURE() << (vm ? "VM" : "트리워킹") << "에서 오류가 나지 않음: " << source;
            } catch (const std::exception& e) {
                EXPECT_NE(std::string(e.what()).find(message), std::string::npos)
                    << (vm ? "VM: " : "트리워킹: ") << e.what();
            }
        }
    }

    std::string nestedParens(int depth) {
        return std::string(depth, '(') + "1" + std::string(depth, ')');
    }

    std::string plusChain(int terms) {
        std::string source = "1";
        for (int i = 1; i < terms; i++) {
            source += "+1";
        }
        return source;
    }

    std::string countDown(int depth) {
        return "함수 내려가기(정수 수) -> 정수:\n"
               "    만약 수 <= 0 라면:\n"
               "        리턴 0\n"
               "    리턴 내려가기(수 - 1) + 1\n"
               "내려가기("
               + std::to_string(depth) + ")\n";
    }
} // namespace

TEST(BoundaryTest, IntegerLimitsStayExact) {
    constexpr long long kMax = std::numeric_limits<long long>::max();
    constexpr long long kMin = std::numeric_limits<long long>::min();
    expectIntegerOnBoth("9223372036854775807", kMax);
    expectIntegerOnBoth("-9223372036854775807 - 1", kMin);
    expectIntegerOnBoth("9223372036854775806 + 1", kMax);
    expectIntegerOnBoth("-9223372036854775807 / -1", kMax);
    expectIntegerOnBoth("-9223372036854775807 % 10", -7);
}

TEST(BoundaryTest, IntegerArithmeticNextToTheLimitsStaysExact) {
    constexpr long long kMax = std::numeric_limits<long long>::max();
    constexpr long long kMin = std::numeric_limits<long long>::min();
    expectIntegerOnBoth("정수 최대 = 9223372036854775807\n정수 최소 = -9223372036854775807 - 1\n최소 + 최대", -1);
    expectIntegerOnBoth("정수 최대 = 9223372036854775807\n최대 - 1 + 1", kMax);
    expectIntegerOnBoth("정수 최소 = -9223372036854775807 - 1\n최소 + 1 - 1", kMin);
    expectIntegerOnBoth("정수 값 = -9223372036854775807\n-값", kMax);
    expectIntegerOnBoth("정수 값 = 3037000499\n값 * 값", 9223372030926249001LL);
    expectIntegerOnBoth("정수 값 = -4611686018427387904\n값 * 2", kMin);
    expectIntegerOnBoth("정수 x = 9223372036854775806\nx++\nx", kMax);
    expectIntegerOnBoth("정수 x = -9223372036854775807\nx--\nx", kMin);
    expectIntegerOnBoth("절대값(-9223372036854775807)", kMax);
}

TEST(BoundaryTest, IntegerOverflowIsAnErrorOnBothBackends) {
    const std::string max = "정수 최대 = 9223372036854775807\n";
    const std::string min = "정수 최소 = -9223372036854775807 - 1\n";
    expectRuntimeErrorOnBoth(max + "최대 + 1", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(min + "최소 - 1", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(min + "최소 + -1", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(max + "최대 - -1", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth("정수 큰 = 4611686018427387904\n큰 * 2", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(min + "최소 * -1", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(min + "-최소", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(max + "정수 x = 최대\nx += 1\nx", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(max + "정수 x = 최대\nx++\nx", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(min + "정수 x = 최소\nx--\nx", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth(min + "절대값(최소)", "범위를 넘었습니다");
    // 상수식은 컴파일 단계 폴딩이 넘침을 런타임으로 넘긴다.
    expectRuntimeErrorOnBoth("9223372036854775807 + 1", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth("3037000500 * 3037000500", "범위를 넘었습니다");
}

TEST(BoundaryTest, MinIntegerDividedByMinusOneIsAnErrorNotACrash) {
    expectRuntimeErrorOnBoth("정수 최소 = -9223372036854775807 - 1\n최소 / -1", "범위를 넘었습니다");
    expectRuntimeErrorOnBoth("정수 최소 = -9223372036854775807 - 1\n정수 결과 = 최소\n결과 /= -1\n결과",
        "범위를 넘었습니다");
}

TEST(BoundaryTest, ModuloByMinusOneIsZeroForEveryInteger) {
    expectIntegerOnBoth("정수 최소 = -9223372036854775807 - 1\n최소 % -1", 0);
    expectIntegerOnBoth("정수 최소 = -9223372036854775807 - 1\n정수 결과 = 최소\n결과 %= -1\n결과", 0);
    expectIntegerOnBoth("정수 값 = 7\n값 % -1", 0);
    expectIntegerOnBoth("정수 값 = -7\n값 % 3", -1);
    expectIntegerOnBoth("정수 값 = 7\n값 / -1", -7);
}

TEST(BoundaryTest, FloatToIntegerInRangeKeepsCurrentResults) {
    constexpr long long kMin = std::numeric_limits<long long>::min();
    expectIntegerOnBoth("정수변환(2.9)", 2);
    expectIntegerOnBoth("정수변환(-1.9)", -1);
    expectIntegerOnBoth("반올림(2.5)", 3);
    expectIntegerOnBoth("올림(1.1)", 2);
    expectIntegerOnBoth("내림(-1.1)", -2);
    // 2^63보다 작은 가장 큰 double과 -2^63은 정확히 변환된다.
    expectIntegerOnBoth("정수변환(9223372036854774784.0)", 9223372036854774784LL);
    expectIntegerOnBoth("정수변환(-9223372036854775808.0)", kMin);
}

TEST(BoundaryTest, FloatToIntegerOutOfRangeIsAnError) {
    const std::string huge     = "1" + std::string(300, '0') + ".0"; // 1e300
    const std::string infinity = "실수 큰수 = " + huge + "\n실수 무한 = 큰수 * 큰수\n";
    const std::string notANumber = infinity + "실수 비수 = 무한 - 무한\n";
    for (const std::string fn : {"정수변환", "반올림", "올림", "내림"}) {
        expectRuntimeErrorOnBoth(fn + "(9223372036854775808.0)", "정수로 변환할 수 없습니다");
        expectRuntimeErrorOnBoth(fn + "(1000000000000000000000000000000.0)", "정수로 변환할 수 없습니다");
        expectRuntimeErrorOnBoth(infinity + fn + "(무한)", "정수로 변환할 수 없습니다");
        expectRuntimeErrorOnBoth(infinity + fn + "(0.0 - 무한)", "정수로 변환할 수 없습니다");
        expectRuntimeErrorOnBoth(notANumber + fn + "(비수)", "정수로 변환할 수 없습니다");
    }
}

TEST(BoundaryTest, JsonNumbersInsideInt64StayExactIntegers) {
    constexpr long long kMax = std::numeric_limits<long long>::max();
    constexpr long long kMin = std::numeric_limits<long long>::min();
    expectIntegerOnBoth("JSON_파싱(\"9007199254740993\")", 9007199254740993LL); // 2^53+1, 실수로 바뀌면 정밀도를 잃는다
    expectIntegerOnBoth("JSON_파싱(\"9223372036854775807\")", kMax);
    expectIntegerOnBoth("JSON_파싱(\"-9223372036854775808\")", kMin);
    expectFloatOnBoth("JSON_파싱(\"1.5e10\")");
}

TEST(BoundaryTest, JsonNumbersOutOfRangeAreRejected) {
    for (const std::string number :
        {"9223372036854775808", "-9223372036854775809", "99999999999999999999", "1e999", "-1e999", "1e-400"}) {
        expectRuntimeErrorOnBoth("JSON_파싱(\"" + number + "\")", "JSON 숫자가 범위를 벗어났습니다");
    }
}

TEST(BoundaryTest, PowerKeepsIntegerWhenItFitsAndFallsBackToFloatOtherwise) {
    expectIntegerOnBoth("2 ** 62", 4611686018427387904LL);
    expectIntegerOnBoth("3 ** 39", 4052555153018976267LL);
    expectIntegerOnBoth("(-3) ** 39", -4052555153018976267LL);
    expectIntegerOnBoth("(-2) ** 62", 4611686018427387904LL);
    expectIntegerOnBoth("0 ** 0", 1);
    expectIntegerOnBoth("7 ** 1", 7);
    expectFloatOnBoth("2 ** 63");
    expectFloatOnBoth("(-2) ** 63");
    expectFloatOnBoth("3 ** 40");
    expectFloatOnBoth("2 ** -1");
}

TEST(BoundaryTest, PowerWithHugeExponentFinishesOnBothBackends) {
    expectIntegerOnBoth("1 ** 100000000000", 1);
    expectIntegerOnBoth("0 ** 100000000000", 0);
    expectIntegerOnBoth("(-1) ** 100000000000", 1);
    expectIntegerOnBoth("(-1) ** 100000000001", -1);
    expectFloatOnBoth("2 ** 100000000000");
    expectFloatOnBoth("(-2) ** 100000000001");
    // 리터럴은 VM 컴파일러가 상수로 접으므로, 변수로 런타임 경로(VM::binaryOp)도 확인한다.
    expectIntegerOnBoth("정수 밑 = -1\n정수 지수 = 100000000001\n밑 ** 지수", -1);
    expectIntegerOnBoth("정수 밑 = 3\n정수 지수 = 39\n밑 ** 지수", 4052555153018976267LL);
    expectFloatOnBoth("정수 밑 = 2\n정수 지수 = 100000000000\n밑 ** 지수");
}

TEST(BoundaryTest, ModerateNestingRunsOnBothBackends) {
    expectIntegerOnBoth(nestedParens(200), 1);
    expectIntegerOnBoth(plusChain(300), 300);
}

TEST(BoundaryTest, ModerateRecursionRunsOnBothBackends) {
    expectIntegerOnBoth(countDown(50), 50);
}

TEST(BoundaryTest, EvalReportsRecursionLimitInsteadOfCrashing) {
    auto sp = bench::prepare(countDown(1100));
    try {
        bench::runEval(sp);
        FAIL() << "재귀 한도 초과가 오류로 보고되지 않음";
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find("최대 재귀 깊이"), std::string::npos) << e.what();
    }
}

TEST(BoundaryTest, VmReportsFrameLimitInsteadOfCrashing) {
    auto sp = bench::prepare(countDown(300));
    try {
        bench::runVm(sp);
        FAIL() << "프레임 한도 초과가 오류로 보고되지 않음";
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find("최대 호출 프레임 수"), std::string::npos) << e.what();
    }
}
