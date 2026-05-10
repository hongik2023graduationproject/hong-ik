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

struct GoldenCase {
    std::string name;
};

class GoldenTest : public ::testing::TestWithParam<GoldenCase> {};

TEST_P(GoldenTest, Evaluator) {
    const auto& c = GetParam();
    std::filesystem::path dir = HONGIK_GOLDEN_FIXTURES_DIR;
    auto source   = dir / (c.name + ".hik");
    auto expected = slurp(dir / (c.name + ".expected.txt"));
    auto actual   = runFile(source, /*useVM=*/false);
    EXPECT_EQ(expected, actual)
        << "Evaluator output diverged for fixture: " << c.name;
}

TEST_P(GoldenTest, VM) {
    const auto& c = GetParam();
    std::filesystem::path dir = HONGIK_GOLDEN_FIXTURES_DIR;
    auto source   = dir / (c.name + ".hik");
    auto expected = slurp(dir / (c.name + ".expected.txt"));
    auto actual   = runFile(source, /*useVM=*/true);
    EXPECT_EQ(expected, actual)
        << "VM output diverged for fixture: " << c.name;
}

INSTANTIATE_TEST_SUITE_P(
    PhaseZeroGoldens, GoldenTest,
    ::testing::Values(
        GoldenCase{"arith_basic"}
    ),
    [](const ::testing::TestParamInfo<GoldenCase>& info) {
        return info.param.name;
    });

}  // namespace
