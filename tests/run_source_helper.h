// 테스트 공용: 소스 문자열을 파일 모드(Repl::FileMode) 그대로 실행해 표준 출력을 돌려준다.
// 타입 검사는 strict로 돌려서, 타입 진단이 하나라도 나면 exitCode가 1이 된다 (거짓 경고 검출용).
#ifndef HONGIK_TESTS_RUN_SOURCE_HELPER_H
#define HONGIK_TESTS_RUN_SOURCE_HELPER_H

#include "repl/repl.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <string>

namespace test_helper {

    struct RunOutput {
        std::string output;
        int exitCode = 0;
    };

    inline RunOutput runSourceFile(const std::string& source, bool useVM, TypeCheckMode mode = TypeCheckMode::Strict) {
        static int counter = 0;
        const auto* info   = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string stem   = std::string("hongik_") + (info ? info->test_suite_name() : "t") + "_"
                         + (info ? info->name() : "x") + "_" + std::to_string(counter++) + ".hik";
        for (auto& c : stem) {
            if (c == '/') {
                c = '_';
            }
        }
        auto path = std::filesystem::temp_directory_path() / stem;
        {
            std::ofstream out(path, std::ios::binary);
            out << source;
        }
        std::ostringstream captured;
        std::streambuf* coutBackup = std::cout.rdbuf(captured.rdbuf());
        RunOutput result;
        try {
            Repl repl(useVM, mode);
            result.exitCode = repl.FileMode(path.string());
        } catch (...) {
            std::cout.rdbuf(coutBackup);
            std::filesystem::remove(path);
            throw;
        }
        std::cout.rdbuf(coutBackup);
        std::filesystem::remove(path);
        result.output = captured.str();
        return result;
    }

    // 두 백엔드(VM·트리워킹)가 같은 출력을 내고, 그 출력이 expected이며, strict 타입 검사도 통과하는지 확인한다.
    inline void expectSameOutputOnBoth(const std::string& source, const std::string& expected) {
        auto vm   = runSourceFile(source, /*useVM=*/true);
        auto eval = runSourceFile(source, /*useVM=*/false);
        EXPECT_EQ(vm.output, expected) << "VM 출력이 다름:\n" << source;
        EXPECT_EQ(eval.output, expected) << "트리워킹 출력이 다름:\n" << source;
        EXPECT_EQ(vm.exitCode, 0) << "strict 타입 검사 진단이 남:\n" << source;
    }

} // namespace test_helper

#endif // HONGIK_TESTS_RUN_SOURCE_HELPER_H
