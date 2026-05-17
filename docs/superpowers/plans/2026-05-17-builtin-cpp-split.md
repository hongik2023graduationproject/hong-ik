# built_in.cpp 분할 (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `object/built_in.cpp` (1,168줄)을 카테고리별 7개 .cpp 파일로 분할한다. 함수 본문은 **순수 이동**, 동작 변경 0건, 인터페이스(`built_in.h`) 보존.

**Architecture:** 헤더는 그대로 두고 `.cpp`만 카테고리(math/string/array/collection/io/conversion/json)별로 쪼갠다. 각 카테고리 옮길 때마다 `built_in.cpp`에서 해당 함수를 삭제 → CMakeLists에 새 파일 추가 → 빌드+테스트로 게이트. 모든 함수 이동 완료 후 빈 `built_in.cpp` 삭제, 최종 commit 1개.

**Tech Stack:** C++17/26, CMake 3.28+, Google Test, gcc/clang/MSVC. 작업 경로: `E:\dev\projects\school\hongik\hong-ik`.

---

## 기준 매핑 (출처 line은 분할 전 `built_in.cpp` 기준)

| 카테고리 | 파일 | 함수 | 출처 line |
|----------|------|------|-----------|
| math | `object/builtins/math.cpp` | Abs, Sqrt, Max, Min, Random, Sin, Cos, Tan, Log, Ln, Power, Pi, EulerE, Round, Ceil, Floor (16) | 358~609 |
| string | `object/builtins/string.cpp` | Split, ToUpper, ToLower, Replace, Trim, StartsWith, EndsWith, Repeat, Pad, Substring (10) | 611~798 |
| array | `object/builtins/array.cpp` | Sort, Reverse, Find, Slice, Push (5) | 800~882 + 78~88 |
| collection | `object/builtins/collection.cpp` | Length, Contains, MapSet, Remove, Keys (5) | 17~33 + 192~275 |
| io | `object/builtins/io.cpp` | Print, Input, FileRead, FileWrite (4) | 35~76 + 167~190 + 277~356 |
| conversion | `object/builtins/conversion.cpp` | TypeOf, ToInteger, ToFloat, ToString_ (4) | 90~165 |
| json | `object/builtins/json.cpp` | 익명 namespace의 `JsonParser` 클래스 + `serializeToJson` 헬퍼 + JsonParse, JsonSerialize (2 + helpers) | 884~1168 |

**총 46개 Builtin + JSON 헬퍼**가 빠짐없이 옮겨져야 한다.

각 새 .cpp는 공통 헤더 패턴 사용:
```cpp
#include "../built_in.h"
// 카테고리별 추가 include (아래 task 본문 참조)

using namespace std;
```
`using namespace std;`는 분할 작업 범위 밖 — 그대로 유지. 헤더 정리는 로드맵 #6에서.

---

## Task 0: Baseline 빌드/테스트 통과 확인

분할 시작 전에 현재 코드가 깨끗히 빌드/통과하는지 확인.

**Files:** 없음 (read-only)

- [ ] **Step 1: 빌드**

```
cmake --build cmake-build-debug
```
Expected: 빌드 성공, warning 없음.

- [ ] **Step 2: 전체 테스트 실행**

```
ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: 모든 테스트 통과. 테스트 개수와 PASS/FAIL 출력을 **기록해 둔다** (예: "100% tests passed, 345 tests"). 이 숫자가 분할 종료 시 동일해야 한다.

- [ ] **Step 3: 현재 commit hash 기록**

```
git log -1 --oneline
```
회귀 시 `git revert <hash>` 또는 `git reset --hard <hash>`로 원복할 기준점.

---

## Task 1: math 카테고리 분리

**Files:**
- Create: `object/builtins/math.cpp`
- Modify: `object/built_in.cpp` (line 358~609 함수 16개 삭제)
- Modify: `CMakeLists.txt` (line 53 근방, `HONGIK_CORE_SOURCES`)

- [ ] **Step 1: `object/builtins/math.cpp` 생성**

다음 헤더로 시작:
```cpp
#include "../built_in.h"

#include <cmath>
#include <random>
#include <stdexcept>

using namespace std;
```

이어서 `built_in.cpp` line 358~609의 다음 16개 함수 본문을 **수정 없이 그대로** 복사: `Abs`, `Sqrt`, `Max`, `Min`, `Random`, `Sin`, `Cos`, `Tan`, `Log`, `Ln`, `Power`, `Pi`, `EulerE`, `Round`, `Ceil`, `Floor`. 함수 사이의 한국어 주석(예: `// 수학 내장함수`, `// 삼각함수` 등)도 같이 옮긴다.

`Read` 도구로 `built_in.cpp` line 358~609 범위를 읽어 그대로 옮기기.

- [ ] **Step 2: `built_in.cpp`에서 옮긴 16개 함수 정의 + 주석 삭제**

`Read`로 line 355~615 확인 후 `Edit`로 그대로 제거. 주변 빈 줄은 1개만 남긴다.

- [ ] **Step 3: CMakeLists.txt 업데이트**

`HONGIK_CORE_SOURCES`에 `object/builtins/math.cpp` 한 줄 추가. **`object/built_in.cpp`는 아직 남겨둔다** (다른 함수들이 있으므로).

```cmake
set(HONGIK_CORE_SOURCES
        ...
        object/built_in.cpp
        object/builtins/math.cpp
        ...
)
```

- [ ] **Step 4: 빌드**

```
cmake --build cmake-build-debug
```
Expected: 빌드 성공. 만약 중복 심볼 에러 시 → 같은 함수가 두 곳에 정의된 것. `grep -n "Abs::function\|Sqrt::function" object/built_in.cpp object/builtins/math.cpp` 로 확인.

- [ ] **Step 5: 전체 테스트**

```
ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: Task 0에서 기록한 개수와 동일하게 100% 통과.

- [ ] **Step 6: 검증 — 16개 함수가 빠짐없이 새 위치에**

```
grep -cE "^std::shared_ptr<Object> (Abs|Sqrt|Max|Min|Random|Sin|Cos|Tan|Log|Ln|Power|Pi|EulerE|Round|Ceil|Floor)::function" object/builtins/math.cpp
```
Expected: `16`

```
grep -cE "^std::shared_ptr<Object> (Abs|Sqrt|Max|Min|Random|Sin|Cos|Tan|Log|Ln|Power|Pi|EulerE|Round|Ceil|Floor)::function" object/built_in.cpp
```
Expected: `0`

---

## Task 2: string 카테고리 분리

**Files:**
- Create: `object/builtins/string.cpp`
- Modify: `object/built_in.cpp` (분할 후 기준 line으로 string 함수 10개 삭제)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: `object/builtins/string.cpp` 생성**

헤더:
```cpp
#include "../built_in.h"

#include "../../util/utf8_utils.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

using namespace std;
```

이어서 `built_in.cpp`의 다음 10개 함수를 그대로 옮긴다: `Split`, `ToUpper`, `ToLower`, `Replace`, `Trim`, `StartsWith`, `EndsWith`, `Repeat`, `Pad`, `Substring`. 사이의 `// 문자열 내장함수` 같은 주석도 함께.

위치 찾기: `grep -nE "^std::shared_ptr<Object> Split::" object/built_in.cpp` 로 시작 line 확인 → 마지막 함수(`Substring`)의 닫는 중괄호 line까지 `Read`.

- [ ] **Step 2: `built_in.cpp`에서 10개 함수 + 주석 삭제**

- [ ] **Step 3: CMakeLists.txt에 `object/builtins/string.cpp` 추가**

- [ ] **Step 4: 빌드 + 테스트**

```
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: 빌드 성공, 모든 테스트 통과.

- [ ] **Step 5: 검증**

```
grep -cE "^std::shared_ptr<Object> (Split|ToUpper|ToLower|Replace|Trim|StartsWith|EndsWith|Repeat|Pad|Substring)::function" object/builtins/string.cpp
```
Expected: `10`

```
grep -cE "^std::shared_ptr<Object> (Split|ToUpper|ToLower|Replace|Trim|StartsWith|EndsWith|Repeat|Pad|Substring)::function" object/built_in.cpp
```
Expected: `0`

---

## Task 3: array 카테고리 분리

**Files:**
- Create: `object/builtins/array.cpp`
- Modify: `object/built_in.cpp` (Sort, Reverse, Find, Slice + Push 삭제 — Push는 line 78 근처에 있고 나머지는 line 800 근처)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: `object/builtins/array.cpp` 생성**

헤더:
```cpp
#include "../built_in.h"

#include <algorithm>
#include <stdexcept>

using namespace std;
```

이어서 다음 5개 함수를 옮긴다: `Push`, `Sort`, `Reverse`, `Find`, `Slice`. 파일 내 순서는 새 파일에서 의미상 정리(Push → Sort → Reverse → Find → Slice). 사이의 `// 배열 내장함수` 주석도 함께. **Push는 원래 line 78 근처(Length 옆)에 있고 나머지는 line 800 근처에 있다 — 두 위치 모두에서 가져온다.**

- [ ] **Step 2: `built_in.cpp`에서 5개 함수 + 주석 삭제**

Push는 line 78~88 (Length 다음, TypeOf 이전 위치). Sort/Reverse/Find/Slice는 line 800~882 근처. 각각 Read로 확인 후 Edit.

- [ ] **Step 3: CMakeLists.txt에 `object/builtins/array.cpp` 추가**

- [ ] **Step 4: 빌드 + 테스트**

```
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: 빌드 성공, 모든 테스트 통과.

- [ ] **Step 5: 검증**

```
grep -cE "^std::shared_ptr<Object> (Push|Sort|Reverse|Find|Slice)::function" object/builtins/array.cpp
```
Expected: `5`

```
grep -cE "^std::shared_ptr<Object> (Push|Sort|Reverse|Find|Slice)::function" object/built_in.cpp
```
Expected: `0`

---

## Task 4: collection 카테고리 분리

**Files:**
- Create: `object/builtins/collection.cpp`
- Modify: `object/built_in.cpp` (Length, Contains, MapSet, Remove, Keys 삭제)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: `object/builtins/collection.cpp` 생성**

헤더:
```cpp
#include "../built_in.h"

#include "../../util/utf8_utils.h"

#include <stdexcept>

using namespace std;
```

다음 5개 함수를 옮긴다: `Length`, `Keys`, `Contains`, `MapSet`, `Remove`. 

`Length` 본문은 UTF-8 코드포인트 처리에 `utf8::` 네임스페이스를 사용하므로 `util/utf8_utils.h` include 필수. (원본 line 17~33 본문 확인)

- [ ] **Step 2: `built_in.cpp`에서 5개 함수 삭제**

Length(line 17), Keys(192), Contains(209), MapSet(236), Remove(253) — 각각 Read로 끝 line 확인.

- [ ] **Step 3: CMakeLists.txt에 `object/builtins/collection.cpp` 추가**

- [ ] **Step 4: 빌드 + 테스트**

```
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: 통과.

- [ ] **Step 5: 검증**

```
grep -cE "^std::shared_ptr<Object> (Length|Keys|Contains|MapSet|Remove)::function" object/builtins/collection.cpp
```
Expected: `5`

```
grep -cE "^std::shared_ptr<Object> (Length|Keys|Contains|MapSet|Remove)::function" object/built_in.cpp
```
Expected: `0`

---

## Task 5: io 카테고리 분리

**Files:**
- Create: `object/builtins/io.cpp`
- Modify: `object/built_in.cpp` (Print, Input, FileRead, FileWrite 삭제)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: `object/builtins/io.cpp` 생성**

헤더:
```cpp
#include "../built_in.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
```

다음 4개 함수를 옮긴다: `Print`, `Input`, `FileRead`, `FileWrite`.

- [ ] **Step 2: `built_in.cpp`에서 4개 함수 삭제**

Print(35), Input(167), FileRead(277), FileWrite(315) — Read로 끝 line 확인.

- [ ] **Step 3: CMakeLists.txt에 `object/builtins/io.cpp` 추가**

- [ ] **Step 4: 빌드 + 테스트**

```
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: 통과. 특히 `*Print*` 필터로도 확인:
```
cmake-build-debug/HongIkTests --gtest_filter="*Print*"
```

- [ ] **Step 5: 검증**

```
grep -cE "^std::shared_ptr<Object> (Print|Input|FileRead|FileWrite)::function" object/builtins/io.cpp
```
Expected: `4`

```
grep -cE "^std::shared_ptr<Object> (Print|Input|FileRead|FileWrite)::function" object/built_in.cpp
```
Expected: `0`

---

## Task 6: conversion 카테고리 분리

**Files:**
- Create: `object/builtins/conversion.cpp`
- Modify: `object/built_in.cpp` (TypeOf, ToInteger, ToFloat, ToString_ 삭제)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: `object/builtins/conversion.cpp` 생성**

헤더:
```cpp
#include "../built_in.h"

#include <sstream>
#include <stdexcept>

using namespace std;
```

다음 4개 함수를 옮긴다: `TypeOf`, `ToInteger`, `ToFloat`, `ToString_`.

- [ ] **Step 2: `built_in.cpp`에서 4개 함수 삭제**

TypeOf(90), ToInteger(112), ToFloat(137), ToString_(159) — 끝 line은 Read로 확인.

- [ ] **Step 3: CMakeLists.txt에 `object/builtins/conversion.cpp` 추가**

- [ ] **Step 4: 빌드 + 테스트**

```
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: 통과.

- [ ] **Step 5: 검증**

```
grep -cE "^std::shared_ptr<Object> (TypeOf|ToInteger|ToFloat|ToString_)::function" object/builtins/conversion.cpp
```
Expected: `4`

```
grep -cE "^std::shared_ptr<Object> (TypeOf|ToInteger|ToFloat|ToString_)::function" object/built_in.cpp
```
Expected: `0`

---

## Task 7: json 카테고리 분리 (헬퍼 포함)

**Files:**
- Create: `object/builtins/json.cpp`
- Modify: `object/built_in.cpp` (`// ===== JSON 내장함수 =====` 주석부터 EOF까지 전부 삭제)
- Modify: `CMakeLists.txt`

이 task는 익명 namespace 내부의 `JsonParser` 클래스와 `serializeToJson` 함수까지 함께 옮긴다.

- [ ] **Step 1: `object/builtins/json.cpp` 생성**

헤더:
```cpp
#include "../built_in.h"

#include "../../util/json_util.h"

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;
```

이어서 분할 전 기준 line 884~1168의 다음 블록을 **그대로** 옮긴다:
1. `// ===== JSON 내장함수 =====` 주석
2. `namespace { ... } // anonymous namespace` 전체 (JsonParser 클래스 정의 + `serializeToJson` 함수)
3. `JsonParse::function(...)` 정의
4. `JsonSerialize::function(...)` 정의

찾기: `grep -nE "^namespace \{|^class JsonParser|^string serializeToJson|^\} // anonymous|^std::shared_ptr<Object> JsonParse|^std::shared_ptr<Object> JsonSerialize" object/built_in.cpp`

- [ ] **Step 2: `built_in.cpp`에서 위 블록 전체 삭제**

`// ===== JSON 내장함수 =====` 주석부터 파일 끝(JsonSerialize의 닫는 중괄호)까지 삭제.

- [ ] **Step 3: CMakeLists.txt에 `object/builtins/json.cpp` 추가**

- [ ] **Step 4: 빌드 + 테스트**

```
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: 통과.

```
cmake-build-debug/HongIkTests --gtest_filter="*Json*"
```
Expected: JSON 관련 테스트 통과.

- [ ] **Step 5: 검증**

```
grep -cE "^std::shared_ptr<Object> (JsonParse|JsonSerialize)::function" object/builtins/json.cpp
```
Expected: `2`

```
grep -cE "JsonParser|serializeToJson|JsonParse::function|JsonSerialize::function" object/built_in.cpp
```
Expected: `0`

---

## Task 8: 빈 built_in.cpp 삭제 + 최종 검증

이 시점 `object/built_in.cpp`는 헤더 include 몇 줄과 빈 본문만 남아 있다 (혹은 완전 텅 빈 파일).

**Files:**
- Delete: `object/built_in.cpp`
- Modify: `CMakeLists.txt` (`object/built_in.cpp` 항목 제거)

- [ ] **Step 1: `object/built_in.cpp` 잔여 내용 확인**

```
wc -l object/built_in.cpp
```
Expected: 20줄 미만 (include + 빈 본문). 만약 20줄 이상이라면 옮기지 못한 함수가 있다는 뜻 — 추가 조사 필요.

```
grep -cE "^std::shared_ptr<Object> [A-Z]" object/built_in.cpp
```
Expected: `0` (어떤 Builtin 함수 정의도 남아있지 않아야 함)

- [ ] **Step 2: `object/built_in.cpp` 삭제**

PowerShell: `Remove-Item object/built_in.cpp`
또는 Bash: `rm object/built_in.cpp`

- [ ] **Step 3: CMakeLists.txt에서 `object/built_in.cpp` 줄 제거**

`HONGIK_CORE_SOURCES`에서 해당 한 줄 삭제. 결과는 다음과 같아야 한다 (다른 라인은 변경 없음):
```cmake
set(HONGIK_CORE_SOURCES
        repl/repl.cpp
        utf8_converter/utf8_converter.cpp
        lexer/lexer.cpp
        token/token.cpp
        parser/parser.cpp
        evaluator/evaluator.cpp
        environment/environment.cpp
        object/builtins/math.cpp
        object/builtins/string.cpp
        object/builtins/array.cpp
        object/builtins/collection.cpp
        object/builtins/io.cpp
        object/builtins/conversion.cpp
        object/builtins/json.cpp
        vm/compiler.cpp
        vm/vm.cpp
        io/io_interface.cpp
        error/hongik_error.cpp
        analyzer/analyzer.cpp
)
```

- [ ] **Step 4: 클린 빌드**

```
cmake --build cmake-build-debug --clean-first
```
Expected: 빌드 성공, warning 0건 (build log를 Task 0과 비교).

- [ ] **Step 5: 전체 테스트 통과 확인**

```
ctest --test-dir cmake-build-debug --output-on-failure
```
Expected: Task 0과 동일한 테스트 개수 100% 통과.

- [ ] **Step 6: 카테고리별 필터 회귀 확인**

```
cmake-build-debug/HongIkTests --gtest_filter="*Builtin*:*Print*:*Length*:*Json*:*Math*"
```
Expected: 모두 통과.

```
cmake-build-debug/HongIkTests --gtest_filter="GoldenTest.*"
```
Expected: 모두 통과.

- [ ] **Step 7: 전수 검증 — 46개 Builtin 모두 정의됨**

```
grep -rhE "^std::shared_ptr<Object> [A-Z][a-zA-Z_]*::function" object/builtins/ | wc -l
```
Expected: `46`

```
grep -rhE "^std::shared_ptr<Object> [A-Z][a-zA-Z_]*::function" object/builtins/ | sort -u | wc -l
```
Expected: `46` (중복 없음)

---

## Task 9: 단일 commit

**Files:** 없음 (git 작업만)

- [ ] **Step 1: 변경 사항 확인**

```
git status
git diff --stat
```
Expected: `built_in.cpp` 삭제, `builtins/*.cpp` 7개 추가, `CMakeLists.txt` 수정. 다른 변경은 없어야 함.

- [ ] **Step 2: 스테이징**

```
git add object/built_in.cpp object/builtins/ CMakeLists.txt
```

- [ ] **Step 3: 단일 commit 생성**

```
git commit -m "refactor(object): split built_in.cpp into category files

Split 1,168-line built_in.cpp into seven category files under
object/builtins/ (math, string, array, collection, io, conversion,
json). Pure relocation - no behavior change, public header
(built_in.h) untouched.

Phase 1 of the giant-file split roadmap (see
docs/superpowers/specs/2026-05-17-cpp-giant-file-split-design.md).
"
```

- [ ] **Step 4: commit 확인**

```
git log -1 --stat
```
Expected: 7 .cpp 추가, 1 .cpp 삭제, 1 CMakeLists 수정.

---

## Task 10: 메모리 인덱스 업데이트

**Files:**
- Modify: `C:\Users\98kim\.claude\projects\E--dev-projects-school-hongik\memory\project-refactoring-roadmap.md`

- [ ] **Step 1: 로드맵 메모리 갱신**

`Read` 후 `Edit`로 `#2 hong-ik 거대 파일 분할` 섹션을 업데이트:
- **완료** 섹션에 다음을 추가:
  - `**#2 Phase 1: built_in.cpp 분할 (High)** — 1,168줄 → 7개 카테고리 파일(math/string/array/collection/io/conversion/json), 헤더 보존, 345 tests 통과.`
- **대기** 섹션의 `#2 hong-ik 거대 파일 분할 (High)` 항목을 **Phase 2 시작점**으로 변경:
  - `#2 거대 파일 분할 Phase 2 (vm.cpp run() 메서드 분할) ← 다음 세션 시작점`
  - 나머지 phase 3, 4도 명시.

---

## Self-Review 체크리스트 (계획서 작성자 검토 완료)

- [x] **Spec coverage** — 설계서의 7개 카테고리 매핑이 Task 1~7에 1:1로 대응. 헤더 보존, CMake 변경, 검증 절차, 단일 commit 모두 task로 매핑됨.
- [x] **Placeholder scan** — TBD/TODO/"적절히" 없음. 모든 step에 정확한 명령어, expected 결과, 변경할 file path 포함.
- [x] **Type consistency** — 모든 task에서 `built_in.h`를 `../built_in.h`로 include하고, `using namespace std;` 패턴 일관됨. `JsonParser` 헬퍼 포함 빠뜨림 없음.

---

## 실행 옵션

Plan complete and saved to `docs/superpowers/plans/2026-05-17-builtin-cpp-split.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
