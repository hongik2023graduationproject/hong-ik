# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 프로젝트 개요

**hong-ik**은 한글 키워드 기반의 프로그래밍 언어 인터프리터입니다. 네이티브 빌드는 C++26, WASM(Emscripten) 빌드는 C++20으로 컴파일하며, CMake + Google Test를 사용합니다.

## 빌드 및 테스트 명령어

```bash
# 빌드 설정
cmake -B cmake-build-debug

# 빌드
cmake --build cmake-build-debug

# 전체 테스트 실행
ctest --test-dir cmake-build-debug

# 특정 테스트 실행 (예: 렉서 테스트만)
cmake-build-debug/HongIkTests --gtest_filter="LexerTest.*"

# 인터프리터 실행 (파일 모드)
./cmake-build-debug/HongIk <filename.hik>

# 인터프리터 실행 (REPL 모드)
./cmake-build-debug/HongIk
```

코드 포맷: `.clang-format` 기준 (LLVM 스타일, 4칸 들여쓰기, 120자 제한)

### Sanitizer

CI(`.github/workflows/sanitizers.yml`)는 Linux clang에서 ASan+UBSan과 TSan을 따로 돌립니다. 로컬 Windows에서 재현할 수 있는 범위는 아래와 같습니다.

```bash
# MSVC ASan (UBSan·TSan·LeakSanitizer는 MSVC 미지원)
cmake -B build-asan -G "Visual Studio 17 2022" -A x64 -DHONGIK_ENABLE_SANITIZERS=ON -DHONGIK_SANITIZERS=address
cmake --build build-asan --config RelWithDebInfo
# 실행 전 ASan 런타임 DLL 폴더를 PATH에 넣는다. 없으면 각 테스트가 0xc0000135 오류 창을 띄우고 멈춘다.
export PATH="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/<버전>/bin/Hostx64/x64:$PATH"
ctest --test-dir build-asan --build-config RelWithDebInfo --output-on-failure --timeout 120

# MinGW g++ UBSan: 런타임이 없어 trap 모드만 가능 (메시지 없이 0xC000001D로 종료)
cmake -B build-ubsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fsanitize-trap=undefined"
```

- ctest에는 `--timeout`을 준다.
- Git Bash에서 `/O2`처럼 `/`로 시작하는 MSVC 옵션을 넘기면 MSYS가 경로로 바꾼다. `-O2` 형식을 쓴다.

## 아키텍처

전형적인 인터프리터 파이프라인을 따릅니다:

```
소스코드(한글) → Lexer → Parser → Compiler → VM (기본)      → 결과
                  ↓        ↓    └→ Evaluator (--eval, 트리워킹) ↗
               Tokens     AST
```

### 주요 컴포넌트

| 디렉토리 | 역할 |
|----------|------|
| `analyzer/` | 신택스 하이라이팅 + 정적 타입 검사기(`type_checker*` — `--type-check=off\|warn\|strict`, 기본 warn) |
| `vm/` | 바이트코드 컴파일러 + VM — **기본 실행 백엔드** (트리워킹은 `--eval`) |
| `lexer/` | 소스코드를 토큰으로 변환 |
| `token/` | 토큰 타입 정의 (`token_type.h`) |
| `parser/` | 토큰 → AST 변환 |
| `ast/` | AST 노드 정의 (statements, expressions, literals) |
| `evaluator/` | AST를 순회하며 실행 |
| `environment/` | 변수/스코프 관리 |
| `object/` | 런타임 객체 타입 및 내장 함수 |
| `utf8_converter/` | 한글(UTF-8) 인코딩 처리 |
| `repl/` | 파일 실행 및 REPL 진입점, 파이프라인 조율 |
| `exception/` | 예외 정의 |
| `tests/` | Google Test 기반 단위 테스트 |

### 언어 문법 요약

```
// 변수 선언 (선언 타입 검사 — 암묵 승격 없음, Optional은 타입?)
정수 x = 10
실수? r = 없음

// 조건문 (블록 스코프 — 블록 내 선언은 밖에서 안 보임)
만약 조건 라면:
    ...
아니면:
    ...

// 함수 선언/호출
함수 더하기(정수 a, 정수 b) -> 정수:
    리턴 a + b
더하기(1, 2)

// 클래스 (상속: 클래스 자식 < 부모)
클래스 점:
    정수 x
    생성(정수 a):
        자기.x = a
    함수 값() -> 정수:
        리턴 자기.x
```

내장 함수 45종 — 단일 진실의 원천은 `object/builtin_registry.cpp` (정적 검사용 시그니처는 `object/builtin_signatures.h`).

### 핵심 데이터 흐름

- `main.cpp`: UTF-8 콘솔 설정 후 파일/REPL 모드 분기
- `repl/repl.h`: Lexer → Parser → Compiler → VM 파이프라인 구동 (`--eval`이면 Evaluator)
- `object/object.h`: 런타임 값 표현 (Integer, Boolean, String, Function, Array, ReturnValue, Builtin)
- `environment/environment.h`: 외부 스코프 참조를 포함한 중첩 환경 구현
