# hong-ik 거대 C++ 파일 분할 설계 (1단계: built_in.cpp)

- 작성일: 2026-05-17
- 대상: `hong-ik` (C++17/26 한글 인터프리터)
- 목적: 4대 거대 파일(1,000줄 이상)을 의미 있는 단위로 분할 — **순수 이동, 동작 보존**
- 로드맵 위치: 리팩토링 #2 (메모리 인덱스 기준)

## 1. 목표와 범위

### 목표
1. 1,000줄 이상 .cpp 파일 4개를 카테고리 단위로 분할해 유지보수성을 회복한다.
2. 4단계로 나눠 점진 진행한다. 각 단계는 별도 commit으로 회복 지점을 확보한다.
3. **이번 설계서는 1단계(`built_in.cpp` 분할)만 다룬다.** 2~4단계는 1단계 완료 후 학습을 반영해 별도 설계서로 작성한다.

### 비목표 (이번 단계에서 하지 않음)
- 함수 본문 리팩토링 / 버그 수정 / 시그니처 변경.
- `built_in.h` 분할 — 헤더는 200줄 수준으로 적당하고, 외부(`evaluator.h`, `vm.h`)가 의존하는 인터페이스다.
- `using namespace std;` 제거 — 로드맵 #6의 일괄 작업.
- 2~4단계 작업 (vm.cpp, compiler.cpp, evaluator.cpp).
- 신규 테스트 작성.

## 2. 현재 상태 진단

### 4대 거대 파일

| 파일 | 줄수 | 내부 구조 | 분할 난이도 |
|------|------|-----------|-------------|
| `evaluator/evaluator.cpp` | 1,593 | `eval()` 한 함수가 line 148~790 (~640줄)에 `if(dynamic_cast<X>())` 30+개로 노드 dispatch + 나머지 helper 28개 메서드 | ★★★ 높음 |
| `vm/compiler.cpp` | 1,323 | `compileStatement`/`compileExpression` switch에서 카테고리별 메서드 호출 — 메서드는 이미 잘 쪼개져 있음 | ★ 낮음 |
| `vm/vm.cpp` | 1,087 | `run()` 한 함수가 line 240~1073 (~830줄)에 OP_ 50+ opcode 거대 switch | ★★ 중간 |
| `object/built_in.cpp` | 1,168 | 46개 독립 Builtin 클래스 (math 16, string 10, array 5, collection 5, io 4, conversion 4, json 2). 함수 간 의존 0 | ★ 낮음 |

### 회귀 안전망

- `tests/repl_test.cpp` (~1,900줄) — end-to-end 회귀
- `tests/vm_test.cpp` (~25KB) — VM 단위 테스트
- `tests/evaluator_test.cpp` (~28KB) — evaluator 단위 테스트
- `tests/golden_test.cpp` + `tests/fixtures/golden/` — 골든 출력 비교
- 총 345개 테스트가 4단계 전체에 대해 회귀 감지 게이트로 작동.

### `built_in.cpp` 의존성

- `built_in.h`는 외부에서 `evaluator/evaluator.h`, `vm/vm.h` 두 곳만 include한다.
- `built_in.cpp`는 `built_in.h` + `util/json_util.h` + `util/utf8_utils.h` + 표준 라이브러리(`<algorithm>`, `<cmath>`, `<fstream>`, `<iostream>`, `<random>`, `<sstream>`, `<stdexcept>`)에만 의존.
- 46개 Builtin 클래스 간 호출 의존 없음 — 기계적으로 옮길 수 있다.

## 3. 1단계 분할 설계

### 디렉토리 변경

```
object/
├── object.h                    (변경 없음)
├── object_type.h               (변경 없음)
├── built_in.h                  (변경 없음 — 외부 인터페이스 보존)
├── built_in.cpp                (삭제)
└── builtins/                   (신설)
    ├── math.cpp                # Abs, Sqrt, Max, Min, Random, Sin, Cos, Tan,
    │                           # Log, Ln, Power, Pi, EulerE, Round, Ceil, Floor
    ├── string.cpp              # Split, ToUpper, ToLower, Replace, Trim,
    │                           # StartsWith, EndsWith, Repeat, Pad, Substring
    ├── array.cpp               # Sort, Reverse, Find, Slice, Push
    ├── collection.cpp          # Length, Contains, MapSet, Remove, Keys
    ├── io.cpp                  # Print, Input, FileRead, FileWrite
    ├── conversion.cpp          # TypeOf, ToInteger, ToFloat, ToString_
    └── json.cpp                # JsonParse, JsonSerialize
```

**Push 위치 결정:** 헤더 순서상 Length 옆에 있지만, 의미상 배열 조작이므로 `array.cpp`에 둔다.

### 각 .cpp 헤더 패턴

각 카테고리 파일은 **필요한 헤더만** include한다 (현재 built_in.cpp는 모든 include를 한꺼번에 함).

| 파일 | include 목록 (예시) |
|------|----------------------|
| `math.cpp` | `built_in.h`, `<cmath>`, `<random>` |
| `string.cpp` | `built_in.h`, `util/utf8_utils.h`, `<algorithm>`, `<sstream>` |
| `array.cpp` | `built_in.h`, `<algorithm>` |
| `collection.cpp` | `built_in.h` |
| `io.cpp` | `built_in.h`, `<fstream>`, `<iostream>`, `<sstream>` |
| `conversion.cpp` | `built_in.h`, `<sstream>`, `<stdexcept>` |
| `json.cpp` | `built_in.h`, `util/json_util.h` |

각 .cpp는 `using namespace std;`를 그대로 유지(별도 정리 사이클).

### CMakeLists.txt 변경

`hong-ik/CMakeLists.txt` line 53의 `HONGIK_CORE_SOURCES`에서:

```cmake
# AS-IS
object/built_in.cpp

# TO-BE
object/builtins/math.cpp
object/builtins/string.cpp
object/builtins/array.cpp
object/builtins/collection.cpp
object/builtins/io.cpp
object/builtins/conversion.cpp
object/builtins/json.cpp
```

다른 CMake 설정(네이티브/WASM 분기, 테스트 타겟, sanitizer 등)은 영향 없음 — `HONGIK_CORE_SOURCES` 한 변수만 수정.

## 4. 검증 절차

분할 1회 완료 후 다음 순서로 실행:

```bash
# 1) 빌드
cmake --build cmake-build-debug 2>&1 | tee build.log

# 2) 전체 테스트
ctest --test-dir cmake-build-debug --output-on-failure

# 3) 카테고리별 핵심 필터 (회귀 명시 확인)
cmake-build-debug/HongIkTests --gtest_filter="*Builtin*:*Print*:*Length*:*Json*:*Math*"

# 4) 골든 테스트 (fixture 기반 end-to-end)
cmake-build-debug/HongIkTests --gtest_filter="GoldenTest.*"
```

**합격 기준:**
- 345개 테스트 전부 통과
- 새 warning 0건 (build.log 비교)

## 5. 위험 분석과 롤백

| 위험 | 가능성 | 대응 |
|------|--------|------|
| include 누락 (예: math.cpp가 `<cmath>` 없이) | 중 | 빌드 에러로 즉시 드러남 |
| CMakeLists에 파일 추가 누락 | 낮 | 링크 에러로 즉시 드러남 |
| 함수 매핑 실수 (예: Push를 collection.cpp로 잘못 분류) | 낮 | 동작은 동일, 의미 분류 차이 → 리뷰에서 잡기 |
| 헤더 변경으로 외부 코드 영향 | 0 | 헤더 안 건드림 — 외부 영향 없음 |
| 회귀 테스트 실패 | 매우 낮 | 단순 이동이라 가능성 거의 없음. 실패 시 단일 commit `git revert` 한 줄로 원복 |

**롤백 계획:** 1단계는 단일 commit으로 진행. 회귀 발생 시 `git revert <commit>` → 즉시 원복.

## 6. 단계 완료 정의 (Definition of Done)

- [ ] `object/built_in.cpp` 삭제됨
- [ ] `object/builtins/` 디렉토리에 7개 .cpp 존재
- [ ] 46개 Builtin 함수가 빠짐 없이 새 위치에 존재 (네이밍/시그니처 보존)
- [ ] `CMakeLists.txt`의 `HONGIK_CORE_SOURCES` 업데이트
- [ ] `cmake --build cmake-build-debug` 성공
- [ ] `ctest --test-dir cmake-build-debug` 345개 통과
- [ ] 새 warning 0건
- [ ] 단일 commit (메시지: `refactor(object): split built_in.cpp into category files`)
- [ ] 메모리 인덱스에 1단계 완료 기록 + 2단계 시작점 명시

## 7. 후속 단계 미리보기 (이번 설계서 범위 밖)

1단계 완료 후 별도 설계서로 진행:

- **2단계 — `vm/vm.cpp`**: `run()` 거대 switch를 opcode 카테고리별 private 메서드로 추출 → `vm.cpp` + `vm_opcode_handlers.cpp` 분리.
- **3단계 — `vm/compiler.cpp`**: 메서드는 이미 분리되어 있으므로 `compiler_stmt.cpp` + `compiler_expr.cpp`로 파일만 분할.
- **4단계 — `evaluator/evaluator.cpp`**: `eval()`의 dynamic_cast chain을 `evalStatementNode`/`evalExpressionNode`로 1차 추출 → `evaluator_stmt.cpp` + `evaluator_expr.cpp` + 카테고리별 helper 분리. **가장 위험.**
