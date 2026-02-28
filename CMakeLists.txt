cmake_minimum_required(VERSION 3.28)
project(HongIk)

set(CMAKE_CXX_STANDARD 26)
# set(CMAKE_EXE_LINKER_FLAGS "-static")

# Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# For visual studio
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

enable_testing()

add_library(HongIkLib STATIC
        repl/repl.cpp
        utf8_converter/utf8_converter.cpp
        lexer/lexer.cpp
        token/token.cpp
        parser/parser.cpp
        evaluator/evaluator.cpp
        environment/environment.cpp
        object/built_in.cpp
)

add_executable(HongIk main.cpp)
target_link_libraries(HongIk HongIkLib)

add_executable(HongIkTests
        tests/lexer_test.cpp
        tests/utf8_converter_test.cpp
        tests/parser_test.cpp
        tests/evaluator_test.cpp
        tests/repl_test.cpp
)

# 헤더 파일 경로 추가
target_include_directories(HongIkTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}  # 프로젝트 루트 디렉토리
)

target_link_libraries(
        HongIkTests
        HongIkLib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(HongIkTests)
