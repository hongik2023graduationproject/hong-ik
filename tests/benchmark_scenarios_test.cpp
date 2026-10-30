#include "benchmarks/runner.h"
#include "object/object.h"
#include <gtest/gtest.h>

namespace {

    long long asInt(const std::shared_ptr<Object>& obj) {
        auto* i = dynamic_cast<Integer*>(obj.get());
        EXPECT_NE(i, nullptr) << "결과가 Integer가 아님: " << (obj ? obj->ToString() : "null");
        return i ? i->value : -1;
    }

    // 시나리오별 기대값 — 등가 Python으로 독립 계산한 값 (benchmarks/reference/ 참조)
    TEST(BenchScenarios, ArithLoopVmAndEvalAgree) {
        auto sp = bench::loadScenario("arith_loop");
        EXPECT_EQ(asInt(bench::runVm(sp)), 500000500000LL); // sum(1..1000000)
        EXPECT_EQ(asInt(bench::runEval(sp)), 500000500000LL);
    }

    TEST(BenchScenarios, FibRecursiveVmAndEvalAgree) {
        auto sp = bench::loadScenario("fib_recursive");
        EXPECT_EQ(asInt(bench::runVm(sp)), 75025LL); // fib(25)
        EXPECT_EQ(asInt(bench::runEval(sp)), 75025LL);
    }

    TEST(BenchScenarios, StringConcat) {
        auto sp = bench::loadScenario("string_concat");
        EXPECT_EQ(asInt(bench::runVm(sp)), 5000LL); // 5000회 1자 결합 후 길이
        EXPECT_EQ(asInt(bench::runEval(sp)), 5000LL);
    }

    TEST(BenchScenarios, StringIndex) {
        auto sp = bench::loadScenario("string_index");
        EXPECT_EQ(asInt(bench::runVm(sp)), 20000LL); // i in 0..99999, i%5==0 개수
        EXPECT_EQ(asInt(bench::runEval(sp)), 20000LL);
    }

    TEST(BenchScenarios, ArrayOps) {
        auto sp = bench::loadScenario("array_ops");
        EXPECT_EQ(asInt(bench::runVm(sp)), 49995000LL); // sum(0..9999)
        EXPECT_EQ(asInt(bench::runEval(sp)), 49995000LL);
    }

    TEST(BenchScenarios, HashmapOps) {
        auto sp = bench::loadScenario("hashmap_ops");
        EXPECT_EQ(asInt(bench::runVm(sp)), 600030000LL); // sum over 20000 iters of (3i+3)
        EXPECT_EQ(asInt(bench::runEval(sp)), 600030000LL);
    }

    TEST(BenchScenarios, ClassMethod) {
        auto sp = bench::loadScenario("class_method");
        EXPECT_EQ(asInt(bench::runVm(sp)), 1250025000LL); // sum(1..50000)
        EXPECT_EQ(asInt(bench::runEval(sp)), 1250025000LL);
    }

    TEST(BenchScenarios, BuiltinCalls) {
        auto sp = bench::loadScenario("builtin_calls");
        EXPECT_EQ(asInt(bench::runVm(sp)), 500000LL); // 길이(a)=5, 100000회
        EXPECT_EQ(asInt(bench::runEval(sp)), 500000LL);
    }

    // 결정성: 같은 ScenarioProgram으로 2회 실행해도 같은 결과 (벤치 반복 실행의 전제).
    // 상수 풀 변형 회귀가 실제로 위험한 컬렉션·클래스 시나리오까지 포함한다
    // (arith/string 계열은 스칼라라 제외 — CI 시간 절충).
    TEST(BenchScenarios, RepeatedRunsAreDeterministic) {
        for (const char* name : {"fib_recursive", "array_ops", "hashmap_ops", "class_method", "builtin_calls"}) {
            auto sp = bench::loadScenario(name);
            EXPECT_EQ(asInt(bench::runVm(sp)), asInt(bench::runVm(sp))) << name;
            EXPECT_EQ(asInt(bench::runEval(sp)), asInt(bench::runEval(sp))) << name;
        }
    }

    TEST(BenchScenarios, MissingScenarioThrows) {
        EXPECT_THROW(bench::loadScenario("no_such_scenario"), std::runtime_error);
    }

    TEST(BenchScenarios, ParseErrorThrows) {
        EXPECT_THROW(bench::prepare("만약 만약 만약\n"), std::runtime_error);
    }

    TEST(BenchScenarios, LexErrorThrows) {
        // 미종결 문자열 → lexer ParseError → runtime_error 변환 검증
        EXPECT_THROW(bench::prepare("\"미완성 문자열\n"), std::runtime_error);
    }

    TEST(BenchScenarios, FrontendSourceParses) {
        auto src     = bench::buildFrontendSource(10);
        auto program = bench::parseOnly(src);
        ASSERT_NE(program, nullptr);
        EXPECT_FALSE(program->statements.empty());
    }

} // namespace
