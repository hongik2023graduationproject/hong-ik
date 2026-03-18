cmake_minimum_required(VERSION 3.28)
project(HongIk)

# set(CMAKE_EXE_LINKER_FLAGS "-static")

# 공통 소스 (네이티브 + WASM 모두 사용)
set(HONGIK_CORE_SOURCES
        repl/repl.cpp
        utf8_converter/utf8_converter.cpp
        lexer/lexer.cpp
        token/token.cpp
        parser/parser.cpp
        evaluator/evaluator.cpp
        environment/environment.cpp
        object/built_in.cpp
        vm/compiler.cpp
        vm/vm.cpp
        io/io_interface.cpp
        error/hongik_error.cpp
        analyzer/analyzer.cpp
)

if(EMSCRIPTEN)
    # ==========================================
    # Emscripten WASM 빌드
    # ==========================================
    set(CMAKE_CXX_STANDARD 20)
    add_compile_options(-fexceptions)

    add_library(HongIkLib STATIC ${HONGIK_CORE_SOURCES})
    target_include_directories(HongIkLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(HongIkLib PRIVATE -fexceptions)

    # WASM 타겟
    add_executable(hongik-wasm
            web/wasm_interface.cpp
            web/bindings.cpp
    )
    target_include_directories(hongik-wasm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(hongik-wasm HongIkLib)

    # Emscripten 링크 플래그
    set(WASM_LINK_FLAGS
        "--bind"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=HongIkModule"
        "-sENVIRONMENT=web,worker"
        "-sNO_EXIT_RUNTIME=1"
        "-sDISABLE_EXCEPTION_CATCHING=0"
        "-O3"
        "-flto"
    )
    target_link_options(hongik-wasm PRIVATE ${WASM_LINK_FLAGS})
    set_target_properties(hongik-wasm PROPERTIES SUFFIX ".js")

else()
    # ==========================================
    # 네이티브 빌드 (기존)
    # ==========================================
    set(CMAKE_CXX_STANDARD 26)

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

    add_library(HongIkLib STATIC ${HONGIK_CORE_SOURCES})

    add_executable(HongIk main.cpp)
    target_link_libraries(HongIk HongIkLib)

    add_executable(HongIkBench tests/benchmark.cpp)
    target_include_directories(HongIkBench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(HongIkBench HongIkLib)

    add_executable(HongIkTests
            tests/lexer_test.cpp
            tests/utf8_converter_test.cpp
            tests/parser_test.cpp
            tests/evaluator_test.cpp
            tests/repl_test.cpp
            tests/vm_test.cpp
            tests/web_module_test.cpp
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
endif()
