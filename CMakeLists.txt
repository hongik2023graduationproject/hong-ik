cmake_minimum_required(VERSION 3.28)
project(HongIk)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXE_LINKER_FLAGS "-static")

# Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(HongIk main.cpp
        repl/repl.cpp
        repl/repl.h
        utf8_converter/utf8_converter.cpp
        utf8_converter/utf8_converter.h
        lexer/lexer.cpp
        lexer/lexer.h
        token/token.h
        token/token.cpp
        token/token_type.h
        parser/parser.cpp
        parser/parser.h
        ast/program.h
        ast/node.h
        ast/statements.h
        ast/literals.h
        ast/expressions.h
        evaluator/evaluator.cpp
        evaluator/evaluator.h
        object/object.h
        object/object_type.h
        environment/environment.h
        environment/environment.cpp
        object/built_in.cpp
        object/built_in.h
        exception/exception.h
)

add_library(HongIkLib STATIC
        utf8_converter/utf8_converter.cpp
        lexer/lexer.cpp
        parser/parser.cpp
        token/token_type.h
        token/token.cpp
        exception/exception.h
        ast/node.h
        ast/program.h
        ast/statements.h
        ast/expressions.h
        ast/literals.h
)

add_executable(HongIkTests
        tests/lexer_test.cpp
        tests/utf8_converter_test.cpp
        tests/parser_test.cpp
        tests/evaluator_test.cpp
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