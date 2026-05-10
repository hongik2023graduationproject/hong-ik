# Phase 0 — 안전망 구축 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** hong-ik 구조 리팩토링의 회귀 안전망 구축 — 골든 출력 통합 테스트(두 백엔드 동시 검증), WASM/Sanitizer 빌드 기준치 기록, 죽은 코드 의심 항목 확인.

**Architecture:** 새 gtest 파일 `tests/golden_test.cpp` 가 디스크의 fixture 쌍(`*.hik` + `*.expected.txt`)을 읽고, 기존 `Repl::FileMode()` 진입점을 통해 두 백엔드(`useVM=false` 트리워킹, `useVM=true` VM)로 각각 실행한 뒤 캡처한 stdout을 골든과 정확히 비교한다. fixture 디렉토리 위치는 CMake 컴파일 타임 정의로 전달.

**Tech Stack:** C++26, GoogleTest 1.14+, CMake 3.28+, Emscripten (WASM), AddressSanitizer + UndefinedBehaviorSanitizer.

**참고 스펙:** [`docs/superpowers/specs/2026-05-10-hong-ik-structure-refactor-design.md`](../specs/2026-05-10-hong-ik-structure-refactor-design.md) §5 Phase 0, §6.

---

## File Structure

**Create:**
- `tests/golden_test.cpp` — gtest harness, fixture를 두 백엔드로 실행 후 stdout 비교
- `tests/fixtures/golden/arith_basic.hik` + `.expected.txt`
- `tests/fixtures/golden/vars_types.hik` + `.expected.txt`
- `tests/fixtures/golden/if_else.hik` + `.expected.txt`
- `tests/fixtures/golden/loops_break.hik` + `.expected.txt`
- `tests/fixtures/golden/func_recursion.hik` + `.expected.txt`
- `tests/fixtures/golden/builtins.hik` + `.expected.txt`
- `tests/fixtures/golden/utf8_strings.hik` + `.expected.txt`
- `tests/fixtures/golden/error_undefined.hik` + `.expected.txt`
- `docs/superpowers/plans/2026-05-10-phase-0-baseline.md` — Phase 0 종료 시 기록되는 기준치 (WASM 산출물 사이즈 등)

**Modify:**
- `CMakeLists.txt` — `golden_test.cpp` 를 `HongIkTests` 소스 리스트에 추가, fixture 경로 컴파일 정의 부여
- `.github/workflows/build.yml` — golden 테스트 + WASM 빌드 게이트 확인 (변경 없을 수 있음, Task 4 에서 점검)

**Untouched:** 기존 `lexer/`, `parser/`, `evaluator/`, `vm/`, `repl/`, `web/`, `sandbox/` 등 모든 프로덕션 코드. **Phase 0 은 코드 이동/변경 없음** — 테스트와 검증만.

---

## Task 1: 베이스라인 빌드 + 테스트 그린 확인 / 죽은 코드 검사

**Files:**
- 변경 없음 (확인 작업)

이 태스크는 모든 후속 작업의 전제조건이다. 깨진 베이스라인 위에 테스트를 더 쌓으면 회귀를 못 잡는다.

- [ ] **Step 1: 빌드 디렉토리에서 cmake configure**

```powershell
cmake -B cmake-build-debug
```

Expected: 마지막 줄에 `-- Generating done` + `-- Build files have been written to: .../cmake-build-debug`. 경고는 OK, 에러 없음.

- [ ] **Step 2: 전체 빌드**

```powershell
cmake --build cmake-build-debug
```

Expected: `HongIk`, `HongIkLib`, `HongIkTests`, `HongIkBench` 4 타겟 모두 성공. 마지막에 빌드 에러 없음.

- [ ] **Step 3: 전체 테스트 실행**

```powershell
ctest --test-dir cmake-build-debug --output-on-failure
```

Expected: `100% tests passed`. 만약 실패 테스트가 있다면 — **여기서 멈추고 사용자에게 보고**. 깨진 테스트 위에 Phase 0 을 쌓지 않음.

- [ ] **Step 4: `web/memory_filesystem.h` 사용처 확인**

```powershell
# 프로젝트 루트(hong-ik)에서, 빌드 산출물 제외하고 검색
git grep -l memory_filesystem -- ':!build*' ':!cmake-build-*'
```

Expected: 최소 `web/wasm_interface.h` 가 나와야 함. 그 외 사용처가 없어도 정상 — 헤더는 wasm_interface 를 통해 전이 의존됨. 결론: **죽은 코드 아님, 그대로 둠**. (만약 grep 결과가 비었다면 헤더 자체가 미사용이므로 별도 이슈로 보고하고 제거 — 이번 plan 범위 외.)

- [ ] **Step 5: 결과 기록 (커밋 없음, 다음 태스크 진행 준비)**

이 태스크는 외부 산출물을 만들지 않는다. 모든 것이 그린이면 다음 태스크로.

---

## Task 2: 골든 테스트 하니스 + 첫 fixture (smoke test)

**Files:**
- Create: `tests/golden_test.cpp`
- Create: `tests/fixtures/golden/arith_basic.hik`
- Create: `tests/fixtures/golden/arith_basic.expected.txt`
- Modify: `CMakeLists.txt:130-138` (HongIkTests 소스 리스트에 추가)

목표: 하나의 fixture가 두 백엔드 모두에서 그린이 되는 지점까지. 인프라가 작동함을 증명한다.

- [ ] **Step 1: fixture 디렉토리 생성 + 첫 fixture 작성**

`tests/fixtures/golden/arith_basic.hik`:

```
:(1 + 2 * 3)출력
:(10 - 4)출력
:(15 / 4)출력
:(15 % 4)출력
```

(이 언어의 함수 호출 형식은 `:(인자)함수명`. `출력` 은 stdout 으로 인자를 출력하는 빌트인.)

- [ ] **Step 2: 기존 빌드로 기대 출력 캡처**

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/arith_basic.hik > tests/fixtures/golden/arith_basic.expected.txt
```

`arith_basic.expected.txt` 의 내용을 직접 열어서 확인한다. 기대값:
- 첫 줄: `7`
- 둘째 줄: `6`
- 셋째 줄: `3` (정수 나눗셈)
- 넷째 줄: `3`

만약 실제 출력이 다르면 — **버그를 골든으로 굳히지 말 것**. 멈추고 무엇이 어긋났는지 사용자에게 보고. (예: `15 / 4` 가 부동소수점이면 언어 의미를 spec 에 반영하고 fixture 내용 자체를 조정.)

- [ ] **Step 3: 골든 테스트 하니스 작성**

`tests/golden_test.cpp`:

```cpp
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
```

- [ ] **Step 4: CMakeLists.txt 수정 — 소스 추가 + fixture 경로 정의**

`CMakeLists.txt:130-138` (현재 `HongIkTests` 정의)을 다음과 같이 변경:

```cmake
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
```

그리고 같은 블록에서 `target_include_directories(HongIkTests ...)` 바로 뒤에 추가:

```cmake
    target_compile_definitions(HongIkTests PRIVATE
            "HONGIK_GOLDEN_FIXTURES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden\""
    )
```

- [ ] **Step 5: 빌드 + 골든 테스트 실행**

```powershell
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure -R Golden
```

Expected: `PhaseZeroGoldens/GoldenTest.Evaluator/arith_basic` PASS, `PhaseZeroGoldens/GoldenTest.VM/arith_basic` PASS. 두 줄 모두 그린.

- [ ] **Step 6: 전체 테스트도 그린 확인 (회귀 없음)**

```powershell
ctest --test-dir cmake-build-debug --output-on-failure
```

Expected: `100% tests passed`. 새 골든 테스트가 추가되었고 기존 테스트는 모두 그대로 통과.

- [ ] **Step 7: 커밋**

```powershell
git add tests/golden_test.cpp tests/fixtures/golden/arith_basic.hik tests/fixtures/golden/arith_basic.expected.txt CMakeLists.txt
git commit -m "test: add golden output harness with arith_basic fixture"
```

---

## Task 3: 나머지 골든 fixture 추가

**Files:**
- Create: `tests/fixtures/golden/vars_types.hik` + `.expected.txt`
- Create: `tests/fixtures/golden/if_else.hik` + `.expected.txt`
- Create: `tests/fixtures/golden/loops_break.hik` + `.expected.txt`
- Create: `tests/fixtures/golden/func_recursion.hik` + `.expected.txt`
- Create: `tests/fixtures/golden/builtins.hik` + `.expected.txt`
- Create: `tests/fixtures/golden/utf8_strings.hik` + `.expected.txt`
- Create: `tests/fixtures/golden/error_undefined.hik` + `.expected.txt`
- Modify: `tests/golden_test.cpp` — `INSTANTIATE_TEST_SUITE_P` 의 fixture 리스트 확장

각 fixture 마다 같은 사이클: ① `.hik` 작성 → ② 기존 빌드로 출력 캡처 → ③ 출력 검사 (의도와 일치하는지 사람이 확인) → ④ 두 백엔드 모두 그린 확인. 의도와 다르면 fixture 를 조정하거나 사용자에게 보고. **버그를 골든으로 굳히지 말 것.**

- [ ] **Step 1: vars_types fixture**

`tests/fixtures/golden/vars_types.hik`:

```
[정수] a = 42
[실수] b = 3.14
[문자] c = "안녕"
[논리] d = true
[배열] e = [1, 2, 3]
[사전] f = {"키": 100}
:(a)출력
:(b)출력
:(c)출력
:(d)출력
:(e)출력
:(f)출력
```

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/vars_types.hik > tests/fixtures/golden/vars_types.expected.txt
```

캡처된 파일 내용 확인. 기대 라인: `42`, `3.14`, `안녕`, `true`, 그리고 배열/사전의 ToString 표현(언어가 실제로 출력하는 형식 — 캡처값 그대로 받아들이고 사람이 한 번 검토). 만약 `[배열]` 또는 `[사전]` 타입 선언 키워드가 다르면 ([CLAUDE.md](../../CLAUDE.md) 참고) 멈추고 사용자에게 보고.

- [ ] **Step 2: if_else fixture**

`tests/fixtures/golden/if_else.hik`:

```
[정수] x = 7
만약 x > 5 라면:
    :("크다")출력
아니면:
    :("작다")출력

[정수] y = 3
만약 y > 5 라면:
    :("크다")출력
아니면:
    :("작다")출력
```

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/if_else.hik > tests/fixtures/golden/if_else.expected.txt
```

기대 라인: `크다`, `작다`.

- [ ] **Step 3: loops_break fixture**

`tests/fixtures/golden/loops_break.hik`:

```
[정수] i = 0
반복 i < 5 동안:
    만약 i == 3 라면:
        중단
    :(i)출력
    i = i + 1
```

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/loops_break.hik > tests/fixtures/golden/loops_break.expected.txt
```

기대 라인: `0`, `1`, `2` (i==3 에서 중단).

만약 위 반복문 문법이 실제 hong-ik 문법과 다르면 ([CLAUDE.md](../../CLAUDE.md) 와 기존 테스트의 패턴 참고하여 조정), 멈추고 사용자에게 확인. 문법 추측이 골든을 잘못된 형태로 굳히는 것을 방지.

- [ ] **Step 4: func_recursion fixture (피보나치)**

`tests/fixtures/golden/func_recursion.hik`:

```
함수: [정수]n 피보 -> [정수]:
    만약 n < 2 라면:
        리턴 n
    리턴 :(n - 1)피보 + :(n - 2)피보

:(:(0)피보)출력
:(:(1)피보)출력
:(:(5)피보)출력
:(:(10)피보)출력
```

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/func_recursion.hik > tests/fixtures/golden/func_recursion.expected.txt
```

기대 라인: `0`, `1`, `5`, `55`. 함수 호출 문법(`:(인자)함수명`)이 다르게 동작하면 멈추고 보고.

- [ ] **Step 5: builtins fixture (출력 / 길이 / 추가)**

`tests/fixtures/golden/builtins.hik`:

```
[배열] arr = [1, 2, 3]
:(:(arr)길이)출력
:(arr, 4)추가
:(:(arr)길이)출력
[문자] s = "안녕세상"
:(:(s)길이)출력
```

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/builtins.hik > tests/fixtures/golden/builtins.expected.txt
```

기대 라인: `3`, `4`, `4` (한글 4글자 기준). 만약 길이가 바이트 단위면 (12 등) — 그건 언어의 현재 동작이므로 **캡처값 그대로 골든화** (Phase 0 은 동작 보존이지 의미 수정이 아님).

- [ ] **Step 6: utf8_strings fixture**

`tests/fixtures/golden/utf8_strings.hik`:

```
[문자] greeting = "안녕하세요"
:(greeting)출력
[문자] mixed = "Hello, 세상!"
:(mixed)출력
```

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/utf8_strings.hik > tests/fixtures/golden/utf8_strings.expected.txt
```

기대 라인: `안녕하세요`, `Hello, 세상!`.

- [ ] **Step 7: error_undefined fixture (에러 메시지 회귀 감지)**

`tests/fixtures/golden/error_undefined.hik`:

```
:(없는변수)출력
```

```powershell
./cmake-build-debug/HongIk tests/fixtures/golden/error_undefined.hik > tests/fixtures/golden/error_undefined.expected.txt
```

캡처된 출력에는 `Repl::FileMode` 가 `RuntimeException` 을 잡아서 `HongIkError::formatError(...)` 또는 `Error: ...` 형식으로 stdout 에 찍은 에러 메시지가 들어있어야 한다 (line 137~144 in `repl/repl.cpp`). 캡처된 텍스트가 줄 번호와 변수명을 포함하는지 확인.

**중요**: Phase 0 의 골든 테스트는 **정확히 동일한 문자열 비교**다. 그래서 에러 메시지 포맷이 리팩토링 중에 한 글자라도 바뀌면 이 테스트가 잡아낸다. 이는 **의도된 안전장치** — 에러 포맷 변경이 의도되지 않은 회귀임을 즉시 알리고, 의도된 변경이라면 expected.txt 도 같이 업데이트해야 함을 강제한다.

- [ ] **Step 8: golden_test.cpp 의 fixture 리스트 확장**

`tests/golden_test.cpp` 의 `INSTANTIATE_TEST_SUITE_P` 블록을 다음으로 교체:

```cpp
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
        GoldenCase{"error_undefined"}
    ),
    [](const ::testing::TestParamInfo<GoldenCase>& info) {
        return info.param.name;
    });
```

- [ ] **Step 9: 전체 빌드 + 골든 테스트 실행**

```powershell
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure -R Golden
```

Expected: 8 fixture × 2 backend = **16 골든 테스트 모두 PASS**. 한쪽 백엔드만 깨진 fixture 가 있으면 — evaluator/vm 의 의미 차이가 드러난 것. 멈추고 사용자에게 보고 (Phase 0 의 부수 효과로 발견된 버그 → 별도 이슈로).

- [ ] **Step 10: 전체 테스트 그린 재확인**

```powershell
ctest --test-dir cmake-build-debug --output-on-failure
```

Expected: `100% tests passed`.

- [ ] **Step 11: 커밋**

```powershell
git add tests/fixtures/golden/ tests/golden_test.cpp
git commit -m "test: extend golden fixtures (vars/if/loop/func/builtin/utf8/error)"
```

---

## Task 4: WASM 빌드 + Sanitizer 빌드 베이스라인 기록

**Files:**
- Create: `docs/superpowers/plans/2026-05-10-phase-0-baseline.md`

목적: Phase 1~4 진행 중 WASM 산출물 사이즈가 크게 흔들리거나 sanitizer 가 새로 잡는 이슈가 생겼는지 비교할 수 있도록 기준치를 박아둔다.

- [ ] **Step 1: WASM 빌드 (별도 빌드 디렉토리)**

(Emscripten 환경이 활성화되어 있어야 함 — `emcmake` 사용 가능 가정. 미설치 시 사용자에게 요청.)

```powershell
emcmake cmake -B build-wasm
cmake --build build-wasm
```

Expected: `build-wasm/hongik-wasm.js` + `build-wasm/hongik-wasm.wasm` 생성. 빌드 에러 없음.

- [ ] **Step 2: WASM 산출물 사이즈 기록**

```powershell
Get-Item build-wasm/hongik-wasm.js, build-wasm/hongik-wasm.wasm | Select-Object Name, Length
```

출력 결과의 두 줄(이름 + 바이트)을 다음 단계의 baseline 문서에 옮겨 적는다.

- [ ] **Step 3: Sanitizer 빌드**

(MSVC 환경에서는 ASan 만 지원. clang/gcc 환경이면 ASan + UBSan 모두.)

```powershell
cmake -B cmake-build-san -DHONGIK_ENABLE_SANITIZERS=ON
cmake --build cmake-build-san
ctest --test-dir cmake-build-san --output-on-failure
```

Expected: 빌드 성공, 모든 테스트(골든 포함) 그린. ASan/UBSan 이 잡는 이슈가 있으면 출력에 명확히 드러남 — 있으면 멈추고 사용자에게 보고. **Phase 0 끝나기 전에 sanitizer 그린 상태가 베이스라인이어야 함.**

- [ ] **Step 4: 베이스라인 문서 작성**

`docs/superpowers/plans/2026-05-10-phase-0-baseline.md`:

```markdown
# Phase 0 베이스라인

- 기록일: 2026-05-10
- 빌드 머신: <OS / 컴파일러 / Emscripten 버전을 단계 1~3 출력에서 채워 넣을 것>

## WASM 산출물 사이즈

| 파일 | 바이트 |
|---|---|
| `build-wasm/hongik-wasm.js` | <Step 2 의 Length> |
| `build-wasm/hongik-wasm.wasm` | <Step 2 의 Length> |

## Sanitizer 결과

- `cmake -B cmake-build-san -DHONGIK_ENABLE_SANITIZERS=ON` 빌드 성공: ✓
- `ctest` 그린: ✓ (전체 N 개 테스트 통과 — Step 3 의 출력에서 N 채워 넣을 것)
- 검출된 이슈: 없음

## 사용 방법

Phase 1~4 의 각 PR 머지 직전, 위 명령을 동일하게 다시 실행하고:
- WASM 사이즈가 ±10% 를 초과하면 PR 설명에 사유 기록.
- Sanitizer 가 새 이슈를 잡으면 PR 머지 전에 해결.
```

문서 안의 `<...>` 자리는 실제 캡처값으로 채워넣고 저장. **저장 시점에 `<...>` 표기가 남아있으면 안 됨.**

- [ ] **Step 5: 골든 + 일반 테스트 + sanitizer 빌드 모두 그린 최종 확인**

```powershell
ctest --test-dir cmake-build-debug --output-on-failure
ctest --test-dir cmake-build-san --output-on-failure
```

Expected: 둘 다 `100% tests passed`. WASM 빌드는 별도 — `build-wasm/hongik-wasm.js` 가 존재하는지 마지막에 한 번 더 확인.

```powershell
Test-Path build-wasm/hongik-wasm.js
```

Expected: `True`.

- [ ] **Step 6: CI 워크플로 점검**

`.github/workflows/build.yml` 을 열어 다음을 모두 만족하는지 확인:

1. 네이티브 빌드 + `ctest` 단계가 있는가? — `golden_test.cpp` 가 `HongIkTests` 에 추가되었으므로 자동으로 CI 에서 실행될 것.
2. WASM 빌드 단계가 있는가? — 없으면 PR 별로 산출물이 안 만들어졌는지 자동 검증 불가.

만약 (1) 또는 (2) 가 누락이면 — **Phase 0 plan 에서 직접 수정하지 말고**, 발견 사항을 베이스라인 문서(`2026-05-10-phase-0-baseline.md`) 의 "후속 작업" 섹션에 기록. CI 강화는 별도 PR 로 분리 (이번 plan 의 핵심은 안전망 코드 추가이지 CI 인프라 변경 아님).

만약 (1)(2) 모두 OK 이면 — 베이스라인 문서에 "CI 게이트 검증 완료" 한 줄 기록.

- [ ] **Step 7: 커밋**

```powershell
git add docs/superpowers/plans/2026-05-10-phase-0-baseline.md
git commit -m "docs(phase-0): record baseline WASM size and sanitizer status"
```

---

## Phase 0 종료 조건 체크리스트

Task 4 까지 끝나면 다음이 모두 참:

- [ ] `cmake-build-debug` ctest 100% pass (골든 포함)
- [ ] 8 fixture × 2 backend = 16 골든 테스트 그린
- [ ] WASM 빌드 (`build-wasm/hongik-wasm.{js,wasm}`) 성공
- [ ] Sanitizer 빌드 (`cmake-build-san`) 성공 + ctest 100% pass
- [ ] `docs/superpowers/plans/2026-05-10-phase-0-baseline.md` 에 실제 수치 기록 (`<...>` 잔재 없음, CI 게이트 점검 결과 기록)
- [ ] git log 에 3개 커밋 (`test: add golden output harness with arith_basic fixture`, `test: extend golden fixtures (vars/if/loop/func/builtin/utf8/error)`, `docs(phase-0): record baseline WASM size and sanitizer status`)

이 시점부터 Phase 1 (디렉토리 골격 + 단순 이동) 시작 가능. Phase 1 plan 은 별도 작성.

---

## Notes for the Implementer

- **추측 금지**: hong-ik 문법(반복문 키워드, 함수 호출 형식)이 plan 의 fixture 와 다르게 보이면 멈추고 보고. 추측해서 fixture 를 만들면 골든이 잘못된 동작을 잠그게 된다.
- **버그 굳히기 금지**: 캡처한 expected 출력이 fixture 의 의도와 어긋나면(예: `15 / 4` 가 `3.75` 로 나옴) — 그것을 골든으로 박지 말고 멈춰서 사용자에게 알릴 것. Phase 0 의 목적은 "현재 동작을 안전하게 보존" 이지 "현재 버그를 정상으로 승격" 이 아님.
- **eval/vm 의미 차이**: 어떤 fixture 가 한 백엔드만 깨지면 그 자체가 발견 — 별도 이슈로 보고. 해당 fixture 는 Phase 0 골든에서 임시로 제외 (주석 처리 + TODO 주석으로 사유 명시) 하고 Phase 0 종료. 두 백엔드 동등성은 리팩토링 별 작업으로 넘김.
- **PowerShell vs bash**: 명령은 PowerShell 기준. bash 환경이면 `/` 경로 + `Get-Item` 대신 `ls -l` 등으로 적절히 치환.
- **Phase 0 은 코드 변경 0**: 프로덕션 코드(`lexer/`, `parser/`, ...) 는 절대 건드리지 않는다. 새 테스트 + fixtures + 베이스라인 문서뿐.
