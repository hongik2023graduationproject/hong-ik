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

// 결정성: 같은 ScenarioProgram으로 2회 실행해도 같은 결과 (벤치 반복 실행의 전제)
TEST(BenchScenarios, RepeatedRunsAreDeterministic) {
    auto sp = bench::loadScenario("fib_recursive");
    EXPECT_EQ(asInt(bench::runVm(sp)), asInt(bench::runVm(sp)));
    EXPECT_EQ(asInt(bench::runEval(sp)), asInt(bench::runEval(sp)));
}

TEST(BenchScenarios, MissingScenarioThrows) {
    EXPECT_THROW(bench::loadScenario("no_such_scenario"), std::runtime_error);
}

TEST(BenchScenarios, ParseErrorThrows) {
    EXPECT_THROW(bench::prepare("만약 만약 만약\n"), std::runtime_error);
}

TEST(BenchScenarios, FrontendSourceParses) {
    auto src = bench::buildFrontendSource(10);
    auto program = bench::parseOnly(src);
    ASSERT_NE(program, nullptr);
    EXPECT_FALSE(program->statements.empty());
}

} // namespace
