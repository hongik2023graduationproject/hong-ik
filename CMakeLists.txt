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
        # /fsanitize=address는 네이티브 빌드 시작부에서 전역으로 건다 (FetchContent 의존성과 LNK2038 방지).
        return()
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

# ----------------------------------------------------------------------------
# Windows 실행 파일 스택 크기
#
# 재귀 하강 파서·컴파일러·트리워킹 평가기는 AST 깊이만큼 C++ 스택을 쓴다. Windows 기본 스택
# (MSVC 1MB, MinGW 2MB)으로는 평가기 재귀 한도(1000)에 닿기 전에 스택이 넘쳐 프로세스가 죽는다.
# 16MB에서 MSVC Release·MSVC ASan·MinGW 모두 평가기 한도가 먼저 걸리고, VM은 식 깊이 5000 이상을 처리한다.
# 예약 크기라 실제 메모리는 쓰는 만큼만 잡힌다. Linux/macOS는 ulimit(기본 8MB)을 따른다.
# ----------------------------------------------------------------------------
set(HONGIK_STACK_BYTES 16777216 CACHE STRING "Windows 실행 파일의 스택 예약 크기(바이트)")

function(hongik_apply_stack_size target)
    if(MSVC)
        target_link_options(${target} PRIVATE "/STACK:${HONGIK_STACK_BYTES}")
    elseif(WIN32)
        target_link_options(${target} PRIVATE "-Wl,--stack,${HONGIK_STACK_BYTES}")
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
        object/builtin_registry.cpp
        object/builtins/math.cpp
        object/builtins/string.cpp
        object/builtins/array.cpp
        object/builtins/collection.cpp
        object/builtins/io.cpp
        object/builtins/conversion.cpp
        object/builtins/json.cpp
        vm/compiler.cpp
        vm/compiler_stmt.cpp
        vm/compiler_expr.cpp
        vm/vm.cpp
        vm/vm_handlers.cpp
        vm/peephole.cpp
        io/io_interface.cpp
        error/hongik_error.cpp
        analyzer/analyzer.cpp
        analyzer/type_checker.cpp
        analyzer/type_checker_expr.cpp
        analyzer/type_checker_class.cpp
        util/type.cpp
)

if(EMSCRIPTEN)
    # ==========================================
    # Emscripten WASM 빌드
    # ==========================================
    set(CMAKE_CXX_STANDARD 20)
    add_compile_options(-fexceptions)

    add_library(HongIkLib STATIC ${HONGIK_CORE_SOURCES})
    target_include_directories(HongIkLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # WASM 빌드는 링크 시점에만 -O3 -flto가 들어가 있었다.
    # 정적 라이브러리(HongIkLib) 컴파일에도 동일 최적화를 명시해 LTO/devirtualization 기회를 늘린다.
    target_compile_options(HongIkLib PRIVATE -fexceptions -O3 -flto)

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

    # For visual studio — 모든 타깃 정의보다 앞에 있어야 적용된다
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

    # MSVC ASan은 같은 링크 단위의 모든 오브젝트가 STL 주석(annotate_vector/annotate_string) 설정이 같아야 한다.
    # 타깃별로만 걸면 FetchContent로 받은 gtest·benchmark와 LNK2038이 나므로 MSVC에서는 전역으로 건다.
    # 실행 시 clang_rt.asan_dynamic-x86_64.dll(MSVC bin\Hostx64\x64)이 PATH에 있어야 한다.
    if(MSVC AND HONGIK_ENABLE_SANITIZERS AND "address" IN_LIST HONGIK_SANITIZERS)
        add_compile_options(/fsanitize=address)
    endif()

    # Google Test
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # ---- LSP 서버 (native 전용) ----
    FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)

    # ---- Google Benchmark (native 전용, 벤치마크 타깃용) ----
    FetchContent_Declare(
            googlebenchmark
            URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)

    set(HONGIK_LSP_SOURCES
            lsp/position.cpp
            lsp/jsonrpc.cpp
            lsp/document_store.cpp
            lsp/server.cpp
            lsp/features/diagnostics.cpp
            lsp/features/hover.cpp
            lsp/features/completion.cpp
            lsp/features/definition.cpp
            lsp/features/symbols.cpp
            analyzer/symbol_collector.cpp
    )
    add_library(HongIkLspLib STATIC ${HONGIK_LSP_SOURCES})
    target_include_directories(HongIkLspLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(HongIkLspLib PUBLIC HongIkLib nlohmann_json::nlohmann_json)
    hongik_apply_sanitizers(HongIkLspLib)

    add_executable(hongik-lsp lsp/main.cpp)
    target_link_libraries(hongik-lsp HongIkLspLib)
    hongik_apply_sanitizers(hongik-lsp)
    hongik_apply_stack_size(hongik-lsp)

    enable_testing()

    add_library(HongIkLib STATIC ${HONGIK_CORE_SOURCES})
    hongik_apply_sanitizers(HongIkLib)

    add_executable(HongIk main.cpp)
    target_link_libraries(HongIk HongIkLib)
    hongik_apply_sanitizers(HongIk)
    hongik_apply_stack_size(HongIk)

    add_executable(HongIkBenchmarks benchmarks/main.cpp benchmarks/runner.cpp)
    target_include_directories(HongIkBenchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(HongIkBenchmarks HongIkLib benchmark::benchmark)
    target_compile_definitions(HongIkBenchmarks PRIVATE
            "HONGIK_BENCH_SCENARIO_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/scenarios\"")
    hongik_apply_sanitizers(HongIkBenchmarks)
    hongik_apply_stack_size(HongIkBenchmarks)

    add_executable(HongIkTests
            tests/lexer_test.cpp
            tests/utf8_converter_test.cpp
            tests/parser_test.cpp
            tests/parser_diagnostics_test.cpp
            tests/evaluator_test.cpp
            tests/repl_test.cpp
            tests/vm_test.cpp
            tests/web_module_test.cpp
            tests/golden_test.cpp
            tests/type_checker_test.cpp
            tests/lsp_position_test.cpp
            tests/lsp_jsonrpc_test.cpp
            tests/lsp_document_store_test.cpp
            tests/lsp_server_test.cpp
            tests/symbol_collector_test.cpp
            tests/benchmark_scenarios_test.cpp
            tests/peephole_test.cpp
            tests/boundary_test.cpp
            tests/checked_int_test.cpp
            tests/expression_depth_test.cpp
            benchmarks/runner.cpp
            # WasmInterface는 EMSCRIPTEN 빌드의 일부이지만 클래스 자체는 plain C++.
            # 백엔드 parity 테스트에서 직접 인스턴스화하므로 native 테스트에서도 빌드한다.
            web/wasm_interface.cpp
    )

    # 헤더 파일 경로 추가
    target_include_directories(HongIkTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}  # 프로젝트 루트 디렉토리
    )

    target_compile_definitions(HongIkTests PRIVATE
            "HONGIK_GOLDEN_FIXTURES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden\""
            "HONGIK_BENCH_SCENARIO_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/scenarios\""
    )

    target_link_libraries(
            HongIkTests
            HongIkLib
            HongIkLspLib
            GTest::gtest_main
    )
    hongik_apply_sanitizers(HongIkTests)
    hongik_apply_stack_size(HongIkTests)

    include(GoogleTest)
    gtest_discover_tests(HongIkTests)
endif()
