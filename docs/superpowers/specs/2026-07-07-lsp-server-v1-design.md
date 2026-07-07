# hong-ik LSP 서버 v1 설계

Revision: v1 (2026-07-07)

**Goal:** VSCode에서 `.hik` 파일을 열면 실시간 진단·호버·자동완성·정의로 이동·문서 심볼이 동작하는 LSP 서버를 제공한다. "완성도 쇼케이스" 트랙의 첫 프로젝트 — 기존 lexer/parser/analyzer 자산을 IDE 경험으로 승격시킨다.

**배경:** 현재 vscode-extension은 TextMate 하이라이팅 + 스니펫뿐이다. 반면 코어에는 정적 타입 검사기(`analyzer/type_checker*`, `TypeDiagnostic{line, column}`)와 파서 panic-mode 에러 복구(`parser.cpp` Parsing 루프: 예외 시 `skipToNextLine()` 후 계속)가 이미 있어, LSP의 핵심 재료가 갖춰져 있다. 격차는 ① 파서 에러가 비구조화 문자열(`errors.push_back(e.what())`), ② AST `Node`에 `line`만 있고 column/범위 없음, ③ LSP 프로토콜 레이어 부재.

---

## 아키텍처 결정

| # | 항목 | 결정 | 근거 |
|---|---|---|---|
| 1 | 서버 형태 | C++ 단일 바이너리 `hongik-lsp`, stdio 위 JSON-RPC 2.0 | 기존 lexer/parser/analyzer 정적 링크 재사용. 대안(TS 서버가 CLI 호출, WASM 브리지)은 레이턴시·구조화 열세 |
| 2 | JSON 라이브러리 | nlohmann/json (header-only, CMake FetchContent) | 사실상 표준, 의존성 부담 최소 |
| 3 | LSP 프레임워크 | 사용 안 함 — 프로토콜 레이어 직접 구현 | v1 요청 종류 ~8개 규모에서 프레임워크가 과함. 직접 구현이 쇼케이스 가치도 있음 |
| 4 | 문서 동기화 | `TextDocumentSyncKind.Full` (전문 전송) | 교육·데모 규모 파일에서 full reparse는 ms 단위 — incremental은 YAGNI |
| 5 | 클라이언트 | 기존 vscode-extension에 `vscode-languageclient` 추가 | TextMate 하이라이팅은 유지 (semantic tokens는 v2) |

## Phase 0 — 코어 정지작업 (LSP 품질의 결정 지점)

LSP 기능 구현 전에 코어가 위치 정보를 구조화해 내보내야 한다.

### P0-1. 파서 진단 구조화

- `ParseDiagnostic { long long line; long long column; long long endColumn; std::string message; }` 도입.
- 파서 예외 클래스들(`UnexpectedTokenException` 등)은 이미 line을 갖고 있음 — column/endColumn 전파를 추가하고, `Parser::Parsing`의 catch에서 문자열 대신 `ParseDiagnostic`을 수집한다.
- 기존 `getErrors()`(`vector<string>`)는 진단에서 문자열을 생성하는 형태로 유지 — REPL/CLI 출력 등 기존 소비자 무변경.

### P0-2. AST 노드에 대표 토큰 보관

- `Node`에 `std::shared_ptr<Token> token;` 추가 (대표=첫 토큰). 기존 `line` 필드는 `token`에서 유도 가능해지지만 v1에서는 병행 유지(소비자 수정 최소화).
- 파서의 각 노드 생성부에서 대표 토큰을 채운다. **position→노드 lookup(호버·정의로 이동)의 전제 조건.**
- 채우지 못한 노드는 `token == nullptr` 허용 — LSP 쪽에서 해당 노드는 lookup 대상에서 제외 (기능 저하일 뿐 크래시 아님).

### P0-3. 심볼 테이블 방출

- 타입 체커가 스코프 진입/탈퇴를 이미 추적하므로, 체크 과정에서 `SymbolTable`을 부수 산출물로 방출한다.
- 엔트리: `{ 이름, 종류(변수|함수|클래스|파라미터|필드), 선언 위치(line, column, endColumn), 타입 문자열, 스코프 범위(시작/끝 line) }`.
- 내장 함수 45종은 `object/builtin_signatures.h`에서 로드해 전역 심볼로 합류.

## 컴포넌트 (`lsp/` 신설)

| 파일 | 역할 |
|---|---|
| `lsp/jsonrpc.{h,cpp}` | stdio Content-Length framing, 메시지 파싱/직렬화, 요청 디스패치 |
| `lsp/server.{h,cpp}` | 라이프사이클(initialize/shutdown/exit), capabilities 광고, 요청→feature 라우팅 |
| `lsp/document_store.{h,cpp}` | URI→열린 문서(텍스트, 버전, 파싱 결과·심볼 테이블 캐시) |
| `lsp/features/diagnostics.cpp` | ParseDiagnostic + TypeDiagnostic → `publishDiagnostics` |
| `lsp/features/hover.cpp` | position→노드/심볼 lookup → 타입·시그니처 마크다운 |
| `lsp/features/completion.cpp` | 키워드 + 내장 함수 45종 + 커서 위치 스코프의 심볼 |
| `lsp/features/definition.cpp` | 식별자 position → 심볼 테이블 선언 위치 |
| `lsp/features/symbols.cpp` | 문서 심볼(함수·클래스·전역 변수) 아웃라인 |
| `lsp/main.cpp` | `hongik-lsp` 엔트리포인트 (별도 CMake 타깃) |

vscode-extension: `client/` 추가(`vscode-languageclient`), 서버 바이너리 경로 설정(`hongik.lsp.path`) + 미설정 시 PATH 탐색. 바이너리 번들은 v1 범위 외 — 설정 안내 문서로 대체.

## 데이터 플로

```
didOpen/didChange → document_store 갱신 → Lexer → Parser(panic-mode, 부분 AST + ParseDiagnostic[])
                                        → TypeChecker(warn 모드, TypeDiagnostic[] + SymbolTable)
                                        → publishDiagnostics (심각도 매핑은 아래 참조)
hover/completion/definition/documentSymbol → 캐시된 부분 AST + SymbolTable 조회 (재파싱 없음)
```

- 진단 심각도: 파서 진단 = `Error`, 타입 체커 `error()` = `Error`, `warn()` = `Warning`.
- 위치 좌표 변환: hong-ik 내부는 1-based line/column, LSP는 0-based — 변환은 `lsp/` 경계에서 단일 함수로 일원화 (오프바이원 사고 방지).
- **UTF-16 코드 유닛 규칙:** LSP position의 character는 UTF-16 기준. 한글은 UTF-16 1유닛이므로 대부분 일치하지만, 내부 column이 바이트 기준이라면 변환 필수 — Phase 0에서 내부 column의 단위(바이트/코드포인트)를 실측하고 변환 함수를 확정한다.

## 에러 처리

- 파싱 일부 실패 → 부분 AST로 hover/completion 계속 동작 (panic-mode 복구 덕분).
- 알 수 없는 메서드 → JSON-RPC `MethodNotFound`. 잘못된 파라미터 → `InvalidParams`. 요청 처리 중 C++ 예외 → `InternalError` 응답 후 서버 계속 (요청 단위 격리, 서버 프로세스는 죽지 않음).
- 서버 크래시 → vscode-languageclient 기본 자동 재시작에 위임.

## 테스트 전략 (TDD)

- `jsonrpc`: framing 왕복(Content-Length 파싱, 부분 수신, 연속 메시지) 단위 테스트.
- feature별 골든 테스트: "문서 텍스트 + 요청 JSON → 기대 응답 JSON" — 기존 GoogleTest 스위트(`HongIkTests`)에 통합, stdio 없이 디스패치 함수 직접 호출.
- Phase 0: ParseDiagnostic 위치 정확성 테스트(멀티 에러 파일), Node.token 채움 검증, SymbolTable 스냅샷 테스트.
- E2E: VSCode 수동 검증 시나리오 문서 + 데모 GIF (자동화는 v1 범위 외).

## v1 범위 제외 (YAGNI)

incremental sync · semantic tokens · rename · find-references · formatting · workspace 심볼(멀티 파일) · `가져오기` 모듈 간 정의로 이동 · 서버 바이너리 자동 다운로드/번들. 전부 v2 이후 판단.

## 리스크

| 리스크 | 심각도 | 대응 |
|---|---|---|
| P0-2가 파서 노드 생성부 전체를 건드림 (가장 침습적) | 중 | 노드 타입별 순차 적용 + `nullptr` 허용 설계로 부분 적용 가능. 기존 테스트 스위트가 회귀 안전망 |
| 내부 column 단위(바이트 vs 코드포인트) 불명 → 한글 위치 어긋남 | 중 | Phase 0 첫 작업으로 실측·확정. 골든 테스트에 한글 식별자 케이스 필수 포함 |
| 타입 체커가 에러 복구된 부분 AST에서 크래시 가능성 | 중 | Phase 0에서 깨진 AST 입력 퍼징성 테스트 추가, null 방어 |
| Windows stdio 바이너리 모드(CRLF 변환) 문제 | 저 | `_setmode(_fileno(stdin), _O_BINARY)` 초기화, framing 테스트로 검증 |
