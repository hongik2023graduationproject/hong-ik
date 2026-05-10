cmake_minimum_required(VERSION 3.28)
project(HongIk)

# set(CMAKE_EXE_LINKER_FLAGS "-static")

# ----------------------------------------------------------------------------
# Sanitizer support
#
# Enable with: cmake -B build -DHONGIK_ENABLE_SANITIZERS=ON
# Pick a subset via HONGIK_SANITIZERS (default: "address;undefined").
# TSan and MSan are mutually exclusive with ASan, so don't combine them.
# Skipped automatically for Emscripten/WASM builds (no sanitizer runtime).
# ----------------------------------------------------------------------------
option(HONGIK_ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
set(HONGIK_SANITIZERS "address;undefined" CACHE STRING "Semicolon-separated sanitizer list (e.g. address;undefined or thread)")

function(hongik_apply_sanitizers target)
    if(NOT HONGIK_ENABLE_SANITIZERS OR EMSCRIPTEN)
        return()
    endif()

    set(_flags "")
    if(MSVC)
        # MSVC supports ASan only; UBSan/TSan/MSan are unavailable.
        list(FIND HONGIK_SANITIZERS "address" _has_asan)
        if(_has_asan GREATER -1)
            list(APPEND _flags /fsanitize=address)
        endif()
    else()
        foreach(_san IN LISTS HONGIK_SANITIZERS)
            list(APPEND _flags -fsanitize=${_san})
        endforeach()
        list(APPEND _flags -fno-omit-frame-pointer -fno-sanitize-recover=all)
    endif()

    if(_flags)
        target_compile_options(${target} PRIVATE ${_flags})
        if(NOT MSVC)
            target_link_options(${target} PRIVATE ${_flags})
        endif()
    endif()
endfunction()

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
    hongik_apply_sanitizers(HongIkLib)

    add_executable(HongIk main.cpp)
    target_link_libraries(HongIk HongIkLib)
    hongik_apply_sanitizers(HongIk)

    add_executable(HongIkBench tests/benchmark.cpp)
    target_include_directories(HongIkBench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(HongIkBench HongIkLib)
    hongik_apply_sanitizers(HongIkBench)

    add_executable(HongIkTests
            tests/lexer_test.cpp
            tests/utf8_converter_test.cpp
            tests/parser_test.cpp
            tests/evaluator_test.cpp
            tests/repl_test.cpp
            tests/vm_test.cpp
            tests/web_module_test.cpp
            tests/golden_test.cpp
    )

    # 헤더 파일 경로 추가
    target_include_directories(HongIkTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}  # 프로젝트 루트 디렉토리
    )

    target_compile_definitions(HongIkTests PRIVATE
            "HONGIK_GOLDEN_FIXTURES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden\""
    )

    target_link_libraries(
            HongIkTests
            HongIkLib
            GTest::gtest_main
    )
    hongik_apply_sanitizers(HongIkTests)

    include(GoogleTest)
    gtest_discover_tests(HongIkTests)
endif()
