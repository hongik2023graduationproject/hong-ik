# 구조

## 실행 파이프라인

```
소스코드(한글) -> Lexer -> Parser -> Compiler -> VM -> 결과        (기본)
                  |         |          |          |
               Tokens     AST     Bytecode     Stack

             또는 (--eval)

소스코드(한글) -> Lexer -> Parser -> Evaluator -> 결과
                  |         |          |
               Tokens     AST    Environment
```

VM은 소스코드를 바이트코드로 컴파일한 뒤 스택 기반 가상 머신에서 실행합니다. `main.cpp`가 파일 모드와 REPL을 가르고, `repl/`이 파이프라인을 구동합니다.

## 디렉터리

| 경로 | 내용 |
|---|---|
| `lexer/` | 소스코드를 토큰으로 변환 |
| `token/` | 토큰 타입 정의 |
| `parser/` | 토큰을 AST로 변환 |
| `ast/` | AST 노드 정의 |
| `analyzer/` | 신택스 하이라이팅, 심볼 수집, 정적 타입 검사기 |
| `vm/` | 바이트코드 컴파일러, 피홀 최적화, 가상 머신(기본 백엔드) |
| `evaluator/` | AST 순회 실행(`--eval`) |
| `environment/` | 변수와 스코프 관리 |
| `object/` | 런타임 객체와 내장 함수 |
| `error/` | 구조화된 에러 타입(HongIkError, 위치 정보) |
| `exception/` | 예외 정의 |
| `io/` | 콜백 기반 I/O 인터페이스(IOContext: 콘솔, 웹) |
| `sandbox/` | 실행 시간과 루프 반복 제한(ExecutionLimiter) |
| `util/` | 공용 유틸(넘침 검사 정수 연산, 사용자 파일 경로 검사, 슬라이스 경계, 정적 타입 모델, JSON, UTF-8) |
| `utf8_converter/` | UTF-8 인코딩 처리 |
| `repl/` | REPL과 파일 실행 |
| `lsp/` | Language Server(`hongik-lsp` 타깃) |
| `web/` | WASM(Emscripten) 바인딩과 인메모리 파일시스템(`hongik-wasm` 타깃) |
| `vscode-extension/` | VSCode 확장(문법 하이라이팅, 스니펫, LSP 클라이언트) |
| `benchmarks/` | Google Benchmark 스위트(`HongIkBenchmarks` 타깃) |
| `scripts/` | 빌드 스크립트(`build-wasm.sh`) |
| `docs/` | 설계 스펙·계획, 벤치마크 결과 |
| `tests/` | Google Test 단위 테스트(`HongIkTests` 타깃) |

## 지켜야 할 규칙

- 내장 함수는 `object/builtin_registry.cpp`에 등록합니다. 정적 검사용 시그니처는 `object/builtin_signatures.h`에 있습니다.
- 네이티브 빌드(C++26)와 WASM 빌드(C++20)는 같은 코어 소스(`HONGIK_CORE_SOURCES`)를 컴파일합니다.
