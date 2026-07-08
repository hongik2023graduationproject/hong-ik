# 벤치마크 스위트 v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Google Benchmark 기반 성능 측정 인프라 — 런타임 시나리오 8종 × (VM/트리워킹) + 프론트엔드 2종, 정확성 스모크 테스트, CPython 등가 스크립트, 베이스라인 결과 문서.

**Architecture:** `benchmarks/runner.{h,cpp}`가 시나리오 로드·실행을 담당하고(HongIkTests와 HongIkBenchmarks가 공유), `benchmarks/main.cpp`가 Google Benchmark에 벤치를 등록한다. 시나리오는 `benchmarks/scenarios/*.hik` 실제 파일. 스펙: `docs/superpowers/specs/2026-07-08-benchmark-suite-design.md`.

**Tech Stack:** C++26, Google Benchmark v1.9.1 (FetchContent), GoogleTest(기존), CPython 3.14(수동 비교용).

## Global Constraints

- 벤치마크 이름·시나리오 파일명은 **ASCII** (Windows cp949 콘솔 사고 방지). 스크립트 내용은 한글.
- 새 CMake 타깃·FetchContent는 native 분기(else 절) 안, **MSVC `/utf-8` add_compile_options 이후**에 정의 (49bcff5 회귀 교훈 — 현재는 /utf-8이 분기 최상단이라 자연 충족되지만 순서를 바꾸지 말 것).
- WASM 분기(`if(EMSCRIPTEN)`) 무변경. 기존 536 테스트 무회귀.
- 코드 스타일: `.clang-format` (LLVM, 4칸, 120자). cmake/ctest는 PATH에 없음 — CLion 번들 사용: `/c/Program Files/JetBrains/CLion 2025.2.2/bin/cmake/win/x64/bin/cmake.exe` (ctest.exe 동일 디렉터리).
- 빌드/테스트: `cmake.exe --build cmake-build-debug --target <타깃>`, `ctest.exe --test-dir cmake-build-debug`.
- **언어 문법이 불확실하면 반드시 tests/vm_test.cpp·tests/evaluator_test.cpp에서 실제 사용례를 grep해 확인 후 작성** (배열 리터럴 `[]` 허용 여부, `추가`/`길이` 호출형이 `추가(a, v)`인지 `a.추가(v)`인지, 클래스 인스턴스 생성 구문 등). 스모크 기대값은 등가 Python으로 독립 계산해 교차 검증.
- 커밋 푸터: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`. 푸시는 하지 말 것(세션 리드가 수행).

---

### Task 1: runner 코어 + 시드 시나리오 2종 + 결정성 테스트

**Files:**
- Create: `benchmarks/runner.h`, `benchmarks/runner.cpp`
- Create: `benchmarks/scenarios/arith_loop.hik`, `benchmarks/scenarios/fib_recursive.hik`
- Create: `tests/benchmark_scenarios_test.cpp`
- Modify: `CMakeLists.txt` (native 분기 — HongIkTests 소스에 runner.cpp·테스트 추가, scenario dir 정의)

**Interfaces:**
- Produces (Task 2, 3이 사용):
  ```cpp
  // benchmarks/runner.h  (namespace bench)
  struct ScenarioProgram {
      std::shared_ptr<Program> program;              // 트리워킹용
      std::shared_ptr<CompiledFunction> bytecode;    // VM용 (Compile 1회, setup에서)
  };
  std::string loadScenarioSource(const std::string& name);   // scenarios/<name>.hik 원문, 실패 시 std::runtime_error
  ScenarioProgram prepare(const std::string& source);        // 파이프라인 전체, 파스 에러 시 std::runtime_error
  ScenarioProgram loadScenario(const std::string& name);     // loadScenarioSource + prepare
  std::shared_ptr<Object> runVm(const ScenarioProgram&);     // 매 호출 새 VM
  std::shared_ptr<Object> runEval(const ScenarioProgram&);   // 매 호출 새 Evaluator
  std::shared_ptr<Program> parseOnly(const std::string& source);  // 프론트엔드 벤치용 (렉스+파스만)
  std::string buildFrontendSource(int repeats);              // 대표 코드 블록 repeats회 반복
  ```

- [ ] **Step 1: 시드 시나리오 2종 작성**

`benchmarks/scenarios/arith_loop.hik`:
```
정수 합계 = 0
정수 i = 1
반복 i <= 1000000 동안:
    합계 += i
    i += 1
합계
```

`benchmarks/scenarios/fib_recursive.hik`:
```
함수 피보나치(정수 수) -> 정수:
    만약 수 <= 1 라면:
        리턴 수
    리턴 피보나치(수 - 1) + 피보나치(수 - 2)
피보나치(25)
```
(둘 다 마지막 줄 개행 포함으로 저장.)

- [ ] **Step 2: 실패하는 테스트 작성** — `tests/benchmark_scenarios_test.cpp`

```cpp
#include "benchmarks/runner.h"
#include "object/object.h"
#include <gtest/gtest.h>

namespace {

long long asInt(const std::shared_ptr<Object>& obj) {
    auto* i = dynamic_cast<Integer*>(obj.get());
    EXPECT_NE(i, nullptr) << "결과가 Integer가 아님: " << (obj ? obj->ToString() : "null");
    return i ? i->value : -1;
}

// 시나리오별 기대값 — 등가 Python으로 독립 계산한 값 (benchmarks/reference/ 참조)
TEST(BenchScenarios, ArithLoopVmAndEvalAgree) {
    auto sp = bench::loadScenario("arith_loop");
    EXPECT_EQ(asInt(bench::runVm(sp)), 500000500000LL);   // sum(1..1000000)
    EXPECT_EQ(asInt(bench::runEval(sp)), 500000500000LL);
}

TEST(BenchScenarios, FibRecursiveVmAndEvalAgree) {
    auto sp = bench::loadScenario("fib_recursive");
    EXPECT_EQ(asInt(bench::runVm(sp)), 75025LL);          // fib(25)
    EXPECT_EQ(asInt(bench::runEval(sp)), 75025LL);
}

// 결정성: 같은 ScenarioProgram으로 2회 실행해도 같은 결과 (벤치 반복 실행의 전제)
TEST(BenchScenarios, RepeatedRunsAreDeterministic) {
    auto sp = bench::loadScenario("fib_recursive");
    EXPECT_EQ(asInt(bench::runVm(sp)), asInt(bench::runVm(sp)));
    EXPECT_EQ(asInt(bench::runEval(sp)), asInt(bench::runEval(sp)));
}

TEST(BenchScenarios, MissingScenarioThrows) {
    EXPECT_THROW(bench::loadScenario("no_such_scenario"), std::runtime_error);
}

TEST(BenchScenarios, ParseErrorThrows) {
    EXPECT_THROW(bench::prepare("만약 만약 만약\n"), std::runtime_error);
}

TEST(BenchScenarios, FrontendSourceParses) {
    auto src = bench::buildFrontendSource(10);
    auto program = bench::parseOnly(src);
    ASSERT_NE(program, nullptr);
    EXPECT_FALSE(program->statements.empty());
}

} // namespace
```

- [ ] **Step 3: CMake 배선** — native 분기에서 HongIkTests 소스 목록에 `tests/benchmark_scenarios_test.cpp`와 `benchmarks/runner.cpp` 추가, 그리고 HongIkTests에:

```cmake
target_compile_definitions(HongIkTests PRIVATE
        HONGIK_BENCH_SCENARIO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/scenarios")
```

- [ ] **Step 4: 빌드해 테스트 실패 확인** (runner 미구현 → 컴파일 에러가 "실패" 신호)

- [ ] **Step 5: runner 구현** — `benchmarks/runner.cpp` (파이프라인은 `lsp/document_store.cpp:analyze`와 `tests/benchmark.cpp:parseSource` 준용):

```cpp
#include "runner.h"
#include "evaluator/evaluator.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "utf8_converter/utf8_converter.h"
#include "util/token_utils.h"
#include "vm/compiler.h"
#include "vm/vm.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace bench {

std::string loadScenarioSource(const std::string& name) {
    std::string path = std::string(HONGIK_BENCH_SCENARIO_DIR) + "/" + name + ".hik";
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("시나리오 파일을 열 수 없음: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::shared_ptr<Program> parseOnly(const std::string& source) {
    std::string src = source;
    if (src.empty() || src.back() != '\n') src += '\n';
    Lexer lexer;
    auto tokens = lexer.Tokenize(Utf8Converter::Convert(src));
    token_utils::appendMissingBlockClosers(tokens);
    Parser parser;
    auto program = parser.Parsing(tokens);
    if (!parser.getErrors().empty()) {
        throw std::runtime_error("시나리오 파스 에러: " + parser.getErrors().front());
    }
    return program;
}

ScenarioProgram prepare(const std::string& source) {
    ScenarioProgram sp;
    sp.program = parseOnly(source);
    Compiler compiler;
    sp.bytecode = compiler.Compile(sp.program);
    return sp;
}

ScenarioProgram loadScenario(const std::string& name) { return prepare(loadScenarioSource(name)); }

std::shared_ptr<Object> runVm(const ScenarioProgram& sp) {
    VM vm;
    return vm.Execute(sp.bytecode);
}

std::shared_ptr<Object> runEval(const ScenarioProgram& sp) {
    Evaluator evaluator;
    return evaluator.Evaluate(sp.program);
}

std::string buildFrontendSource(int repeats) {
    // 함수·클래스·루프·조건이 섞인 대표 블록 — 프론트엔드가 실제로 만나는 코드 분포 근사
    static const std::string kBlock =
        "함수 계산기(정수 a, 정수 b) -> 정수:\n"
        "    정수 결과 = a * b + a - b\n"
        "    만약 결과 > 100 라면:\n"
        "        리턴 결과 % 100\n"
        "    리턴 결과\n"
        "클래스 저장소:\n"
        "    정수 값\n"
        "    생성(정수 초기):\n"
        "        자기.값 = 초기\n"
        "    함수 읽기() -> 정수:\n"
        "        리턴 자기.값\n"
        "정수 누적 = 0\n"
        "정수 n = 0\n"
        "반복 n < 10 동안:\n"
        "    누적 += 계산기(n, n + 1)\n"
        "    n += 1\n";
    std::string out;
    out.reserve(kBlock.size() * repeats);
    for (int i = 0; i < repeats; i++) out += kBlock;
    return out;
}

} // namespace bench
```

`benchmarks/runner.h`:
```cpp
#ifndef HONGIK_BENCH_RUNNER_H
#define HONGIK_BENCH_RUNNER_H

#include "ast/node.h"
#include "object/object.h"
#include "vm/chunk.h"
#include <memory>
#include <string>

namespace bench {

struct ScenarioProgram {
    std::shared_ptr<Program> program;
    std::shared_ptr<CompiledFunction> bytecode;
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
```
(주의: `Program`/`CompiledFunction` 실제 정의 헤더는 코드베이스에서 확인 — `Program`은 `ast/`의 정의 헤더, `CompiledFunction`은 `vm/chunk.h`. include가 다르면 맞게 수정. `buildFrontendSource`의 문법 요소도 tests에서 실사용 확인 후 필요 시 조정.)

- [ ] **Step 6: 빌드 + 테스트 통과 확인**

Run: `cmake.exe --build cmake-build-debug --target HongIkTests && ctest.exe --test-dir cmake-build-debug -R BenchScenarios --output-on-failure`
Expected: 6개 테스트 PASS. **결정성 테스트가 실패하면**(Evaluator가 AST를 변형하는 경우): `ScenarioProgram`에 `std::string source`를 보관하고 `runEval`이 매 호출 `parseOnly(sp.source)`로 재파싱하도록 변경 + 그 사실을 runner.h 주석에 기록 (측정 결과 문서에도 명시해야 하므로).

- [ ] **Step 7: 전체 테스트 무회귀 확인**

Run: `ctest.exe --test-dir cmake-build-debug --output-on-failure`
Expected: 전체 PASS (536 + 신규 6).

- [ ] **Step 8: Commit** — `feat(bench): scenario runner core + seed scenarios`

### Task 2: 나머지 런타임 시나리오 6종 + 스모크 기대값

**Files:**
- Create: `benchmarks/scenarios/string_concat.hik`, `string_index.hik`, `array_ops.hik`, `hashmap_ops.hik`, `class_method.hik`, `builtin_calls.hik`
- Modify: `tests/benchmark_scenarios_test.cpp` (시나리오별 TEST 추가)

**Interfaces:**
- Consumes: Task 1의 `bench::loadScenario/runVm/runEval`
- Produces: Task 3이 등록할 시나리오 이름 8종 전체 — `arith_loop, fib_recursive, string_concat, string_index, array_ops, hashmap_ops, class_method, builtin_calls`

- [ ] **Step 1: 문법 실사용 확인** — 아래 각 구문을 tests/vm_test.cpp·tests/evaluator_test.cpp에서 grep으로 확인하고, 없는 구문은 사용하지 말 것: 빈 배열 리터럴 `[]`, `추가`/`길이` 호출형(자유함수형 vs 메서드형), 문자열 `+=`, 문자열 인덱싱 `s[i]`, `%` 연산, 클래스 인스턴스 생성 구문, 사전 리터럴/인덱스 대입.

- [ ] **Step 2: 시나리오 초안 작성** (아래는 초안 — Step 1 확인 결과에 맞게 조정하고, 조정 시 기대값도 재계산):

`string_concat.hik` (기대: 5000):
```
문자 s = ""
정수 i = 0
반복 i < 5000 동안:
    s = s + "가"
    i += 1
길이(s)
```

`string_index.hik` (기대: 20000 — 5자 패턴에서 i%5==0 위치만 "가"):
```
문자 s = "가나다라마"
정수 개수 = 0
정수 i = 0
반복 i < 100000 동안:
    만약 s[i % 5] == "가" 라면:
        개수 += 1
    i += 1
개수
```

`array_ops.hik` (기대: 49995000 = sum(0..9999)):
```
배열 a = []
정수 i = 0
반복 i < 10000 동안:
    추가(a, i)
    i += 1
정수 합 = 0
정수 j = 0
반복 j < 10000 동안:
    합 += a[j]
    j += 1
합
```

`hashmap_ops.hik` (기대: 600030000 — Python으로 교차 계산할 것):
```
사전 d = {"a": 0, "b": 0, "c": 0}
정수 합 = 0
정수 i = 0
반복 i < 20000 동안:
    d["a"] = i
    d["b"] = i + 1
    d["c"] = i + 2
    합 += d["a"] + d["b"] + d["c"]
    i += 1
합
```

`class_method.hik` (기대: 1250025000 = sum(1..50000)) — 인스턴스 생성 구문은 Step 1 확인 결과로 대체:
```
클래스 계수기:
    정수 값
    생성(정수 시작):
        자기.값 = 시작
    함수 증가() -> 정수:
        자기.값 += 1
        리턴 자기.값
정수 총 = 0
정수 i = 0
반복 i < 50000 동안:
    계수기 c = 계수기(i)
    총 += c.증가()
    i += 1
총
```

`builtin_calls.hik` (기대: 500000):
```
배열 a = [1, 2, 3, 4, 5]
정수 합 = 0
정수 i = 0
반복 i < 100000 동안:
    합 += 길이(a)
    i += 1
합
```

- [ ] **Step 3: 시나리오별 실패 테스트 추가** — Task 1의 패턴 그대로, 시나리오당 하나:

```cpp
TEST(BenchScenarios, StringConcat) {
    auto sp = bench::loadScenario("string_concat");
    EXPECT_EQ(asInt(bench::runVm(sp)), 5000LL);
    EXPECT_EQ(asInt(bench::runEval(sp)), 5000LL);
}
// string_index=20000, array_ops=49995000, hashmap_ops=600030000,
// class_method=1250025000, builtin_calls=500000 — 동일 패턴으로 6개
```

- [ ] **Step 4: 빌드·테스트 → 조정 반복** — 파스 에러/기대값 불일치 시 Step 1 확인 결과로 시나리오를 고치고 기대값 재계산 (기대값은 반드시 Python 한 줄 계산으로 교차 검증, 예: `sum(range(10000))`).

Run: `ctest.exe --test-dir cmake-build-debug -R BenchScenarios --output-on-failure`
Expected: 12개 전부 PASS. 트리워킹이 수십 초 이상 걸리는 시나리오가 있으면 워크로드를 줄이고 기대값 갱신 (스모크는 CI에서도 돌므로 시나리오당 몇 초 이내 목표).

- [ ] **Step 5: 전체 테스트 무회귀 + Commit** — `feat(bench): 6 runtime scenarios + correctness smoke tests`

### Task 3: Google Benchmark 타깃 + HongIkBench 대체

**Files:**
- Create: `benchmarks/main.cpp`
- Modify: `CMakeLists.txt` (FetchContent + HongIkBenchmarks 타깃)
- Delete: `tests/benchmark.cpp` (+ CMakeLists의 HongIkBench 타깃 제거)

**Interfaces:**
- Consumes: Task 1·2의 runner API와 시나리오 8종 이름

- [ ] **Step 1: CMake** — native 분기, nlohmann_json FetchContent 옆에 추가:

```cmake
FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)
```

HongIkBench 타깃 정의(add_executable(HongIkBench ...) 3줄)를 삭제하고 그 자리에:

```cmake
add_executable(HongIkBenchmarks benchmarks/main.cpp benchmarks/runner.cpp)
target_include_directories(HongIkBenchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(HongIkBenchmarks HongIkLib benchmark::benchmark)
target_compile_definitions(HongIkBenchmarks PRIVATE
        HONGIK_BENCH_SCENARIO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/scenarios")
hongik_apply_sanitizers(HongIkBenchmarks)
```

`git rm tests/benchmark.cpp`.

- [ ] **Step 2: main.cpp**

```cpp
#include "runner.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "utf8_converter/utf8_converter.h"
#include "util/token_utils.h"
#include "vm/compiler.h"
#include <benchmark/benchmark.h>
#include <memory>
#include <string>
#include <vector>

namespace {

const std::vector<std::string> kScenarios = {
    "arith_loop", "fib_recursive", "string_concat", "string_index",
    "array_ops",  "hashmap_ops",   "class_method",  "builtin_calls",
};

void registerAll() {
    for (const auto& name : kScenarios) {
        auto sp = std::make_shared<bench::ScenarioProgram>(bench::loadScenario(name)); // 실패 시 throw → 즉시 종료
        benchmark::RegisterBenchmark(("vm/" + name).c_str(), [sp](benchmark::State& st) {
            for (auto _ : st) benchmark::DoNotOptimize(bench::runVm(*sp));
        })->Unit(benchmark::kMillisecond);
        benchmark::RegisterBenchmark(("eval/" + name).c_str(), [sp](benchmark::State& st) {
            for (auto _ : st) benchmark::DoNotOptimize(bench::runEval(*sp));
        })->Unit(benchmark::kMillisecond);
    }

    auto src = std::make_shared<std::string>(bench::buildFrontendSource(100));
    benchmark::RegisterBenchmark("frontend/parse", [src](benchmark::State& st) {
        for (auto _ : st) benchmark::DoNotOptimize(bench::parseOnly(*src));
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
    registerAll();
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
```

- [ ] **Step 3: 빌드 + 스모크 실행**

Run: `cmake.exe --build cmake-build-debug --target HongIkBenchmarks && ./cmake-build-debug/HongIkBenchmarks --benchmark_filter=vm/arith_loop`
Expected: 표 형식 결과 1행 출력, 에러 없음.

- [ ] **Step 4: 캘리브레이션** — 전체 1회 실행(`./cmake-build-debug/HongIkBenchmarks`)해 확인: ① VM 시나리오 iteration당 1ms 미만이면 워크로드 상향(기대값·스모크 갱신), ② eval 포함 전체 러닝타임이 5분을 크게 넘으면 해당 시나리오 하향. 조정 시 Task 2 테스트 기대값 동기화 필수.

- [ ] **Step 5: 전체 테스트 무회귀 확인 + Commit** — `feat(bench): Google Benchmark suite replaces HongIkBench`

### Task 4: CPython 등가 스크립트 + README

**Files:**
- Create: `benchmarks/reference/arith_loop.py` 외 7종 (시나리오와 1:1, 최종 확정된 워크로드 크기와 동일하게)
- Create: `benchmarks/README.md`

**Interfaces:**
- Consumes: Task 2·3에서 확정된 시나리오 8종의 최종 워크로드 크기 (.hik 파일이 단일 진실)

- [ ] **Step 1: 등가 .py 작성** — 각 파일은 동일 워크로드를 수행하고 결과와 경과 ms를 출력. 패턴:

```python
import time

def main() -> int:
    total = 0
    i = 1
    while i <= 1_000_000:   # .hik와 동일한 while 루프 구조 유지 (for/sum 최적화 금지 — 등가성)
        total += i
        i += 1
    return total

start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
```

8종 모두: 결과값이 .hik 스모크 기대값과 동일해야 함 (등가성 증명). `class_method.py`는 실제 class/인스턴스 생성, `hashmap_ops.py`는 dict, `string_index.py`는 str 인덱싱으로 — 관용구 최적화 없이 구조 등가로.

- [ ] **Step 2: 결과값 일치 확인**

Run: `for f in benchmarks/reference/*.py; do python "$f"; done`
Expected: 각 result= 값이 tests/benchmark_scenarios_test.cpp의 기대값과 일치.

- [ ] **Step 3: README.md** — 내용: 빌드·실행 명령, `--benchmark_filter`/`--benchmark_format=json`/`--benchmark_repetitions` 사용 예, JSON 두 개를 비교하는 GBench `compare.py` 안내(구글 벤치마크 저장소 tools/), reference/ 실행법과 "수동 1회 측정, 스위트 미포함" 원칙, 시나리오 추가 시 체크리스트(.hik + 스모크 기대값 + .py 등가 + main.cpp 목록).

- [ ] **Step 4: Commit** — `docs(bench): CPython reference scripts + usage README`

### Task 5: 베이스라인 측정 + 결과 문서 (세션 리드 직접 수행)

**Files:**
- Create: `docs/benchmarks/2026-07-08-baseline.md`

- [ ] **Step 1: 측정** — 백그라운드 앱 종료 후: `./cmake-build-debug/HongIkBenchmarks --benchmark_repetitions=5 --benchmark_format=json > docs/benchmarks/2026-07-08-baseline.json` (Debug가 아닌 Release 빌드 디렉터리가 따로 있으면 그쪽 사용 — 없으면 `cmake.exe -B cmake-build-release -DCMAKE_BUILD_TYPE=Release` 후 빌드해 사용. **최적화 빌드로 측정할 것**)
- [ ] **Step 2: CPython 수치** — reference/ 8종 각 5회 실행, 중앙값 기록.
- [ ] **Step 3: 문서 작성** — 표(시나리오 × VM/Eval/CPython 중앙값 ms), VM/Eval 비율, 관찰(어느 축이 상대적으로 느린가), 다음 최적화 후보(Layer 2 peephole 등)와 예상 효과, 측정 환경(CPU/OS/컴파일러) 명기.
- [ ] **Step 4: Commit** — `docs(bench): 2026-07-08 baseline measurements`

## Self-Review 결과

- 스펙 D1~D7 전부 태스크에 매핑됨 (D1→T3, D2→T1·2, D3→T3·5, D4→T1·2, D5→T1·2, D6→T3, D7→CMake 배치 제약).
- 플레이스홀더 없음. 문법 불확실 지점은 "기존 테스트에서 확인" 지시 + 조정 시 기대값 재계산 규칙으로 처리.
- 타입 일관성: `bench::` API는 T1 정의를 T2·3이 그대로 소비. `CompiledFunction` 헤더 위치만 구현 시 확인 항목으로 명시.
