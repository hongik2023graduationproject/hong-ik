#ifndef HONGIK_BENCH_RUNNER_H
#define HONGIK_BENCH_RUNNER_H

#include "ast/program.h"
#include "object/object.h"
#include "vm/chunk.h"
#include <memory>
#include <string>

namespace bench {

    struct ScenarioProgram {
        std::shared_ptr<Program> program; // 트리워킹용
        std::shared_ptr<CompiledFunction> bytecode; // VM용 (Compile 1회, setup에서)
    };

    std::string loadScenarioSource(const std::string& name);
    ScenarioProgram prepare(const std::string& source);
    ScenarioProgram loadScenario(const std::string& name);
    std::shared_ptr<Object> runVm(const ScenarioProgram& sp);
    std::shared_ptr<Object> runEval(const ScenarioProgram& sp);
    std::shared_ptr<Program> parseOnly(const std::string& source);
    std::string buildFrontendSource(int repeats);

} // namespace bench

#endif // HONGIK_BENCH_RUNNER_H
