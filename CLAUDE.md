# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 프로젝트 개요

**hong-ik**은 한글 키워드 기반의 프로그래밍 언어 인터프리터입니다. C++17/26으로 작성되었으며, CMake + Google Test를 사용합니다.

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

## 아키텍처

전형적인 인터프리터 파이프라인을 따릅니다:

```
소스코드(한글) → Lexer → Parser → Evaluator → 결과
                  ↓        ↓          ↓
               Tokens     AST    Environment
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
- `repl/repl.h`: Lexer → Parser → Evaluator 파이프라인 구동
- `object/object.h`: 런타임 값 표현 (Integer, Boolean, String, Function, Array, ReturnValue, Builtin)
- `environment/environment.h`: 외부 스코프 참조를 포함한 중첩 환경 구현
