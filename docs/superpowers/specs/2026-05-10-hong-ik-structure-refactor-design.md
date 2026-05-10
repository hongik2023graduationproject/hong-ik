# hong-ik 구조 리팩토링 설계

- 작성일: 2026-05-10
- 대상: `hong-ik` (C++17/26 한글 인터프리터)
- 목적: **모듈 경계 정리** — 동작은 보존, 구조만 정돈

## 1. 목표와 범위

### 목표

1. 디렉토리 구조를 의미 있는 5축(`frontend`, `runtime`, `backend`, `tools`, `platform`)으로 재편한다.
2. CMake에 모듈별 STATIC 라이브러리 타겟을 도입해 의존 그래프를 컴파일 타임에 강제한다.
3. `util/` 같은 잡동사니 모듈을 해체하고, 경계를 가로지르는 의존성(특히 `util/type_utils.h`)을 제거한다.
4. 전 코드를 `hongik::` 네임스페이스 트리 아래로 모은다.
5. evaluator(트리 워킹)와 vm(바이트코드)을 **둘 다 보존**하면서, 둘이 동일한 frontend·runtime을 공유하도록 백엔드 경계를 명확히 한다.

### 비목표 (이번 리팩토링에서 하지 않음)

- 언어 의미론, 문법, 한글 식별자 변경.
- 클래스·함수의 공개 시그니처 변경.
- 새로운 기능 추가, 성능 최적화, 클래스/함수 이름 일괄 영문화.
- 테스트 커버리지 끌어올리기 — 기존 테스트는 유지하고 회귀 안전망만 보강.
- 코드 스타일 일괄 reformat (별도 PR).

## 2. 현재 상태 진단

### 구조적 문제

- 모든 `.cpp` 가 루트 `CMakeLists.txt` 의 `HONGIK_CORE_SOURCES` 평탄 리스트에 모여 단일 `HongIkLib` 로 빌드된다. 헤더 include 경계가 강제되지 않아 어떤 모듈이든 어떤 모듈이든 include할 수 있다.
- `util/type_utils.h` 가 `token/`, `object/`, `vm/` 세 모듈을 동시에 include — 3-way 결합을 만들어 모든 잠재적 경계 분리를 막고 있다.
- `error/hongik_error.{h,cpp}` 와 `exception/exception.h` 가 같은 관심사(에러 처리)를 두 디렉토리에 분산.
- `repl/repl.cpp` 는 이름과 달리 파이프라인 드라이버(파일 모드 + REPL 모드 양쪽 구동) 역할.
- `web/memory_filesystem.h` 는 헤더만 존재하고 CMake 빌드에 포함되지 않음 — 죽은 코드 가능성.
- 단편적 네임스페이스(`utf8::`, `json_util::`, `token_utils::`)는 있지만 `hongik::` 같은 일관된 최상위 네임스페이스 없음.

### 보존되어야 할 것

- **두 백엔드 공존**: `evaluator/`(트리 워킹)와 `vm/`(바이트코드 컴파일러+VM) 둘 다 빌드/테스트 대상이며 의도된 대체 경로. 리팩토링은 이 둘을 **각각 독립된 백엔드 모듈**로 두고, 공통 frontend/runtime 위에서 분기하도록 정리한다.
- 한글 식별자(`TokenType::정수`, `ObjectType::배열` 등) — 이 언어의 정체성.
- 네이티브(C++26) + WASM(C++20, Emscripten) 듀얼 빌드.
- 기존 테스트(lexer/utf8/parser/evaluator/repl/vm/web_module).

## 3. 목표 디렉토리 레이아웃

```
hong-ik/
├── apps/
│   ├── cli/                       # 옛 main.cpp
│   └── wasm/                      # 옛 web/{wasm_interface,bindings}.cpp
│
├── src/hongik/
│   ├── frontend/                  # 소스 → AST
│   │   ├── token/                 # 옛 token/ + util/token_utils.h
│   │   ├── lexer/                 # 옛 lexer/
│   │   ├── parser/                # 옛 parser/
│   │   └── ast/                   # 옛 ast/
│   │
│   ├── runtime/                   # 양쪽 백엔드가 공유
│   │   ├── object/                # 옛 object/ + util/type_utils.h 의 object 변환부
│   │   └── environment/           # 옛 environment/
│   │
│   ├── backend/
│   │   ├── tree/                  # 옛 evaluator/
│   │   └── vm/                    # 옛 vm/ + util/type_utils.h 의 OpCode 변환부
│   │
│   ├── tools/
│   │   └── analyzer/              # 옛 analyzer/ — IDE 신택스 하이라이팅
│   │
│   ├── platform/                  # 횡단 관심사
│   │   ├── utf8/                  # 옛 utf8_converter/ + util/utf8_utils.h 통합
│   │   ├── io/                    # 옛 io/
│   │   ├── error/                 # 옛 error/ + exception/ 통합
│   │   ├── sandbox/               # 옛 sandbox/
│   │   └── json/                  # 옛 util/json_util.h
│   │
│   └── driver/                    # 옛 repl/ — 파이프라인 오케스트레이션 (REPL은 driver의 한 모드)
│
├── tests/                         # gtest. 디렉토리 미러링 (frontend/, backend/ ...)
├── docs/
├── scripts/
├── vscode-extension/              # 변경 없음
└── CMakeLists.txt                 # add_subdirectory 트리로 분해
```

### include 정책

- 모든 헤더는 `.cpp` 옆에 둔다. 별도 `include/` 디렉토리는 두지 않음 (외부 공개 API 소비자가 없는 내부 프로젝트라 분리 비용 > 가치).
- 루트 CMake가 `target_include_directories(... PUBLIC src/)` 를 모든 모듈에 부여 → include 형식은 `#include "hongik/<module>/<file>.h"` 로 통일.
- 상대 경로 include(`#include "../foo/bar.h"`) 금지.

### `util/` 4개 헤더 분해 매핑

| 옛 위치 | 새 위치 |
|---|---|
| `util/token_utils.h` | `src/hongik/frontend/token/token_utils.h` |
| `util/utf8_utils.h` + `utf8_converter/` | `src/hongik/platform/utf8/` (한 모듈로 통합) |
| `util/json_util.h` | `src/hongik/platform/json/json_util.h` |
| `util/type_utils.h` (object 관련) | `src/hongik/runtime/object/object_conversions.h` |
| `util/type_utils.h` (OpCode 관련) | `src/hongik/backend/vm/opcode_symbols.h` |

## 4. 모듈 경계 및 의존성 규칙

### 의존 DAG

```
                   ┌─────────────────────────────────┐
                   │  platform/                      │
                   │   utf8 · json · error · sandbox │
                   │   io                            │   (leaf — 누구에게도 의존 안 함)
                   └────────────┬────────────────────┘
                                │
                ┌───────────────┴────────────────────┐
                ▼                                    ▼
         ┌─────────────┐                      ┌─────────────┐
         │  frontend/  │                      │  runtime/   │
         │  token      │                      │  object     │
         │  lexer      │ ◄── token            │  environment│ ── object
         │  parser     │ ◄── token, ast       └──────┬──────┘
         │  ast        │ ◄── token                   │
         └──────┬──────┘                             │
                │                                    │
                └────────────────┬───────────────────┘
                                 ▼
                       ┌──────────────────┐
                       │  backend/        │
                       │  ├ tree (eval.)  │ ── frontend/ast, runtime/*
                       │  └ vm (compile+  │ ── frontend/ast, runtime/*
                       │       execute)   │
                       └──────────┬───────┘
                                  │
                       ┌──────────┴──────────┐
                       ▼                     ▼
                  ┌──────────┐         ┌──────────┐
                  │ driver/  │         │ tools/   │
                  │          │         │ analyzer │ ── frontend/lexer, frontend/token
                  └────┬─────┘         └──────────┘
                       │
              ┌────────┴────────┐
              ▼                 ▼
          ┌────────┐       ┌─────────┐
          │apps/cli│       │apps/wasm│
          └────────┘       └─────────┘
```

### 규칙

1. **platform/** 모듈은 어디서나 의존 가능. platform 내부에서는 단방향만 허용 (예: sandbox/io 가 utf8/error 사용 가능하나 그 역은 불가).
2. **frontend/** 는 token 기반. lexer/parser 는 token 의존. parser 는 ast 의존. **runtime/이나 backend/를 알지 못한다.**
3. **runtime/** 은 object 와 environment 만. **frontend/ 또는 backend/ 에 의존 금지** — 양쪽이 공유하기 위함.
4. **backend/tree** 와 **backend/vm** 은 서로를 모른다. 각자 frontend/ast + runtime/* 만 사용.
5. **tools/analyzer** 는 frontend/lexer, frontend/token 까지만 사용. 실행 경로(backend/, driver/)와 분리.
6. **driver/** 는 모든 백엔드를 알고 어떤 백엔드로 실행할지 결정. 백엔드 추상 인터페이스는 **YAGNI 원칙**: 두 백엔드를 공통 인터페이스로 묶을 자연스러운 호출 시점이 발견되지 않으면 추가하지 않는다.
7. **apps/** 는 driver를 호출하는 얇은 진입점. 비즈니스 로직 금지.

### 의도적으로 깨는 기존 결합

- `util/type_utils.h` 의 3-way 결합(token + object + vm) → 분해해 각 도메인 내부로 이동.
- 평탄 단일 라이브러리 → 모듈별 STATIC 타겟. 잘못된 include는 링크 에러로 즉시 드러남.

## 5. Phase 별 이행 계획

각 Phase 는 독립 PR. 종료 조건은 **빌드 + 모든 ctest 그린 + WASM 빌드 성공**.

### Phase 0 — 안전망 구축

- 현재 main 빌드 + `ctest` 그린 확인. 깨진 테스트가 있으면 멈추고 사용자와 상의 (별도 작업).
- 골든 출력 통합 테스트 추가 (`tests/golden_test.cpp` + `tests/fixtures/golden/*.hik`, `*.expected.txt`). 두 백엔드 모두로 실행.
- WASM 빌드 1회 수행, 산출물(`hongik-wasm.js`) 사이즈 + 실행 가능 여부를 기준치로 기록.
- `web/memory_filesystem.h` 가 어디서 include되는지 grep. 미사용이면 제거, 사용 중이면 그대로 둠.
- `HONGIK_ENABLE_SANITIZERS=ON` 으로 ASan + UBSan 빌드 1회 수행.

산출물: PR — 테스트 추가만, 코드 이동 없음.

### Phase 1 — 디렉토리 골격 + 단순 이동

- `src/hongik/`, `apps/cli/`, `apps/wasm/` 생성.
- 모든 `.cpp/.h` 를 새 경로로 이동 (이름 변경 없음). 단:
  - `repl/repl.{h,cpp}` → `src/hongik/driver/driver.{h,cpp}` (이름 변경)
  - `main.cpp` → `apps/cli/main.cpp`
  - `web/wasm_interface.cpp`, `web/bindings.cpp` → `apps/wasm/`
- `util/` 4개 헤더 해체 (3장의 매핑대로).
- `error/hongik_error.{h,cpp}` 와 `exception/exception.h` 를 모두 `src/hongik/platform/error/` 로 단순 이동 (통합은 Phase 2).
- 모든 `#include "../foo/bar.h"` → `#include "hongik/<module>/bar.h"` 로 일괄 교체 (스크립트).
- 루트 `CMakeLists.txt` 는 여전히 단일 `HongIkLib` 유지. 단 `HONGIK_CORE_SOURCES` 의 경로만 새 위치로 갱신, `target_include_directories(... PUBLIC src/)` 추가.

산출물: PR — 거대한 파일 이동 diff. 빌드/테스트는 Phase 0과 동일하게 그린.

### Phase 2 — 경계 정리 (실제 코드 변경)

- `platform/error/` 안에서 `hongik_error` 와 `exception` 를 통합. 중복/유사 타입 제거, 호출처 수정.
- Phase 1 에서 분해된 두 헤더(`object_conversions.h`, `opcode_symbols.h`)가 각 모듈 내부에서만 쓰이는지 확인.
- driver의 evaluator/vm 호출 인터페이스 점검. 일관성이 떨어지면 정리. (백엔드 추상 인터페이스 도입은 호출 패턴이 자연스럽게 요구할 때만.)
- analyzer 가 lexer 내부를 너무 깊게 들여다보면 좁은 facade 헤더 추가 검토.

산출물: PR — 코드 변경 있음, ctest 그린 유지.

### Phase 3 — CMake 타겟 분해 (경계 강제)

각 모듈에 자체 `CMakeLists.txt`. 의존 관계는 4장의 DAG 그대로.

예시:

```cmake
# src/hongik/frontend/lexer/CMakeLists.txt
add_library(hongik_lexer STATIC lexer.cpp)
target_link_libraries(hongik_lexer
    PUBLIC hongik_token hongik_platform_utf8)

# src/hongik/backend/tree/CMakeLists.txt
add_library(hongik_backend_tree STATIC evaluator.cpp)
target_link_libraries(hongik_backend_tree
    PUBLIC hongik_ast hongik_runtime_object hongik_runtime_environment)
# hongik_lexer/parser/vm 은 의도적으로 링크하지 않음 — backend/tree는 그것들을 알면 안 됨
```

루트 `CMakeLists.txt` 의 `HONGIK_CORE_SOURCES` 평탄 리스트 제거, `add_subdirectory(...)` 트리로 교체.

이 단계에서 숨어있던 의존성 위반이 링크 에러로 드러남. 발견 시:
- (a) DAG에 맞게 호출 측 수정
- (b) 의존이 정당하면 DAG에 추가하고 사유를 이 문서에 기록

산출물: PR — CMake 분해, 발견된 위반 수정.

### Phase 4 — 네임스페이스 정착

- 전 hongik 코드를 `namespace hongik { ... }` 안으로. 하위 모듈은 `hongik::frontend::lexer`, `hongik::backend::tree`, `hongik::platform::utf8` 등.
- 의존성 leaf 부터 거꾸로 적용:
  1. `platform/{json, utf8, error, sandbox, io}`
  2. `frontend/token`
  3. `frontend/{lexer, parser, ast}`
  4. `runtime/{object, environment}`
  5. `backend/{tree, vm}`
  6. `tools/analyzer`, `driver/`
  7. `apps/{cli, wasm}`
- 기존 단편 네임스페이스(`utf8::`, `json_util::`, `token_utils::`)는 `hongik::platform::utf8`, `hongik::platform::json`, `hongik::frontend::token` 으로 흡수.
- 마지막에 sanitizer 빌드 1회 추가 검증.

산출물: 모듈 단위로 작은 PR(들). 대략 Phase 4 만 4–7개 PR. 끝나면 모든 hongik 심볼이 `hongik::` 아래.

### 총 PR 수 예상

5–9개 (Phase 4의 분할 정도에 따라).

## 6. 테스트 안전망 전략

### Phase 0 에서 추가

**골든 출력 통합 테스트** (`tests/golden_test.cpp`)

대표 `.hik` 스크립트 5–10개를 `tests/fixtures/golden/` 에 두고, 각각의 기대 stdout을 `.expected.txt` 로 기록. 두 백엔드(evaluator, vm)로 모두 실행 → 캡처한 출력이 골든과 정확히 일치해야 함.

선정 기준:

- 변수 선언/할당 (정수, 실수, 문자, 논리, 배열, 사전)
- 조건문 (`만약 / 아니면`)
- 반복문 + `중단` / `계속`
- 함수 정의 + 재귀 (피보나치)
- 내장 함수 (`출력`, `길이`, `추가`)
- UTF-8 문자열 처리 (한글 포함)
- 에러 케이스 (별도 파일에서 stderr 메시지 패턴 매칭)

이 테스트는 동시에 두 백엔드의 동등성도 검증한다.

**CI 게이트** (`.github/workflows/build.yml`)

각 Phase PR 머지 조건:

- 네이티브 빌드 + `ctest` 그린
- WASM 빌드 + `hongik-wasm.js` 산출 확인

**위생 장치**

- ASan + UBSan 빌드를 Phase 0 과 Phase 4 종료 시점에 각각 1회.
- `clang-format` 일괄 적용은 리팩토링과 분리 (포맷 변경이 다른 변경과 섞이면 diff 리뷰 불가능).

### Phase 별 검증 매트릭스

| Phase | ctest 그린 | WASM 빌드 | 골든 (eval) | 골든 (vm) | Sanitizers |
|-------|-----------|-----------|-------------|-----------|------------|
| 0     | ✓ 기준    | ✓ 기준    | ✓ 새로 추가 | ✓ 새로 추가 | ✓ 1회      |
| 1     | ✓         | ✓         | ✓           | ✓         | —          |
| 2     | ✓         | ✓         | ✓           | ✓         | —          |
| 3     | ✓         | ✓         | ✓           | ✓         | —          |
| 4     | ✓         | ✓         | ✓           | ✓         | ✓ 마지막   |

## 7. 네임스페이스와 명명 규약

### 네임스페이스 트리

```
hongik
├── frontend
│   ├── token
│   ├── lexer
│   ├── parser
│   └── ast
├── runtime
│   ├── object
│   └── environment
├── backend
│   ├── tree                (옛 evaluator)
│   └── vm
├── tools
│   └── analyzer
├── platform
│   ├── utf8                (옛 namespace utf8 → 흡수)
│   ├── io
│   ├── error
│   ├── sandbox
│   └── json                (옛 namespace json_util → json 으로 단축)
└── driver                  (옛 repl)
```

### 적용 규약

- 모든 hongik 코드는 `namespace hongik { ... }` 안. 글로벌 식별자 금지.
- 모듈 내부 헤더는 해당 모듈 네임스페이스까지 적용 (`namespace hongik::frontend::lexer`).
- 한글 식별자(`TokenType::정수`, `ObjectType::배열`)는 그대로 유지.
- 헤더에서 `using namespace` 금지. `.cpp` 에서도 가급적 회피.
- 타 모듈 타입 사용 시 단축 alias 허용: `namespace ast = hongik::frontend::ast;` (`.cpp` 한정).

### 헤더 가드

`#pragma once` 와 매크로 가드 `HONGIK_<MODULE>_<FILE>_H` 둘 다 사용. 매크로 가드는 모듈 경로를 반영 (예: `HONGIK_FRONTEND_LEXER_LEXER_H`).

### 파일명

소문자 + 언더스코어. 신규 파일에만 적용, 기존 파일 일괄 개명은 안 함.

## 8. 명시적 비목표

- 클래스/함수 이름 변경.
- 한글 식별자를 영어로 번역.
- API 시그니처 변경.
- 새 단위 테스트 대량 추가.
- 테스트 자체 리팩토링 (Phase 1 에서 디렉토리 미러링용 위치 이동만).
- 벤치마크 자동 회귀 게이트.
- 코드 스타일 일괄 reformat.

## 9. 위험과 완화

| 위험 | 완화 |
|---|---|
| Phase 1의 거대한 diff가 머지 충돌을 유발 | 해당 PR 머지를 우선 처리. 다른 작업은 잠시 hold. |
| `util/type_utils.h` 분해로 새 헤더(`object_conversions.h`, `opcode_symbols.h`)가 도리어 어색한 위치에 놓임 | Phase 2에서 호출 패턴 보고 더 자연스러운 위치로 재배치 가능. |
| Phase 3 CMake 분해 시 다수의 의존성 위반 발견 | 발견 자체가 가치. 위반마다 (a)호출 측 수정 (b)정당하면 DAG 추가+문서화 양 갈래 결정. |
| 두 백엔드의 미세한 동작 차이가 골든 테스트에서 한쪽만 깨짐 | 리팩토링 이전에 이미 있던 차이라면 별도 이슈로 분리. 리팩토링이 만든 차이라면 즉시 롤백. |
| WASM 빌드만 깨지는 경우(네이티브 그린) | CI에 WASM 빌드 게이트 포함 — 매 PR 자동 검증. |
| Emscripten 의 C++20 vs 네이티브 C++26 차이로 새 코드가 한쪽에서만 컴파일 | Phase 1에서 새 파일 작성 안 함. 기존 코드 이동만. Phase 2 이후 새 코드는 양쪽 빌드 둘 다 통과해야 함. |
