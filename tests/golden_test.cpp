#include "repl/repl.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef HONGIK_GOLDEN_FIXTURES_DIR
#error "HONGIK_GOLDEN_FIXTURES_DIR must be defined by CMake"
#endif

namespace {

std::string slurp(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::string("<MISSING FILE: ") + path.string() + ">";
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

std::string runFile(const std::filesystem::path& fixturePath, bool useVM) {
    std::ostringstream captured;
    std::streambuf* coutBackup = std::cout.rdbuf(captured.rdbuf());
    try {
        Repl repl(useVM);
        repl.FileMode(fixturePath.string());
    } catch (...) {
        std::cout.rdbuf(coutBackup);
        throw;
    }
    std::cout.rdbuf(coutBackup);
    return captured.str();
}

void runGolden(const std::string& name, bool useVM) {
    std::filesystem::path dir = HONGIK_GOLDEN_FIXTURES_DIR;
    auto source   = dir / (name + ".hik");
    auto expected = slurp(dir / (name + ".expected.txt"));
    auto actual   = runFile(source, useVM);
    EXPECT_EQ(expected, actual)
        << (useVM ? "VM" : "Evaluator") << " output diverged for fixture: " << name;
}

struct GoldenCase {
    std::string name;
};

class GoldenTest : public ::testing::TestWithParam<GoldenCase> {};

TEST_P(GoldenTest, Evaluator) { runGolden(GetParam().name, /*useVM=*/false); }
TEST_P(GoldenTest, VM)        { runGolden(GetParam().name, /*useVM=*/true); }

INSTANTIATE_TEST_SUITE_P(
    PhaseZeroGoldens, GoldenTest,
    ::testing::Values(
        GoldenCase{"arith_basic"},
        GoldenCase{"vars_types"},
        GoldenCase{"if_else"},
        GoldenCase{"loops_break"},
        GoldenCase{"func_recursion"},
        GoldenCase{"builtins"},
        GoldenCase{"utf8_strings"},
        GoldenCase{"error_undefined"},
        // S2 — VM unification parity coverage
        GoldenCase{"classes_basic"},
        GoldenCase{"closures"},
        GoldenCase{"for_each"},
        // 런타임 일관성 (2026-06-12 spec) — 시맨틱 통일 패리티
        GoldenCase{"decl_check"},
        GoldenCase{"try_catch"},
        GoldenCase{"ops_misc"}
    ),
    [](const ::testing::TestParamInfo<GoldenCase>& info) {
        return info.param.name;
    });

}  // namespace
