#include "runner.h"
#include "vm/compiler.h"
#include <benchmark/benchmark.h>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

    const std::vector<std::string> kScenarios = {
        "arith_loop",
        "fib_recursive",
        "string_concat",
        "string_index",
        "array_ops",
        "hashmap_ops",
        "class_method",
        "builtin_calls",
    };

    void registerAll() {
        for (const auto& name : kScenarios) {
            // 파싱·컴파일은 setup에서 1회, 실행만 측정. 로드 실패 시 throw → 즉시 종료.
            auto sp = std::make_shared<bench::ScenarioProgram>(bench::loadScenario(name));
            benchmark::RegisterBenchmark(("vm/" + name).c_str(), [sp](benchmark::State& st) {
                for (auto _ : st) {
                    benchmark::DoNotOptimize(bench::runVm(*sp));
                }
            })->Unit(benchmark::kMillisecond);
            benchmark::RegisterBenchmark(("eval/" + name).c_str(), [sp](benchmark::State& st) {
                for (auto _ : st) {
                    benchmark::DoNotOptimize(bench::runEval(*sp));
                }
            })->Unit(benchmark::kMillisecond);
        }

        // 프론트엔드: 소스 텍스트를 setup으로 두고 렉스+파스, 컴파일을 각각 측정.
        auto src = std::make_shared<std::string>(bench::buildFrontendSource(100));
        benchmark::RegisterBenchmark("frontend/parse", [src](benchmark::State& st) {
            for (auto _ : st) {
                benchmark::DoNotOptimize(bench::parseOnly(*src));
            }
        })->Unit(benchmark::kMillisecond);

        auto parsed = std::make_shared<bench::ScenarioProgram>(bench::prepare(*src));
        benchmark::RegisterBenchmark("frontend/compile", [parsed](benchmark::State& st) {
            for (auto _ : st) {
                Compiler compiler;
                benchmark::DoNotOptimize(compiler.Compile(parsed->program));
            }
        })->Unit(benchmark::kMillisecond);
    }

} // namespace

int main(int argc, char** argv) {
    try {
        registerAll();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "벤치마크 등록 실패: %s\n", e.what());
        return 1;
    }
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
