# hong-ik LSP 서버 v1 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** VSCode에서 `.hik` 파일에 실시간 진단·호버·자동완성·정의로 이동·문서 심볼을 제공하는 `hongik-lsp` 바이너리 + VSCode 클라이언트.

**Architecture:** C++ 단일 바이너리(stdio JSON-RPC 2.0, 프레임워크 없이 직접 구현)가 기존 HongIkLib(lexer/parser/analyzer)를 정적 링크. 문서 변경 시 전문(full-text) 재분석 → 진단 push, 나머지 기능은 캐시된 토큰 스트림 + 심볼 테이블 조회. spec: `docs/superpowers/specs/2026-07-07-lsp-server-v1-design.md` (v1.1).

**Tech Stack:** C++26, nlohmann/json v3.11.3 (FetchContent), GoogleTest, vscode-languageclient ^9 (TypeScript).

## Global Constraints

- 빌드/테스트: `cmake --build cmake-build-debug` / `ctest --test-dir cmake-build-debug`. 특정 테스트: `cmake-build-debug/HongIkTests --gtest_filter="..."`
- **기존 에러 메시지 문자열은 바이트 단위로 보존** — 골든 테스트가 에러 출력 텍스트에 의존한다. 예외 리팩토링 시 메시지 생성 코드를 그대로 옮길 것.
- 좌표 규약: 내부 = line 1-based / column 0-based **코드포인트** (lexer 실측). LSP = line 0-based / character 0-based **UTF-16 코드 유닛**. 변환은 `lsp/position.h` 함수로만 수행.
- 코드 스타일: `.clang-format` (LLVM, 4칸, 120자). 새 LSP 코드는 `lsp::` 네임스페이스 사용.
- WASM(EMSCRIPTEN) 빌드는 건드리지 않는다 — LSP 소스는 native 분기에만 추가.
- 각 태스크 완료 시 **전체 테스트 통과 확인 후 커밋** (커밋 메시지에 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>` 푸터).
- 모든 새 파일은 `hong-ik/` 저장소 루트 기준 경로.

## 파일 구조 (최종 상태)

```
exception/exception.h            수정: ParseError 베이스 도입 (기존 예외 6종이 상속)
parser/parser.h,.cpp             수정: ParseDiagnostic 수집 + getDiagnostics()
ast/statements.h                 수정: 선언 노드 5종에 nameToken (+ClassStatement.fieldNameTokens)
ast/expressions.h                수정: IdentifierExpression.token
lexer/lexer.h,.cpp               수정: static Keywords() 접근자
analyzer/symbol_collector.h,.cpp 신규: AST 순회 → SymbolInfo[]
lsp/position.h,.cpp              신규: cp↔UTF-16 변환, splitLines, cpLength
lsp/jsonrpc.h,.cpp               신규: Content-Length framing + Dispatcher
lsp/document_store.h,.cpp        신규: 문서 캐시 + 분석 파이프라인
lsp/server.h,.cpp                신규: 라이프사이클 + 라우팅 + publishDiagnostics
lsp/features/diagnostics.h,.cpp  신규: 진단 → LSP JSON 변환
lsp/features/hover.h,.cpp        신규: tokenAt + hover
lsp/features/completion.h,.cpp   신규: completion
lsp/features/definition.h,.cpp   신규: definition
lsp/features/symbols.h,.cpp      신규: documentSymbol
lsp/main.cpp                     신규: hongik-lsp 엔트리포인트
CMakeLists.txt                   수정: nlohmann/json + HongIkLspLib + hongik-lsp 타깃
tests/parser_diagnostics_test.cpp, tests/symbol_collector_test.cpp,
tests/lsp_position_test.cpp, tests/lsp_jsonrpc_test.cpp,
tests/lsp_document_store_test.cpp, tests/lsp_server_test.cpp   신규
vscode-extension/                수정: LSP 클라이언트 (extension.ts, package.json, tsconfig.json)
```

---

### Task 1: 파서·렉서 진단 구조화 (P0-1)

**Files:**
- Modify: `exception/exception.h`
- Modify: `parser/parser.h`, `parser/parser.cpp`
- Create: `tests/parser_diagnostics_test.cpp`
- Modify: `CMakeLists.txt` (HongIkTests 소스에 테스트 파일 추가)

**Interfaces:**
- Produces: `struct ParseDiagnostic { long long line, column, endColumn; std::string message; }` (parser.h), `const std::vector<ParseDiagnostic>& Parser::getDiagnostics() const`, `class ParseError : public std::exception` — `line/column/endColumn` public 필드 (exception.h). Task 6이 소비.
- 기존 `Parser::getErrors()`(`vector<string>`)와 모든 에러 메시지 문자열은 **무변경**.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/parser_diagnostics_test.cpp`

```cpp
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>

namespace {
std::shared_ptr<Program> parseWith(Parser& parser, const std::string& src) {
    Lexer lexer;
    return parser.Parsing(lexer.Tokenize(Utf8Converter::Convert(src)));
}
} // namespace

TEST(ParserDiagnosticsTest, StructuredPositionsForMultipleErrors) {
    Parser parser;
    // 1행: '=' 뒤 표현식 없음(개행에서 에러), 3행: 동일 에러 — 2행은 정상
    parseWith(parser, "정수 x = ;\n정수 y = 2\n실수 z = ;\n");
    const auto& diags = parser.getDiagnostics();
    ASSERT_EQ(diags.size(), 2u);
    EXPECT_EQ(diags[0].line, 1);
    EXPECT_EQ(diags[1].line, 3);
    // "정수 x = ;" 코드포인트: 정(0)수(1) ␣(2) x(3) ␣(4) =(5) ␣(6) ;(7) → ';' 토큰 위치
    EXPECT_EQ(diags[0].column, 7);
    EXPECT_EQ(diags[0].endColumn, 8);
    EXPECT_FALSE(diags[0].message.empty());
    // 기존 문자열 인터페이스와 개수 일치 (하위호환)
    EXPECT_EQ(parser.getErrors().size(), diags.size());
    EXPECT_EQ(parser.getErrors()[0], diags[0].message);
}

TEST(ParserDiagnosticsTest, DiagnosticsClearedBetweenParses) {
    Parser parser;
    parseWith(parser, "정수 x = ;\n");
    ASSERT_FALSE(parser.getDiagnostics().empty());
    parseWith(parser, "정수 x = 1\n");
    EXPECT_TRUE(parser.getDiagnostics().empty());
}
```

CMakeLists.txt의 `add_executable(HongIkTests ...)` 소스 목록에 `tests/parser_diagnostics_test.cpp` 추가.

- [ ] **Step 2: 실패 확인**

Run: `cmake --build cmake-build-debug && cmake-build-debug/HongIkTests --gtest_filter="ParserDiagnosticsTest.*"`
Expected: 컴파일 실패 (`getDiagnostics` 미정의)

- [ ] **Step 3: 구현**

`exception/exception.h` — 상단 include에 `#include "../token/token.h"` 추가 후, 첫 예외 클래스 앞에 베이스 삽입:

```cpp
// 위치 정보를 보유한 진단용 예외 베이스 (렉서/파서 → LSP·CLI 공용).
// column/endColumn은 0-based 코드포인트, 미상이면 0.
class ParseError : public std::exception {
public:
    long long line = 0;
    long long column = 0;
    long long endColumn = 0;
    ParseError(std::string msg, long long line, long long column, long long endColumn)
        : line(line), column(column), endColumn(endColumn), message_(std::move(msg)) {}
    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};
```

기존 6종 예외를 ParseError 상속으로 전환. **메시지 문자열 생성식은 글자 그대로 유지**하고, private `message` 멤버와 `what()` 오버라이드는 제거(베이스가 제공):

```cpp
// 렉서 2종 — 시그니처 유지 (lexer.cpp 무수정), 위치는 line만
class UnknownCharacterException : public ParseError {
public:
    explicit UnknownCharacterException(std::string c, long long line)
        : ParseError("[줄 " + std::to_string(line) + "] 알 수 없는 문자 '" + c + "'가 입력되었습니다.", line, 0, 0) {}
};

class UnterminatedStringException : public ParseError {
public:
    explicit UnterminatedStringException(std::string s, long long line)
        : ParseError("[줄 " + std::to_string(line) + "] 문자열: " + s
                         + " 이 닫는 따옴표 없이 끝났습니다. '\"'가 필요합니다.",
                     line, 0, 0) {}
};

// 파서 4종 — Token 참조로 전환 (위치 전파)
class UnknownPrefixParseFunctionException : public ParseError {
public:
    UnknownPrefixParseFunctionException(const TokenType type, const Token& tok)
        : ParseError("[줄 " + std::to_string(tok.line) + "] 해당 토큰 타입 '" + TokenTypeToString(type)
                         + "' 에 대한 prefix 파서 함수가 정의되어 있지 않습니다.",
                     tok.line, tok.column, tok.endColumn) {}
};

class UnexpectedTokenException : public ParseError {
public:
    UnexpectedTokenException(TokenType got, TokenType expected, const Token& tok)
        : ParseError("[줄 " + std::to_string(tok.line)
                         + "] 예상하지 못한 토큰입니다. "
                           "현재 토큰: '"
                         + TokenTypeToString(got) + "', 예상 토큰: '" + TokenTypeToString(expected) + "'",
                     tok.line, tok.column, tok.endColumn) {}
};

class UnknownInfixParseFunctionException : public ParseError {
public:
    UnknownInfixParseFunctionException(const TokenType type, const Token& tok)
        : ParseError("[줄 " + std::to_string(tok.line) + "] 해당 토큰 타입 '" + TokenTypeToString(type)
                         + "' 에 대한 infix 파서 함수가 정의되어 있지 않습니다.",
                     tok.line, tok.column, tok.endColumn) {}
};

class NoTokenException : public ParseError {
public:
    NoTokenException(const Token& lastTok, TokenType expected)
        : ParseError("[줄 " + std::to_string(lastTok.line) + "] 토큰 타입: " + TokenTypeToString(expected)
                         + "이 예상되었으나 토큰이 존재하지 않습니다.",
                     lastTok.line, lastTok.column, lastTok.endColumn) {}
};
```

`parser/parser.h` — 클래스 정의 앞에 struct 추가, 클래스에 멤버/게터 추가:

```cpp
struct ParseDiagnostic {
    long long line = 0;
    long long column = 0;     // 0-based 코드포인트
    long long endColumn = 0;
    std::string message;
};
```

```cpp
    const std::vector<std::string>& getErrors() const { return errors; }
    const std::vector<ParseDiagnostic>& getDiagnostics() const { return diagnostics; }
private:
    std::vector<ParseDiagnostic> diagnostics;
```

`parser/parser.cpp` 수정 지점:
1. `initialization()`의 `errors.clear();` 옆에 `diagnostics.clear();` 추가.
2. `Parsing()`의 catch 블록 교체:

```cpp
        } catch (const ParseError& e) {
            errors.push_back(e.what());
            diagnostics.push_back({e.line, e.column, e.endColumn, e.what()});
            skipToNextLine();
        } catch (const std::exception& e) {
            errors.push_back(e.what());
            diagnostics.push_back({current_token ? current_token->line : 0, 0, 0, e.what()});
            skipToNextLine();
        }
```

3. throw 지점 시그니처 갱신 (parser.cpp:69, 72, 79, 82, 687, 698):
   - `NoTokenException(tokens[current_read_position - 1]->line, type)` → `NoTokenException(*tokens[current_read_position - 1], type)`
   - `UnexpectedTokenException(current_token->type, type, current_token->line)` → `UnexpectedTokenException(current_token->type, type, *current_token)`
   - `UnknownPrefixParseFunctionException(current_token->type, current_token->line)` → `UnknownPrefixParseFunctionException(current_token->type, *current_token)`
   - `UnknownInfixParseFunctionException(next_token->type, next_token->line)` → `UnknownInfixParseFunctionException(next_token->type, *next_token)`

- [ ] **Step 4: 통과 확인 + 회귀 없음**

Run: `cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug`
Expected: 전체 PASS (골든 테스트 포함 — 에러 메시지 불변 검증)

- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(parser): structured ParseDiagnostic with token positions (LSP P0-1)"`

---

### Task 2: 선언 노드 이름 토큰 (P0-2')

**Files:**
- Modify: `ast/statements.h`, `ast/expressions.h`, `parser/parser.cpp`
- Modify: `tests/parser_diagnostics_test.cpp` (테스트 추가)

**Interfaces:**
- Produces: `InitializationStatement::nameToken`, `FunctionStatement::nameToken`, `ClassStatement::nameToken` + `ClassStatement::fieldNameTokens`, `ForEachStatement::nameToken`, `ForRangeStatement::nameToken` (모두 `std::shared_ptr<Token>`, 미설정 시 nullptr), `IdentifierExpression::token`. Task 4(SymbolCollector)가 소비.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/parser_diagnostics_test.cpp`에 추가

```cpp
TEST(DeclNameTokenTest, FunctionAndVariableCarryNameTokens) {
    Parser parser;
    auto program = parseWith(parser,
                             "정수 개수 = 3\n"
                             "함수 더하기(정수 a, 정수 b) -> 정수:\n"
                             "    리턴 a + b\n");
    ASSERT_TRUE(parser.getErrors().empty());
    ASSERT_EQ(program->statements.size(), 2u);

    auto init = std::dynamic_pointer_cast<InitializationStatement>(program->statements[0]);
    ASSERT_NE(init, nullptr);
    ASSERT_NE(init->nameToken, nullptr);
    EXPECT_EQ(init->nameToken->text, "개수");
    EXPECT_EQ(init->nameToken->line, 1);
    EXPECT_EQ(init->nameToken->column, 3);  // 정(0)수(1)␣(2)개(3)수(4)

    auto fn = std::dynamic_pointer_cast<FunctionStatement>(program->statements[1]);
    ASSERT_NE(fn, nullptr);
    ASSERT_NE(fn->nameToken, nullptr);
    EXPECT_EQ(fn->nameToken->text, "더하기");
    ASSERT_EQ(fn->parameters.size(), 2u);
    ASSERT_NE(fn->parameters[0]->token, nullptr);
    EXPECT_EQ(fn->parameters[0]->token->text, "a");
}

TEST(DeclNameTokenTest, ClassCarriesNameAndFieldTokens) {
    Parser parser;
    auto program = parseWith(parser,
                             "클래스 점:\n"
                             "    정수 x\n"
                             "    생성(정수 값):\n"
                             "        자기.x = 값\n");
    ASSERT_TRUE(parser.getErrors().empty());
    auto cls = std::dynamic_pointer_cast<ClassStatement>(program->statements[0]);
    ASSERT_NE(cls, nullptr);
    ASSERT_NE(cls->nameToken, nullptr);
    EXPECT_EQ(cls->nameToken->text, "점");
    ASSERT_EQ(cls->fieldNameTokens.size(), 1u);
    EXPECT_EQ(cls->fieldNameTokens[0]->text, "x");
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build cmake-build-debug && cmake-build-debug/HongIkTests --gtest_filter="DeclNameTokenTest.*"`
Expected: 컴파일 실패 (`nameToken` 미정의)

- [ ] **Step 3: 구현**

`ast/expressions.h` — IdentifierExpression에 멤버 추가:

```cpp
class IdentifierExpression : public Expression {
public:
    std::string name;
    std::shared_ptr<Token> token;  // 이름 토큰 (LSP 위치용, 없으면 nullptr)
```

`ast/statements.h` — 각 선언 노드에 멤버 추가 (기존 name/fieldNames 필드 바로 아래):
- `InitializationStatement`, `FunctionStatement`, `ForEachStatement`(elementName 아래), `ForRangeStatement`(varName 아래), `ClassStatement`(name 아래)에 각각 `std::shared_ptr<Token> nameToken;  // 이름 토큰 (LSP 위치용, 없으면 nullptr)`
- `ClassStatement`에 추가로 `std::vector<std::shared_ptr<Token>> fieldNameTokens;  // fieldNames와 1:1`

`parser/parser.cpp` — 이름을 읽는 지점마다 토큰 저장 (모두 `current_token`이 이름 토큰인 시점):
- `statement->name = current_token->text;` 지점(256, 312, 459, 615행 — 각각 Initialization/CompoundAssignment 여부와 무관하게 **선언 5종 노드에만**) 직후에 `statement->nameToken = current_token;` 추가. ※ 270행처럼 대상이 `AssignmentStatement`인 곳은 선언이 아니므로 추가하지 않는다 — 각 지점이 속한 parse 함수명으로 판별할 것 (`parseInitializationStatement`, `parseFunctionStatement`, `parseClassStatement`, `parseForEachStatement`의 `elementName`, `parseForRangeStatement`의 `varName`).
- 411행 `statement->elementName = ...` 직후 / 1015행 `statement->varName = ...` 직후에도 `statement->nameToken = current_token;`
- 643행 `statement->fieldNames.push_back(current_token->text);` 직후에 `statement->fieldNameTokens.push_back(current_token);`
- `make_shared<IdentifierExpression>()` 생성 지점(286, 481, 658, 765, 772, 779, 984행)에서 `ident->name = ...` 설정 직후 `ident->token = current_token;` 추가 (자기/부모 합성 노드도 해당 키워드 토큰을 저장).

- [ ] **Step 4: 통과 확인 + 회귀 없음**

Run: `cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug`
Expected: 전체 PASS

- [ ] **Step 5: 커밋** — `git commit -am "feat(ast,parser): name tokens on declaration nodes and identifiers (LSP P0-2)"`

---

### Task 3: CMake LSP 골격 + 좌표 변환 유틸

**Files:**
- Modify: `CMakeLists.txt`
- Create: `lsp/position.h`, `lsp/position.cpp`, `tests/lsp_position_test.cpp`

**Interfaces:**
- Produces (namespace `lsp`): `long long cpToUtf16(const std::string& lineText, long long cpColumn)`, `long long utf16ToCp(const std::string& lineText, long long utf16Col)`, `long long cpLength(const std::string& lineText)`, `std::vector<std::string> splitLines(const std::string& text)`. CMake 타깃 `HongIkLspLib`(nlohmann_json 포함). Task 5~11이 소비.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/lsp_position_test.cpp`

```cpp
#include "../lsp/position.h"
#include <gtest/gtest.h>

TEST(LspPositionTest, AsciiIsIdentity) {
    EXPECT_EQ(lsp::cpToUtf16("abc def", 4), 4);
    EXPECT_EQ(lsp::utf16ToCp("abc def", 4), 4);
}

TEST(LspPositionTest, HangulBmpIsOneUnitPerCodepoint) {
    EXPECT_EQ(lsp::cpToUtf16("정수 x = 10", 3), 3);  // '정','수',' ' → 'x' 앞
    EXPECT_EQ(lsp::utf16ToCp("정수 x = 10", 3), 3);
}

TEST(LspPositionTest, AstralCharTakesTwoUtf16Units) {
    const std::string s = "\xF0\x9F\x98\x80가";  // 😀가
    EXPECT_EQ(lsp::cpToUtf16(s, 1), 2);   // 😀 다음 = utf16으로 2
    EXPECT_EQ(lsp::utf16ToCp(s, 2), 1);
    EXPECT_EQ(lsp::utf16ToCp(s, 3), 2);
}

TEST(LspPositionTest, OutOfRangeClampsToEnd) {
    EXPECT_EQ(lsp::cpToUtf16("ab", 99), 2);
    EXPECT_EQ(lsp::utf16ToCp("ab", 99), 2);
    EXPECT_EQ(lsp::cpLength("정수 x"), 4);
}

TEST(LspPositionTest, SplitLinesHandlesLfAndCrLf) {
    auto lines = lsp::splitLines("a\r\nb\nc");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    EXPECT_EQ(lines[2], "c");
    EXPECT_TRUE(lsp::splitLines("").size() == 1u && lsp::splitLines("")[0].empty());
}
```

- [ ] **Step 2: CMake 배선 후 실패 확인**

`CMakeLists.txt` native 분기(`else()` 이후), googletest `FetchContent_MakeAvailable` 다음에 추가:

```cmake
    # ---- LSP 서버 (native 전용) ----
    FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)

    set(HONGIK_LSP_SOURCES
            lsp/position.cpp
    )
    add_library(HongIkLspLib STATIC ${HONGIK_LSP_SOURCES})
    target_include_directories(HongIkLspLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(HongIkLspLib PUBLIC HongIkLib nlohmann_json::nlohmann_json)
    hongik_apply_sanitizers(HongIkLspLib)
```

`HongIkTests` 소스에 `tests/lsp_position_test.cpp` 추가, `target_link_libraries(HongIkTests ...)`에 `HongIkLspLib` 추가.

Run: `cmake -B cmake-build-debug && cmake --build cmake-build-debug`
Expected: 컴파일 실패 (lsp/position.h 없음)

- [ ] **Step 3: 구현**

`lsp/position.h`:

```cpp
#ifndef LSP_POSITION_H
#define LSP_POSITION_H

#include <string>
#include <vector>

// 좌표 규약 (spec v1.1):
//   내부  = line 1-based, column 0-based 코드포인트 (Utf8Converter가 코드포인트 단위 분해)
//   LSP   = line 0-based, character 0-based UTF-16 코드 유닛
// 변환은 반드시 이 모듈을 통해서만 수행한다 (오프바이원 일원화).
namespace lsp {

long long cpToUtf16(const std::string& lineText, long long cpColumn);
long long utf16ToCp(const std::string& lineText, long long utf16Col);
long long cpLength(const std::string& lineText);
// '\n' 기준 분리, 각 줄 끝 '\r' 제거. 빈 입력도 빈 줄 1개를 반환한다.
std::vector<std::string> splitLines(const std::string& text);

} // namespace lsp

#endif // LSP_POSITION_H
```

`lsp/position.cpp`:

```cpp
#include "position.h"

namespace {
int utf8SeqLen(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xE) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1;  // 잘못된 리드 바이트 — 1바이트씩 전진 (방어)
}
} // namespace

namespace lsp {

long long cpToUtf16(const std::string& lineText, long long cpColumn) {
    long long cp = 0, u16 = 0;
    size_t i = 0;
    while (i < lineText.size() && cp < cpColumn) {
        int len = utf8SeqLen(static_cast<unsigned char>(lineText[i]));
        u16 += (len == 4) ? 2 : 1;  // BMP 밖(4바이트)만 서로게이트 쌍
        i += len;
        cp++;
    }
    return u16;
}

long long utf16ToCp(const std::string& lineText, long long utf16Col) {
    long long cp = 0, u16 = 0;
    size_t i = 0;
    while (i < lineText.size() && u16 < utf16Col) {
        int len = utf8SeqLen(static_cast<unsigned char>(lineText[i]));
        u16 += (len == 4) ? 2 : 1;
        i += len;
        cp++;
    }
    return cp;
}

long long cpLength(const std::string& lineText) {
    long long cp = 0;
    size_t i = 0;
    while (i < lineText.size()) {
        i += utf8SeqLen(static_cast<unsigned char>(lineText[i]));
        cp++;
    }
    return cp;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            if (!current.empty() && current.back() == '\r') current.pop_back();
            lines.push_back(std::move(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty() && current.back() == '\r') current.pop_back();
    lines.push_back(std::move(current));
    return lines;
}

} // namespace lsp
```

- [ ] **Step 4: 통과 확인** — `cmake-build-debug/HongIkTests --gtest_filter="LspPositionTest.*"` → PASS, 이후 `ctest --test-dir cmake-build-debug` 전체 PASS
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(lsp): position conversion utils + HongIkLspLib target"`

---

### Task 4: SymbolCollector

**Files:**
- Create: `analyzer/symbol_collector.h`, `analyzer/symbol_collector.cpp`, `tests/symbol_collector_test.cpp`
- Modify: `CMakeLists.txt` (`HONGIK_LSP_SOURCES`에 `analyzer/symbol_collector.cpp`, 테스트 소스 추가)

**Interfaces:**
- Consumes: Task 2의 nameToken 필드들.
- Produces:

```cpp
enum class DocSymbolKind { Variable, Function, Class, Field, Method, Parameter };
struct SymbolInfo {
    std::string name;
    DocSymbolKind kind;
    std::string typeText;   // "정수", "실수?", "함수 더하기(정수 a) -> 정수", "클래스 점" 등
    std::string container;  // 톱레벨이면 "", 클래스 멤버는 클래스명, 함수 내부는 함수명
    long long line = 0;     // 1-based. nameToken 없으면 노드 line
    long long column = 0;   // 0-based cp. nameToken 없으면 0 (endColumn도 0)
    long long endColumn = 0;
};
class SymbolCollector {
public:
    std::vector<SymbolInfo> collect(const std::shared_ptr<Program>& program);
};
```

Task 6~10이 소비. 규약: `container == ""` 가 문서 톱레벨.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/symbol_collector_test.cpp`

```cpp
#include "../analyzer/symbol_collector.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>

namespace {
std::vector<SymbolInfo> collectFrom(const std::string& src) {
    Lexer lexer;
    Parser parser;
    auto program = parser.Parsing(lexer.Tokenize(Utf8Converter::Convert(src)));
    SymbolCollector collector;
    return collector.collect(program);
}
const SymbolInfo* find(const std::vector<SymbolInfo>& v, const std::string& name) {
    for (const auto& s : v)
        if (s.name == name) return &s;
    return nullptr;
}
} // namespace

TEST(SymbolCollectorTest, CollectsGlobalsFunctionsClassesAndLocals) {
    auto symbols = collectFrom(
        "정수 개수 = 3\n"
        "실수? 비율 = 없음\n"
        "함수 더하기(정수 a, 정수 b) -> 정수:\n"
        "    정수 합 = a + b\n"
        "    리턴 합\n"
        "클래스 점:\n"
        "    정수 x\n"
        "    생성(정수 값):\n"
        "        자기.x = 값\n"
        "    함수 값얻기() -> 정수:\n"
        "        리턴 자기.x\n");

    auto* cnt = find(symbols, "개수");
    ASSERT_NE(cnt, nullptr);
    EXPECT_EQ(cnt->kind, DocSymbolKind::Variable);
    EXPECT_EQ(cnt->typeText, "정수");
    EXPECT_EQ(cnt->container, "");
    EXPECT_EQ(cnt->line, 1);
    EXPECT_EQ(cnt->column, 3);

    auto* ratio = find(symbols, "비율");
    ASSERT_NE(ratio, nullptr);
    EXPECT_EQ(ratio->typeText, "실수?");

    auto* add = find(symbols, "더하기");
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->kind, DocSymbolKind::Function);
    EXPECT_EQ(add->typeText, "함수 더하기(정수 a, 정수 b) -> 정수");

    auto* sum = find(symbols, "합");
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->container, "더하기");

    auto* paramA = find(symbols, "a");
    ASSERT_NE(paramA, nullptr);
    EXPECT_EQ(paramA->kind, DocSymbolKind::Parameter);
    EXPECT_EQ(paramA->container, "더하기");

    auto* cls = find(symbols, "점");
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->kind, DocSymbolKind::Class);

    auto* fieldX = find(symbols, "x");
    ASSERT_NE(fieldX, nullptr);
    EXPECT_EQ(fieldX->kind, DocSymbolKind::Field);
    EXPECT_EQ(fieldX->container, "점");

    auto* method = find(symbols, "값얻기");
    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->kind, DocSymbolKind::Method);
    EXPECT_EQ(method->container, "점");
}

TEST(SymbolCollectorTest, CollectsLoopVariables) {
    auto symbols = collectFrom(
        "반복 정수 i = 0 부터 10 까지:\n"
        "    출력(i)\n"
        "각각 정수 요소 [1, 2] 에서:\n"
        "    출력(요소)\n");
    EXPECT_NE(find(symbols, "i"), nullptr);
    EXPECT_NE(find(symbols, "요소"), nullptr);
}

TEST(SymbolCollectorTest, SurvivesPartialAst) {
    auto symbols = collectFrom("정수 x = ;\n정수 y = 2\n");
    EXPECT_NE(find(symbols, "y"), nullptr);  // 1행이 깨져도 2행 심볼은 수집
}
```

※ `각각` 문법이 위와 다르면(예: 요소 선언 형태) `tests/` 기존 evaluator/vm 테스트에서 실제 문법을 확인해 테스트 소스만 맞출 것 — 프로덕션 코드를 테스트에 맞추지 말 것.

- [ ] **Step 2: 실패 확인** — 빌드 에러 (symbol_collector.h 없음)

- [ ] **Step 3: 구현**

`analyzer/symbol_collector.h`:

```cpp
#ifndef SYMBOL_COLLECTOR_H
#define SYMBOL_COLLECTOR_H

#include "../ast/program.h"
#include "../ast/statements.h"
#include <memory>
#include <string>
#include <vector>

enum class DocSymbolKind { Variable, Function, Class, Field, Method, Parameter };

struct SymbolInfo {
    std::string name;
    DocSymbolKind kind;
    std::string typeText;
    std::string container;
    long long line = 0;
    long long column = 0;
    long long endColumn = 0;
};

// AST 1회 순회로 선언 심볼을 수집한다 (LSP hover/completion/definition/documentSymbol 재료).
// hong-ik은 선언 타입 명시 언어라 타입 추론 없이 문법에서 typeText를 얻는다 (spec v1.1 P0-3).
class SymbolCollector {
public:
    std::vector<SymbolInfo> collect(const std::shared_ptr<Program>& program);

private:
    void walkStatement(const std::shared_ptr<Statement>& stmt, const std::string& container,
                       std::vector<SymbolInfo>& out);
    void walkBlock(const std::shared_ptr<BlockStatement>& block, const std::string& container,
                   std::vector<SymbolInfo>& out);
    void addFunction(const std::shared_ptr<FunctionStatement>& fn, const std::string& container,
                     DocSymbolKind kind, std::vector<SymbolInfo>& out);
};

#endif // SYMBOL_COLLECTOR_H
```

`analyzer/symbol_collector.cpp`:

```cpp
#include "symbol_collector.h"

namespace {

SymbolInfo makeInfo(std::string name, DocSymbolKind kind, std::string typeText, std::string container,
                    const std::shared_ptr<Token>& nameToken, long long fallbackLine) {
    SymbolInfo info;
    info.name = std::move(name);
    info.kind = kind;
    info.typeText = std::move(typeText);
    info.container = std::move(container);
    if (nameToken) {
        info.line = nameToken->line;
        info.column = nameToken->column;
        info.endColumn = nameToken->endColumn;
    } else {
        info.line = fallbackLine;
    }
    return info;
}

std::string functionSignature(const FunctionStatement& fn) {
    std::string s = "함수 " + fn.name + "(";
    for (size_t i = 0; i < fn.parameters.size(); ++i) {
        if (i) s += ", ";
        if (i < fn.parameterTypes.size() && fn.parameterTypes[i]) {
            s += fn.parameterTypes[i]->text;
            if (i < fn.parameterOptionals.size() && fn.parameterOptionals[i]) s += "?";
            s += " ";
        }
        s += fn.parameters[i]->name;
    }
    s += ")";
    if (fn.returnType) {
        s += " -> " + fn.returnType->text;
        if (fn.returnTypeOptional) s += "?";
    }
    return s;
}

} // namespace

std::vector<SymbolInfo> SymbolCollector::collect(const std::shared_ptr<Program>& program) {
    std::vector<SymbolInfo> out;
    if (!program) return out;
    for (const auto& stmt : program->statements) walkStatement(stmt, "", out);
    return out;
}

void SymbolCollector::walkBlock(const std::shared_ptr<BlockStatement>& block, const std::string& container,
                                std::vector<SymbolInfo>& out) {
    if (!block) return;
    for (const auto& stmt : block->statements) walkStatement(stmt, container, out);
}

void SymbolCollector::addFunction(const std::shared_ptr<FunctionStatement>& fn, const std::string& container,
                                  DocSymbolKind kind, std::vector<SymbolInfo>& out) {
    out.push_back(makeInfo(fn->name, kind, functionSignature(*fn), container, fn->nameToken, fn->line));
    for (size_t i = 0; i < fn->parameters.size(); ++i) {
        const auto& p = fn->parameters[i];
        std::string typeText = (i < fn->parameterTypes.size() && fn->parameterTypes[i]) ? fn->parameterTypes[i]->text : "";
        if (i < fn->parameterOptionals.size() && fn->parameterOptionals[i]) typeText += "?";
        out.push_back(makeInfo(p->name, DocSymbolKind::Parameter, typeText, fn->name, p->token, fn->line));
    }
    walkBlock(fn->body, fn->name, out);
}

void SymbolCollector::walkStatement(const std::shared_ptr<Statement>& stmt, const std::string& container,
                                    std::vector<SymbolInfo>& out) {
    if (!stmt) return;
    if (auto init = std::dynamic_pointer_cast<InitializationStatement>(stmt)) {
        std::string typeText = init->type ? init->type->text : "";
        if (init->isOptional) typeText += "?";
        out.push_back(makeInfo(init->name, DocSymbolKind::Variable, typeText, container, init->nameToken, init->line));
    } else if (auto fn = std::dynamic_pointer_cast<FunctionStatement>(stmt)) {
        addFunction(fn, container, DocSymbolKind::Function, out);
    } else if (auto cls = std::dynamic_pointer_cast<ClassStatement>(stmt)) {
        out.push_back(makeInfo(cls->name, DocSymbolKind::Class, "클래스 " + cls->name, container, cls->nameToken, cls->line));
        for (size_t i = 0; i < cls->fieldNames.size(); ++i) {
            std::string typeText = (i < cls->fieldTypes.size() && cls->fieldTypes[i]) ? cls->fieldTypes[i]->text : "";
            auto tok = i < cls->fieldNameTokens.size() ? cls->fieldNameTokens[i] : nullptr;
            out.push_back(makeInfo(cls->fieldNames[i], DocSymbolKind::Field, typeText, cls->name, tok, cls->line));
        }
        for (size_t i = 0; i < cls->constructorParams.size(); ++i) {
            const auto& p = cls->constructorParams[i];
            std::string typeText =
                (i < cls->constructorParamTypes.size() && cls->constructorParamTypes[i]) ? cls->constructorParamTypes[i]->text : "";
            out.push_back(makeInfo(p->name, DocSymbolKind::Parameter, typeText, cls->name, p->token, cls->line));
        }
        walkBlock(cls->constructorBody, cls->name, out);
        for (const auto& m : cls->methods) addFunction(m, cls->name, DocSymbolKind::Method, out);
    } else if (auto fe = std::dynamic_pointer_cast<ForEachStatement>(stmt)) {
        out.push_back(makeInfo(fe->elementName, DocSymbolKind::Variable,
                               fe->elementType ? fe->elementType->text : "", container, fe->nameToken, fe->line));
        walkBlock(fe->body, container, out);
    } else if (auto fr = std::dynamic_pointer_cast<ForRangeStatement>(stmt)) {
        out.push_back(makeInfo(fr->varName, DocSymbolKind::Variable, fr->varType ? fr->varType->text : "", container,
                               fr->nameToken, fr->line));
        walkBlock(fr->body, container, out);
    } else if (auto iff = std::dynamic_pointer_cast<IfStatement>(stmt)) {
        walkBlock(iff->consequence, container, out);
        walkBlock(iff->then, container, out);
    } else if (auto wh = std::dynamic_pointer_cast<WhileStatement>(stmt)) {
        walkBlock(wh->body, container, out);
    } else if (auto tc = std::dynamic_pointer_cast<TryCatchStatement>(stmt)) {
        walkBlock(tc->tryBody, container, out);
        walkBlock(tc->catchBody, container, out);
    } else if (auto mt = std::dynamic_pointer_cast<MatchStatement>(stmt)) {
        for (const auto& b : mt->caseBodies) walkBlock(b, container, out);
        walkBlock(mt->defaultBody, container, out);
    } else if (auto blk = std::dynamic_pointer_cast<BlockStatement>(stmt)) {
        walkBlock(blk, container, out);
    }
    // Assignment/Expression/Return/Import 등은 선언이 아님 — 무시
}
```

- [ ] **Step 4: 통과 확인** — `--gtest_filter="SymbolCollectorTest.*"` PASS 후 전체 ctest PASS
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(analyzer): SymbolCollector for LSP symbol table (P0-3)"`

---

### Task 5: JSON-RPC framing + Dispatcher

**Files:**
- Create: `lsp/jsonrpc.h`, `lsp/jsonrpc.cpp`, `tests/lsp_jsonrpc_test.cpp`
- Modify: `CMakeLists.txt` (소스 추가)

**Interfaces:**
- Produces (namespace `lsp`): `std::optional<std::string> readMessage(std::istream&)`, `void writeMessage(std::ostream&, const nlohmann::json&)`, `class Dispatcher` — `onRequest(method, json(const json&))`, `onNotification(method, void(const json&))`, `handle(const json& msg, std::ostream& out)`. Task 7, 11이 소비.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/lsp_jsonrpc_test.cpp`

```cpp
#include "../lsp/jsonrpc.h"
#include <gtest/gtest.h>
#include <sstream>

using nlohmann::json;

TEST(JsonRpcFramingTest, WriteThenReadRoundTrips) {
    std::stringstream ss;
    lsp::writeMessage(ss, json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}});
    auto body = lsp::readMessage(ss);
    ASSERT_TRUE(body.has_value());
    auto parsed = json::parse(*body);
    EXPECT_EQ(parsed["method"], "initialize");
}

TEST(JsonRpcFramingTest, ReadHandlesMultipleMessagesAndHangul) {
    std::stringstream ss;
    lsp::writeMessage(ss, json{{"v", "첫번째"}});
    lsp::writeMessage(ss, json{{"v", "두번째"}});
    EXPECT_EQ(json::parse(*lsp::readMessage(ss))["v"], "첫번째");
    EXPECT_EQ(json::parse(*lsp::readMessage(ss))["v"], "두번째");
    EXPECT_FALSE(lsp::readMessage(ss).has_value());  // EOF
}

TEST(JsonRpcFramingTest, GarbageHeaderReturnsNullopt) {
    std::stringstream ss("Content-Length: abc\r\n\r\n{}");
    EXPECT_FALSE(lsp::readMessage(ss).has_value());
}

TEST(DispatcherTest, RoutesRequestAndWritesResponse) {
    lsp::Dispatcher d;
    d.onRequest("echo", [](const json& p) { return p; });
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 7}, {"method", "echo"}, {"params", {{"x", 1}}}}, out);
    std::istringstream in(out.str());
    auto resp = json::parse(*lsp::readMessage(in));
    EXPECT_EQ(resp["id"], 7);
    EXPECT_EQ(resp["result"]["x"], 1);
}

TEST(DispatcherTest, UnknownRequestReturnsMethodNotFound) {
    lsp::Dispatcher d;
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "nope"}}, out);
    std::istringstream in(out.str());
    auto resp = json::parse(*lsp::readMessage(in));
    EXPECT_EQ(resp["error"]["code"], -32601);
}

TEST(DispatcherTest, HandlerExceptionBecomesInternalError) {
    lsp::Dispatcher d;
    d.onRequest("boom", [](const json&) -> json { throw std::runtime_error("bad"); });
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "boom"}}, out);
    std::istringstream in(out.str());
    auto resp = json::parse(*lsp::readMessage(in));
    EXPECT_EQ(resp["error"]["code"], -32603);
}

TEST(DispatcherTest, NotificationProducesNoOutput) {
    lsp::Dispatcher d;
    bool called = false;
    d.onNotification("note", [&](const json&) { called = true; });
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"method", "note"}}, out);
    EXPECT_TRUE(called);
    EXPECT_TRUE(out.str().empty());
    d.handle(json{{"jsonrpc", "2.0"}, {"method", "unknown-note"}}, out);  // 무시, 크래시 없음
    EXPECT_TRUE(out.str().empty());
}
```

- [ ] **Step 2: 실패 확인** — 빌드 에러

- [ ] **Step 3: 구현**

`lsp/jsonrpc.h`:

```cpp
#ifndef LSP_JSONRPC_H
#define LSP_JSONRPC_H

#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace lsp {

// LSP Base Protocol: "Content-Length: N\r\n\r\n<N바이트 UTF-8 본문>"
std::optional<std::string> readMessage(std::istream& in);
void writeMessage(std::ostream& out, const nlohmann::json& msg);

class Dispatcher {
public:
    using RequestHandler = std::function<nlohmann::json(const nlohmann::json& params)>;
    using NotificationHandler = std::function<void(const nlohmann::json& params)>;

    void onRequest(const std::string& method, RequestHandler handler);
    void onNotification(const std::string& method, NotificationHandler handler);
    // 메시지 1건 처리. 요청이면 응답을 out에 기록, 알림이면 기록 없음.
    void handle(const nlohmann::json& msg, std::ostream& out);

private:
    std::map<std::string, RequestHandler> requests_;
    std::map<std::string, NotificationHandler> notifications_;
};

} // namespace lsp

#endif // LSP_JSONRPC_H
```

`lsp/jsonrpc.cpp`:

```cpp
#include "jsonrpc.h"

namespace lsp {

std::optional<std::string> readMessage(std::istream& in) {
    long long contentLength = -1;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;  // 헤더 종료
        constexpr std::string_view kPrefix = "Content-Length:";
        if (line.rfind(kPrefix, 0) == 0) {
            try {
                contentLength = std::stoll(line.substr(kPrefix.size()));
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    }
    if (!in || contentLength < 0) return std::nullopt;
    std::string body(static_cast<size_t>(contentLength), '\0');
    in.read(body.data(), contentLength);
    if (in.gcount() != contentLength) return std::nullopt;
    return body;
}

void writeMessage(std::ostream& out, const nlohmann::json& msg) {
    const std::string body = msg.dump();
    out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    out.flush();
}

void Dispatcher::onRequest(const std::string& method, RequestHandler handler) {
    requests_[method] = std::move(handler);
}

void Dispatcher::onNotification(const std::string& method, NotificationHandler handler) {
    notifications_[method] = std::move(handler);
}

void Dispatcher::handle(const nlohmann::json& msg, std::ostream& out) {
    const std::string method = msg.value("method", "");
    const nlohmann::json params = msg.contains("params") ? msg["params"] : nlohmann::json::object();

    if (!msg.contains("id")) {  // 알림
        auto it = notifications_.find(method);
        if (it != notifications_.end()) {
            try {
                it->second(params);
            } catch (...) {
                // 알림은 응답 채널이 없다 — 서버 생존 우선, 무시
            }
        }
        return;
    }

    nlohmann::json response = {{"jsonrpc", "2.0"}, {"id", msg["id"]}};
    auto it = requests_.find(method);
    if (it == requests_.end()) {
        response["error"] = {{"code", -32601}, {"message", "method not found: " + method}};
    } else {
        try {
            response["result"] = it->second(params);
        } catch (const std::exception& e) {
            response["error"] = {{"code", -32603}, {"message", e.what()}};
        } catch (...) {
            response["error"] = {{"code", -32603}, {"message", "internal error"}};
        }
    }
    writeMessage(out, response);
}

} // namespace lsp
```

CMake: `HONGIK_LSP_SOURCES`에 `lsp/jsonrpc.cpp`, HongIkTests에 `tests/lsp_jsonrpc_test.cpp` 추가.

- [ ] **Step 4: 통과 확인** — `--gtest_filter="JsonRpcFramingTest.*:DispatcherTest.*"` PASS + 전체 ctest PASS
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(lsp): JSON-RPC framing and dispatcher"`

---

### Task 6: DocumentStore (분석 파이프라인)

**Files:**
- Create: `lsp/document_store.h`, `lsp/document_store.cpp`, `tests/lsp_document_store_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 `ParseDiagnostic`/`ParseError`, Task 3 `splitLines`, Task 4 `SymbolCollector`, 기존 `Lexer/Parser/TypeChecker/Utf8Converter`.
- Produces:

```cpp
struct AnalyzedDocument {
    std::string uri;
    std::string text;
    long long version = 0;
    std::vector<std::string> lines;
    std::vector<std::shared_ptr<Token>> tokens;
    std::shared_ptr<Program> program;  // 렉서 실패 시 nullptr
    std::vector<ParseDiagnostic> parseDiagnostics;
    std::vector<TypeDiagnostic> typeDiagnostics;
    std::vector<SymbolInfo> symbols;
};
class DocumentStore {
    const AnalyzedDocument* open(uri, text, version);
    const AnalyzedDocument* update(uri, text, version);
    void close(uri);
    const AnalyzedDocument* get(uri) const;  // 없으면 nullptr
};
```

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/lsp_document_store_test.cpp`

```cpp
#include "../lsp/document_store.h"
#include <gtest/gtest.h>

TEST(DocumentStoreTest, AnalyzesValidDocument) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///a.hik", "정수 x = 10\n출력(x)\n", 1);
    ASSERT_NE(doc, nullptr);
    EXPECT_TRUE(doc->parseDiagnostics.empty());
    EXPECT_FALSE(doc->tokens.empty());
    EXPECT_FALSE(doc->symbols.empty());
    EXPECT_EQ(doc->symbols[0].name, "x");
}

TEST(DocumentStoreTest, PartialAstOnParseError) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///b.hik", "정수 x = ;\n정수 y = 2\n", 1);
    ASSERT_NE(doc, nullptr);
    EXPECT_FALSE(doc->parseDiagnostics.empty());
    bool foundY = false;
    for (const auto& s : doc->symbols)
        if (s.name == "y") foundY = true;
    EXPECT_TRUE(foundY);  // 부분 AST에서도 심볼 유지
}

TEST(DocumentStoreTest, LexerFailureYieldsSingleDiagnostic) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///c.hik", "정수 x = @\n", 1);
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->parseDiagnostics.size(), 1u);
    EXPECT_EQ(doc->parseDiagnostics[0].line, 1);
    EXPECT_EQ(doc->program, nullptr);
}

TEST(DocumentStoreTest, TypeDiagnosticsReported) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///d.hik", "정수 x = 1\n정수 y = 모름\n", 1);
    ASSERT_NE(doc, nullptr);
    EXPECT_FALSE(doc->typeDiagnostics.empty());  // TC001 미선언 식별자 계열
}

TEST(DocumentStoreTest, UpdateReplacesAndCloseRemoves) {
    lsp::DocumentStore store;
    store.open("file:///e.hik", "정수 x = 1\n", 1);
    const auto* doc = store.update("file:///e.hik", "정수 바뀜 = 2\n", 2);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->symbols[0].name, "바뀜");
    store.close("file:///e.hik");
    EXPECT_EQ(store.get("file:///e.hik"), nullptr);
}
```

※ TypeDiagnosticsReported의 진단 코드가 다르면 실제 발화 코드를 확인해 소스만 조정 (`--type-check=warn` 기본 발화 기준).

- [ ] **Step 2: 실패 확인** — 빌드 에러

- [ ] **Step 3: 구현**

`lsp/document_store.h`:

```cpp
#ifndef LSP_DOCUMENT_STORE_H
#define LSP_DOCUMENT_STORE_H

#include "../analyzer/symbol_collector.h"
#include "../analyzer/type_checker.h"
#include "../parser/parser.h"
#include "../token/token.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lsp {

struct AnalyzedDocument {
    std::string uri;
    std::string text;
    long long version = 0;
    std::vector<std::string> lines;
    std::vector<std::shared_ptr<Token>> tokens;
    std::shared_ptr<Program> program;
    std::vector<ParseDiagnostic> parseDiagnostics;
    std::vector<TypeDiagnostic> typeDiagnostics;
    std::vector<SymbolInfo> symbols;
};

class DocumentStore {
public:
    const AnalyzedDocument* open(const std::string& uri, const std::string& text, long long version);
    const AnalyzedDocument* update(const std::string& uri, const std::string& text, long long version);
    void close(const std::string& uri);
    const AnalyzedDocument* get(const std::string& uri) const;

private:
    static AnalyzedDocument analyze(std::string uri, std::string text, long long version);
    std::map<std::string, AnalyzedDocument> docs_;
};

} // namespace lsp

#endif // LSP_DOCUMENT_STORE_H
```

`lsp/document_store.cpp`:

```cpp
#include "document_store.h"

#include "../exception/exception.h"
#include "../lexer/lexer.h"
#include "../utf8_converter/utf8_converter.h"
#include "position.h"

namespace lsp {

AnalyzedDocument DocumentStore::analyze(std::string uri, std::string text, long long version) {
    AnalyzedDocument doc;
    doc.uri = std::move(uri);
    doc.text = std::move(text);
    doc.version = version;
    doc.lines = splitLines(doc.text);

    try {
        Lexer lexer;
        doc.tokens = lexer.Tokenize(Utf8Converter::Convert(doc.text));
    } catch (const ParseError& e) {
        // 렉서 에러 복구는 v2 — 파일 전체 분석 중단, 단일 진단 (spec v1.1 한계 ③)
        doc.parseDiagnostics.push_back({e.line, e.column, e.endColumn, e.what()});
        return doc;
    } catch (const std::exception& e) {
        doc.parseDiagnostics.push_back({0, 0, 0, e.what()});
        return doc;
    }

    Parser parser;
    doc.program = parser.Parsing(doc.tokens);
    doc.parseDiagnostics = parser.getDiagnostics();

    if (doc.program) {
        try {
            TypeChecker checker;  // 매 분석 새 인스턴스 — REPL용 글로벌 누적이 편집 간 오염을 만들기 때문
            doc.typeDiagnostics = checker.check(doc.program).diagnostics;
        } catch (...) {
            // 에러 복구된 부분 AST 방어 — 타입 진단만 생략하고 계속
        }
        try {
            SymbolCollector collector;
            doc.symbols = collector.collect(doc.program);
        } catch (...) {
            // 동일 방어
        }
    }
    return doc;
}

const AnalyzedDocument* DocumentStore::open(const std::string& uri, const std::string& text, long long version) {
    docs_[uri] = analyze(uri, text, version);
    return &docs_[uri];
}

const AnalyzedDocument* DocumentStore::update(const std::string& uri, const std::string& text, long long version) {
    return open(uri, text, version);
}

void DocumentStore::close(const std::string& uri) {
    docs_.erase(uri);
}

const AnalyzedDocument* DocumentStore::get(const std::string& uri) const {
    auto it = docs_.find(uri);
    return it == docs_.end() ? nullptr : &it->second;
}

} // namespace lsp
```

CMake: `lsp/document_store.cpp` + `tests/lsp_document_store_test.cpp` 추가.

- [ ] **Step 4: 통과 확인** — `--gtest_filter="DocumentStoreTest.*"` PASS + 전체 ctest PASS
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(lsp): DocumentStore analysis pipeline"`

---

### Task 7: Server 라이프사이클 + publishDiagnostics

**Files:**
- Create: `lsp/features/diagnostics.h`, `lsp/features/diagnostics.cpp`, `lsp/server.h`, `lsp/server.cpp`, `tests/lsp_server_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 5 Dispatcher/writeMessage, Task 6 DocumentStore, Task 3 변환 함수.
- Produces: `lsp::Server(std::ostream& out)` — `void bind(Dispatcher&)`, `bool shouldExit() const`. `lsp::features::toRange(doc, line1, col, endCol) -> json`, `lsp::features::buildDiagnostics(doc) -> json array`. Task 8~11이 Server 라우팅에 기능 추가.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/lsp_server_test.cpp`

```cpp
#include "../lsp/jsonrpc.h"
#include "../lsp/server.h"
#include <gtest/gtest.h>
#include <sstream>

using nlohmann::json;

namespace {
// out 스트림에 쌓인 프레임들을 순서대로 파싱
std::vector<json> drain(std::ostringstream& out) {
    std::istringstream in(out.str());
    std::vector<json> msgs;
    while (auto body = lsp::readMessage(in)) msgs.push_back(json::parse(*body));
    out.str("");
    return msgs;
}
json didOpen(const std::string& uri, const std::string& text) {
    return {{"jsonrpc", "2.0"},
            {"method", "textDocument/didOpen"},
            {"params", {{"textDocument", {{"uri", uri}, {"languageId", "hongik"}, {"version", 1}, {"text", text}}}}}};
}
} // namespace

class LspServerTest : public ::testing::Test {
protected:
    std::ostringstream out;
    lsp::Dispatcher d;
    lsp::Server server{out};
    void SetUp() override { server.bind(d); }
};

TEST_F(LspServerTest, InitializeAdvertisesCapabilities) {
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}, {"params", json::object()}}, out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    const auto& caps = msgs[0]["result"]["capabilities"];
    EXPECT_EQ(caps["textDocumentSync"], 1);
    EXPECT_TRUE(caps["hoverProvider"].get<bool>());
    EXPECT_TRUE(caps["definitionProvider"].get<bool>());
    EXPECT_TRUE(caps["documentSymbolProvider"].get<bool>());
    EXPECT_TRUE(caps.contains("completionProvider"));
}

TEST_F(LspServerTest, DidOpenPublishesParseAndTypeDiagnostics) {
    d.handle(didOpen("file:///t.hik", "정수 x = ;\n"), out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0]["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(msgs[0]["params"]["uri"], "file:///t.hik");
    ASSERT_FALSE(msgs[0]["params"]["diagnostics"].empty());
    const auto& diag = msgs[0]["params"]["diagnostics"][0];
    EXPECT_EQ(diag["severity"], 1);
    EXPECT_EQ(diag["range"]["start"]["line"], 0);       // 내부 1-based → LSP 0-based
    EXPECT_EQ(diag["range"]["start"]["character"], 7);  // ';' cp 7 == utf16 7 (한글 BMP)
}

TEST_F(LspServerTest, DidChangeRepublishesAndDidCloseClears) {
    d.handle(didOpen("file:///t.hik", "정수 x = ;\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"},
                  {"method", "textDocument/didChange"},
                  {"params",
                   {{"textDocument", {{"uri", "file:///t.hik"}, {"version", 2}}},
                    {"contentChanges", json::array({{{"text", "정수 x = 1\n"}}})}}}},
             out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_TRUE(msgs[0]["params"]["diagnostics"].empty());  // 고쳐졌으니 빈 배열

    d.handle(json{{"jsonrpc", "2.0"},
                  {"method", "textDocument/didClose"},
                  {"params", {{"textDocument", {{"uri", "file:///t.hik"}}}}}},
             out);
    msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_TRUE(msgs[0]["params"]["diagnostics"].empty());  // 닫으면 진단 클리어
}

TEST_F(LspServerTest, ShutdownThenExitSetsShouldExit) {
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 9}, {"method", "shutdown"}}, out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_TRUE(msgs[0]["result"].is_null());
    EXPECT_FALSE(server.shouldExit());
    d.handle(json{{"jsonrpc", "2.0"}, {"method", "exit"}}, out);
    EXPECT_TRUE(server.shouldExit());
}
```

- [ ] **Step 2: 실패 확인** — 빌드 에러

- [ ] **Step 3: 구현**

`lsp/features/diagnostics.h`:

```cpp
#ifndef LSP_FEATURES_DIAGNOSTICS_H
#define LSP_FEATURES_DIAGNOSTICS_H

#include "../document_store.h"
#include <nlohmann/json.hpp>

namespace lsp::features {

// 내부 좌표(line 1-based, cp column) → LSP Range JSON.
// col==endCol==0(위치 미상)이면 줄 전체를 범위로 잡는다.
nlohmann::json toRange(const AnalyzedDocument& doc, long long line1, long long col, long long endCol);
// parse + type 진단을 LSP Diagnostic 배열로 변환 (parse=Error(1), 타입 ERROR=1/WARNING=2)
nlohmann::json buildDiagnostics(const AnalyzedDocument& doc);

} // namespace lsp::features

#endif // LSP_FEATURES_DIAGNOSTICS_H
```

`lsp/features/diagnostics.cpp`:

```cpp
#include "diagnostics.h"

#include "../position.h"
#include <algorithm>

namespace lsp::features {

nlohmann::json toRange(const AnalyzedDocument& doc, long long line1, long long col, long long endCol) {
    const long long line0 = std::max(0LL, line1 - 1);
    const std::string lineText = line0 < static_cast<long long>(doc.lines.size()) ? doc.lines[line0] : "";
    if (col == 0 && endCol == 0) endCol = std::max(1LL, cpLength(lineText));
    if (endCol <= col) endCol = col + 1;
    return {{"start", {{"line", line0}, {"character", cpToUtf16(lineText, col)}}},
            {"end", {{"line", line0}, {"character", std::max(cpToUtf16(lineText, endCol), cpToUtf16(lineText, col) + 1)}}}};
}

nlohmann::json buildDiagnostics(const AnalyzedDocument& doc) {
    nlohmann::json diags = nlohmann::json::array();
    for (const auto& d : doc.parseDiagnostics) {
        diags.push_back({{"range", toRange(doc, d.line, d.column, d.endColumn)},
                         {"severity", 1},
                         {"source", "hongik"},
                         {"message", d.message}});
    }
    for (const auto& d : doc.typeDiagnostics) {
        diags.push_back({{"range", toRange(doc, d.line, d.column, 0)},
                         {"severity", d.severity == Severity::ERROR ? 1 : 2},
                         {"source", "hongik-type"},
                         {"code", d.code},
                         {"message", d.message}});
    }
    return diags;
}

} // namespace lsp::features
```

`lsp/server.h`:

```cpp
#ifndef LSP_SERVER_H
#define LSP_SERVER_H

#include "document_store.h"
#include "jsonrpc.h"
#include <ostream>

namespace lsp {

class Server {
public:
    explicit Server(std::ostream& out) : out_(out) {}
    void bind(Dispatcher& dispatcher);
    bool shouldExit() const { return exit_; }

private:
    nlohmann::json onInitialize(const nlohmann::json& params);
    void onDidOpen(const nlohmann::json& params);
    void onDidChange(const nlohmann::json& params);
    void onDidClose(const nlohmann::json& params);
    nlohmann::json onHover(const nlohmann::json& params);           // Task 8
    nlohmann::json onCompletion(const nlohmann::json& params);      // Task 9
    nlohmann::json onDefinition(const nlohmann::json& params);      // Task 10
    nlohmann::json onDocumentSymbol(const nlohmann::json& params);  // Task 10

    void publish(const AnalyzedDocument& doc);
    void publishEmpty(const std::string& uri);
    const AnalyzedDocument* docFor(const nlohmann::json& params) const;

    std::ostream& out_;
    DocumentStore store_;
    bool exit_ = false;
};

} // namespace lsp

#endif // LSP_SERVER_H
```

`lsp/server.cpp` (Task 8~10 핸들러는 이 태스크에서는 `return nullptr;` 스텁 — bind에는 등록하지 않고 각 태스크에서 등록):

```cpp
#include "server.h"

#include "features/diagnostics.h"

namespace lsp {

void Server::bind(Dispatcher& d) {
    d.onRequest("initialize", [this](const nlohmann::json& p) { return onInitialize(p); });
    d.onNotification("initialized", [](const nlohmann::json&) {});
    d.onRequest("shutdown", [](const nlohmann::json&) { return nlohmann::json(nullptr); });
    d.onNotification("exit", [this](const nlohmann::json&) { exit_ = true; });
    d.onNotification("textDocument/didOpen", [this](const nlohmann::json& p) { onDidOpen(p); });
    d.onNotification("textDocument/didChange", [this](const nlohmann::json& p) { onDidChange(p); });
    d.onNotification("textDocument/didClose", [this](const nlohmann::json& p) { onDidClose(p); });
}

nlohmann::json Server::onInitialize(const nlohmann::json&) {
    return {{"capabilities",
             {{"textDocumentSync", 1},  // Full
              {"hoverProvider", true},
              {"completionProvider", nlohmann::json::object()},
              {"definitionProvider", true},
              {"documentSymbolProvider", true}}},
            {"serverInfo", {{"name", "hongik-lsp"}, {"version", "0.1.0"}}}};
}

void Server::onDidOpen(const nlohmann::json& params) {
    const auto& td = params.at("textDocument");
    const auto* doc = store_.open(td.at("uri"), td.at("text"), td.value("version", 0LL));
    publish(*doc);
}

void Server::onDidChange(const nlohmann::json& params) {
    const auto& td = params.at("textDocument");
    const auto& changes = params.at("contentChanges");
    if (changes.empty()) return;
    // Full sync — 마지막 변경분의 text가 전체 문서
    const auto* doc = store_.update(td.at("uri"), changes.back().at("text"), td.value("version", 0LL));
    publish(*doc);
}

void Server::onDidClose(const nlohmann::json& params) {
    const std::string uri = params.at("textDocument").at("uri");
    store_.close(uri);
    publishEmpty(uri);
}

void Server::publish(const AnalyzedDocument& doc) {
    writeMessage(out_, {{"jsonrpc", "2.0"},
                        {"method", "textDocument/publishDiagnostics"},
                        {"params",
                         {{"uri", doc.uri}, {"version", doc.version}, {"diagnostics", features::buildDiagnostics(doc)}}}});
}

void Server::publishEmpty(const std::string& uri) {
    writeMessage(out_, {{"jsonrpc", "2.0"},
                        {"method", "textDocument/publishDiagnostics"},
                        {"params", {{"uri", uri}, {"diagnostics", nlohmann::json::array()}}}});
}

const AnalyzedDocument* Server::docFor(const nlohmann::json& params) const {
    return store_.get(params.at("textDocument").at("uri"));
}

} // namespace lsp
```

CMake: `lsp/server.cpp`, `lsp/features/diagnostics.cpp`, `tests/lsp_server_test.cpp` 추가.

- [ ] **Step 4: 통과 확인** — `--gtest_filter="LspServerTest.*"` PASS + 전체 ctest PASS
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(lsp): server lifecycle, document sync, publishDiagnostics"`

---

### Task 8: hover

**Files:**
- Create: `lsp/features/hover.h`, `lsp/features/hover.cpp`
- Modify: `lsp/server.cpp` (핸들러 구현 + bind 등록), `tests/lsp_server_test.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces (`lsp::features`): `std::shared_ptr<Token> tokenAt(const AnalyzedDocument&, long long line1, long long cpCol)`, `const SymbolInfo* resolveSymbol(const AnalyzedDocument&, const std::string& name, long long line1)`, `nlohmann::json hover(const AnalyzedDocument&, long long lspLine, long long lspChar)`. resolveSymbol 규칙: 같은 이름 중 선언 line ≤ 요청 line인 것 중 가장 큰 line, 없으면 가장 작은 line (spec v1.1 한계 ②). Task 10 definition이 tokenAt/resolveSymbol 재사용.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/lsp_server_test.cpp`에 추가

```cpp
TEST_F(LspServerTest, HoverOnVariableShowsDeclaredType) {
    d.handle(didOpen("file:///h.hik", "정수 개수 = 3\n출력(개수)\n"), out);
    drain(out);
    // 2행 "출력(개수)" — 개수: 출(0)력(1)((2)개(3)수(4) → cp 3
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "textDocument/hover"},
                  {"params", {{"textDocument", {{"uri", "file:///h.hik"}}},
                              {"position", {{"line", 1}, {"character", 3}}}}}},
             out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    const std::string value = msgs[0]["result"]["contents"]["value"];
    EXPECT_NE(value.find("정수 개수"), std::string::npos);
}

TEST_F(LspServerTest, HoverOnBuiltinShowsArity) {
    d.handle(didOpen("file:///h2.hik", "출력(1)\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 3}, {"method", "textDocument/hover"},
                  {"params", {{"textDocument", {{"uri", "file:///h2.hik"}}},
                              {"position", {{"line", 0}, {"character", 0}}}}}},
             out);
    auto msgs = drain(out);
    const std::string value = msgs[0]["result"]["contents"]["value"];
    EXPECT_NE(value.find("내장 함수"), std::string::npos);
    EXPECT_NE(value.find("출력"), std::string::npos);
}

TEST_F(LspServerTest, HoverOnNothingReturnsNull) {
    d.handle(didOpen("file:///h3.hik", "정수 x = 1\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 4}, {"method", "textDocument/hover"},
                  {"params", {{"textDocument", {{"uri", "file:///h3.hik"}}},
                              {"position", {{"line", 0}, {"character", 7}}}}}},  // '=' 위
             out);
    auto msgs = drain(out);
    EXPECT_TRUE(msgs[0]["result"].is_null());
}
```

- [ ] **Step 2: 실패 확인** — hover 미등록이므로 `-32601` 응답 → 테스트 FAIL

- [ ] **Step 3: 구현**

`lsp/features/hover.h`:

```cpp
#ifndef LSP_FEATURES_HOVER_H
#define LSP_FEATURES_HOVER_H

#include "../document_store.h"
#include <nlohmann/json.hpp>

namespace lsp::features {

// 내부 좌표(line 1-based, cp col)의 토큰. 없으면 nullptr
std::shared_ptr<Token> tokenAt(const AnalyzedDocument& doc, long long line1, long long cpCol);
// 이름 → 심볼. 선언 line <= 요청 line 중 최근접, 없으면 최초 선언 (spec v1.1 한계 ②)
const SymbolInfo* resolveSymbol(const AnalyzedDocument& doc, const std::string& name, long long line1);
// LSP 좌표를 받아 Hover result JSON (해당 없음 → null)
nlohmann::json hover(const AnalyzedDocument& doc, long long lspLine, long long lspChar);

} // namespace lsp::features

#endif // LSP_FEATURES_HOVER_H
```

`lsp/features/hover.cpp`:

```cpp
#include "hover.h"

#include "../../object/builtin_signatures.h"
#include "../position.h"
#include "diagnostics.h"
#include <climits>

namespace lsp::features {

std::shared_ptr<Token> tokenAt(const AnalyzedDocument& doc, long long line1, long long cpCol) {
    for (const auto& t : doc.tokens) {
        if (t->line == line1 && t->column <= cpCol && cpCol < t->endColumn) return t;
    }
    return nullptr;
}

const SymbolInfo* resolveSymbol(const AnalyzedDocument& doc, const std::string& name, long long line1) {
    const SymbolInfo* before = nullptr;  // line <= 요청 줄 중 가장 늦은 선언
    const SymbolInfo* first = nullptr;   // 전체 중 가장 이른 선언
    for (const auto& s : doc.symbols) {
        if (s.name != name) continue;
        if (!first || s.line < first->line) first = &s;
        if (s.line <= line1 && (!before || s.line > before->line)) before = &s;
    }
    return before ? before : first;
}

namespace {

std::string kindLabel(DocSymbolKind kind) {
    switch (kind) {
    case DocSymbolKind::Variable: return "변수";
    case DocSymbolKind::Function: return "함수";
    case DocSymbolKind::Class: return "클래스";
    case DocSymbolKind::Field: return "필드";
    case DocSymbolKind::Method: return "메서드";
    case DocSymbolKind::Parameter: return "매개변수";
    }
    return "";
}

std::string symbolMarkdown(const SymbolInfo& s) {
    std::string code;
    if (s.kind == DocSymbolKind::Function || s.kind == DocSymbolKind::Method || s.kind == DocSymbolKind::Class) {
        code = s.typeText;  // 시그니처/클래스 표기 전체
    } else {
        code = s.typeText.empty() ? s.name : s.typeText + " " + s.name;
    }
    std::string md = "```hongik\n" + code + "\n```\n" + kindLabel(s.kind);
    if (!s.container.empty()) md += " — " + s.container;
    return md;
}

const BuiltinSignature* findBuiltin(const std::string& name) {
    for (const auto& sig : kBuiltinSignatures)
        if (name == sig.name) return &sig;
    return nullptr;
}

std::string builtinMarkdown(const BuiltinSignature& sig) {
    std::string arity = sig.maxArity == INT_MAX ? std::to_string(sig.minArity) + "개 이상"
                        : sig.minArity == sig.maxArity
                            ? std::to_string(sig.minArity) + "개"
                            : std::to_string(sig.minArity) + "~" + std::to_string(sig.maxArity) + "개";
    return "내장 함수 `" + std::string(sig.name) + "` — 인자 " + arity;
}

} // namespace

nlohmann::json hover(const AnalyzedDocument& doc, long long lspLine, long long lspChar) {
    if (lspLine < 0 || lspLine >= static_cast<long long>(doc.lines.size())) return nullptr;
    const long long cpCol = utf16ToCp(doc.lines[lspLine], lspChar);
    auto token = tokenAt(doc, lspLine + 1, cpCol);
    if (!token || token->type != TokenType::IDENTIFIER) return nullptr;

    std::string value;
    if (const auto* sym = resolveSymbol(doc, token->text, lspLine + 1)) {
        value = symbolMarkdown(*sym);
    } else if (const auto* builtin = findBuiltin(token->text)) {
        value = builtinMarkdown(*builtin);
    } else {
        return nullptr;
    }
    return {{"contents", {{"kind", "markdown"}, {"value", value}}},
            {"range", toRange(doc, token->line, token->column, token->endColumn)}};
}

} // namespace lsp::features
```

`lsp/server.cpp` — bind에 등록 + 핸들러:

```cpp
    d.onRequest("textDocument/hover", [this](const nlohmann::json& p) { return onHover(p); });
```

```cpp
nlohmann::json Server::onHover(const nlohmann::json& params) {
    const auto* doc = docFor(params);
    if (!doc) return nullptr;
    const auto& pos = params.at("position");
    return features::hover(*doc, pos.at("line"), pos.at("character"));
}
```

server.cpp 상단에 `#include "features/hover.h"` 추가. CMake: `lsp/features/hover.cpp` 추가.

- [ ] **Step 4: 통과 확인** — hover 테스트 3종 PASS + 전체 ctest PASS
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(lsp): hover with declared types and builtin signatures"`

---

### Task 9: completion (+ Lexer::Keywords 접근자)

**Files:**
- Modify: `lexer/lexer.h`, `lexer/lexer.cpp`
- Create: `lsp/features/completion.h`, `lsp/features/completion.cpp`
- Modify: `lsp/server.cpp`, `tests/lsp_server_test.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces: `static const std::unordered_map<std::string, TokenType>& Lexer::Keywords()` (단일 진실의 원천 — completion이 키워드 목록 하드코딩하지 않도록), `lsp::features::completion(const AnalyzedDocument&) -> json array`. v1은 스코프 필터링 없음 (spec v1.1 한계 ①).

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/lsp_server_test.cpp`에 추가

```cpp
TEST_F(LspServerTest, CompletionListsKeywordsBuiltinsAndSymbols) {
    d.handle(didOpen("file:///c.hik", "정수 내변수 = 1\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 5}, {"method", "textDocument/completion"},
                  {"params", {{"textDocument", {{"uri", "file:///c.hik"}}},
                              {"position", {{"line", 0}, {"character", 0}}}}}},
             out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    const auto& items = msgs[0]["result"];
    ASSERT_TRUE(items.is_array());
    auto has = [&](const std::string& label, int kind) {
        for (const auto& item : items)
            if (item["label"] == label && item["kind"] == kind) return true;
        return false;
    };
    EXPECT_TRUE(has("만약", 14));    // Keyword
    EXPECT_TRUE(has("출력", 3));     // 내장 Function
    EXPECT_TRUE(has("내변수", 6));   // Variable
}
```

- [ ] **Step 2: 실패 확인** — `-32601` → FAIL

- [ ] **Step 3: 구현**

`lexer/lexer.h` — public에 추가:

```cpp
    // 키워드 목록의 단일 진실의 원천 (LSP completion 공유)
    static const std::unordered_map<std::string, TokenType>& Keywords();
```

`lexer/lexer.cpp` — 생성자의 keywords 초기화 리스트를 정적 함수로 이동 (**목록 내용은 그대로 이동**, 수정 금지):

```cpp
const std::unordered_map<std::string, TokenType>& Lexer::Keywords() {
    static const std::unordered_map<std::string, TokenType> kKeywords = {
        {"정수", TokenType::정수},
        // ... (기존 생성자의 keywords 초기화 목록 전체를 그대로 이동) ...
        {"false", TokenType::FALSE},
    };
    return kKeywords;
}

Lexer::Lexer() {
    keywords = Keywords();
    singleCharacterTokens = { /* 기존 그대로 */ };
    ...
}
```

`lsp/features/completion.h`:

```cpp
#ifndef LSP_FEATURES_COMPLETION_H
#define LSP_FEATURES_COMPLETION_H

#include "../document_store.h"
#include <nlohmann/json.hpp>

namespace lsp::features {

// v1: 키워드 + 내장 함수 + 문서 전체 심볼 (스코프 필터링은 v2)
nlohmann::json completion(const AnalyzedDocument& doc);

} // namespace lsp::features

#endif // LSP_FEATURES_COMPLETION_H
```

`lsp/features/completion.cpp`:

```cpp
#include "completion.h"

#include "../../lexer/lexer.h"
#include "../../object/builtin_signatures.h"

namespace lsp::features {

namespace {
// LSP CompletionItemKind
constexpr int kKindMethod = 2, kKindFunction = 3, kKindField = 5, kKindVariable = 6, kKindClass = 7, kKindKeyword = 14;

int completionKind(DocSymbolKind kind) {
    switch (kind) {
    case DocSymbolKind::Function: return kKindFunction;
    case DocSymbolKind::Class: return kKindClass;
    case DocSymbolKind::Field: return kKindField;
    case DocSymbolKind::Method: return kKindMethod;
    case DocSymbolKind::Variable:
    case DocSymbolKind::Parameter: return kKindVariable;
    }
    return kKindVariable;
}
} // namespace

nlohmann::json completion(const AnalyzedDocument& doc) {
    nlohmann::json items = nlohmann::json::array();
    for (const auto& [keyword, type] : Lexer::Keywords()) {
        items.push_back({{"label", keyword}, {"kind", kKindKeyword}});
    }
    for (const auto& sig : kBuiltinSignatures) {
        items.push_back({{"label", sig.name}, {"kind", kKindFunction}, {"detail", "내장 함수"}});
    }
    for (const auto& s : doc.symbols) {
        items.push_back({{"label", s.name}, {"kind", completionKind(s.kind)}, {"detail", s.typeText}});
    }
    return items;
}

} // namespace lsp::features
```

`lsp/server.cpp` — 등록 + 핸들러 (+`#include "features/completion.h"`):

```cpp
    d.onRequest("textDocument/completion", [this](const nlohmann::json& p) { return onCompletion(p); });
```

```cpp
nlohmann::json Server::onCompletion(const nlohmann::json& params) {
    const auto* doc = docFor(params);
    if (!doc) return nlohmann::json::array();
    return features::completion(*doc);
}
```

CMake: `lsp/features/completion.cpp` 추가.

- [ ] **Step 4: 통과 확인** — completion 테스트 PASS + 전체 ctest PASS (렉서 회귀 주의 — LexerTest 전체 확인)
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(lsp,lexer): completion with keywords/builtins/symbols, Lexer::Keywords accessor"`

---

### Task 10: definition + documentSymbol

**Files:**
- Create: `lsp/features/definition.h`, `lsp/features/definition.cpp`, `lsp/features/symbols.h`, `lsp/features/symbols.cpp`
- Modify: `lsp/server.cpp`, `tests/lsp_server_test.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 8 `tokenAt`/`resolveSymbol`, Task 7 `toRange`.
- Produces: `lsp::features::definition(doc, lspLine, lspChar) -> json (Location | null)`, `lsp::features::documentSymbols(doc) -> json array (DocumentSymbol[], 클래스는 필드/메서드 children)`.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/lsp_server_test.cpp`에 추가

```cpp
TEST_F(LspServerTest, DefinitionJumpsToDeclaration) {
    d.handle(didOpen("file:///g.hik", "정수 개수 = 3\n출력(개수)\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 6}, {"method", "textDocument/definition"},
                  {"params", {{"textDocument", {{"uri", "file:///g.hik"}}},
                              {"position", {{"line", 1}, {"character", 3}}}}}},  // 2행의 '개수'
             out);
    auto msgs = drain(out);
    const auto& loc = msgs[0]["result"];
    ASSERT_FALSE(loc.is_null());
    EXPECT_EQ(loc["uri"], "file:///g.hik");
    EXPECT_EQ(loc["range"]["start"]["line"], 0);       // 1행 선언부
    EXPECT_EQ(loc["range"]["start"]["character"], 3);  // 정수␣ 다음
}

TEST_F(LspServerTest, DocumentSymbolBuildsClassTree) {
    d.handle(didOpen("file:///s.hik",
                     "정수 전역 = 1\n"
                     "함수 도우미() -> 정수:\n"
                     "    리턴 1\n"
                     "클래스 점:\n"
                     "    정수 x\n"
                     "    생성(정수 값):\n"
                     "        자기.x = 값\n"
                     "    함수 값얻기() -> 정수:\n"
                     "        리턴 자기.x\n"),
             out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 7}, {"method", "textDocument/documentSymbol"},
                  {"params", {{"textDocument", {{"uri", "file:///s.hik"}}}}}},
             out);
    auto msgs = drain(out);
    const auto& syms = msgs[0]["result"];
    ASSERT_TRUE(syms.is_array());
    // 톱레벨: 전역(Variable 13), 도우미(Function 12), 점(Class 5) — 지역/파라미터 제외
    ASSERT_EQ(syms.size(), 3u);
    json cls;
    for (const auto& s : syms)
        if (s["name"] == "점") cls = s;
    ASSERT_FALSE(cls.is_null());
    EXPECT_EQ(cls["kind"], 5);
    // children: x(Field 8), 값얻기(Method 6) — 생성자 파라미터 제외
    ASSERT_EQ(cls["children"].size(), 2u);
}
```

- [ ] **Step 2: 실패 확인** — `-32601` → FAIL

- [ ] **Step 3: 구현**

`lsp/features/definition.h`:

```cpp
#ifndef LSP_FEATURES_DEFINITION_H
#define LSP_FEATURES_DEFINITION_H

#include "../document_store.h"
#include <nlohmann/json.hpp>

namespace lsp::features {
nlohmann::json definition(const AnalyzedDocument& doc, long long lspLine, long long lspChar);
} // namespace lsp::features

#endif // LSP_FEATURES_DEFINITION_H
```

`lsp/features/definition.cpp`:

```cpp
#include "definition.h"

#include "../position.h"
#include "diagnostics.h"
#include "hover.h"

namespace lsp::features {

nlohmann::json definition(const AnalyzedDocument& doc, long long lspLine, long long lspChar) {
    if (lspLine < 0 || lspLine >= static_cast<long long>(doc.lines.size())) return nullptr;
    const long long cpCol = utf16ToCp(doc.lines[lspLine], lspChar);
    auto token = tokenAt(doc, lspLine + 1, cpCol);
    if (!token || token->type != TokenType::IDENTIFIER) return nullptr;
    const auto* sym = resolveSymbol(doc, token->text, lspLine + 1);
    if (!sym) return nullptr;
    return {{"uri", doc.uri}, {"range", toRange(doc, sym->line, sym->column, sym->endColumn)}};
}

} // namespace lsp::features
```

`lsp/features/symbols.h`:

```cpp
#ifndef LSP_FEATURES_SYMBOLS_H
#define LSP_FEATURES_SYMBOLS_H

#include "../document_store.h"
#include <nlohmann/json.hpp>

namespace lsp::features {
// DocumentSymbol[] — 톱레벨(container=="")의 변수/함수/클래스, 클래스는 필드/메서드 children
nlohmann::json documentSymbols(const AnalyzedDocument& doc);
} // namespace lsp::features

#endif // LSP_FEATURES_SYMBOLS_H
```

`lsp/features/symbols.cpp`:

```cpp
#include "symbols.h"

#include "diagnostics.h"

namespace lsp::features {

namespace {
// LSP SymbolKind (CompletionItemKind와 다른 enum임에 주의)
constexpr int kSymClass = 5, kSymMethod = 6, kSymField = 8, kSymFunction = 12, kSymVariable = 13;

int lspSymbolKind(DocSymbolKind kind) {
    switch (kind) {
    case DocSymbolKind::Class: return kSymClass;
    case DocSymbolKind::Method: return kSymMethod;
    case DocSymbolKind::Field: return kSymField;
    case DocSymbolKind::Function: return kSymFunction;
    default: return kSymVariable;
    }
}

nlohmann::json toDocumentSymbol(const AnalyzedDocument& doc, const SymbolInfo& s) {
    auto range = toRange(doc, s.line, s.column, s.endColumn);
    return {{"name", s.name},        {"kind", lspSymbolKind(s.kind)},
            {"detail", s.typeText},  {"range", range},
            {"selectionRange", range}, {"children", nlohmann::json::array()}};
}
} // namespace

nlohmann::json documentSymbols(const AnalyzedDocument& doc) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& s : doc.symbols) {
        if (!s.container.empty()) continue;  // 톱레벨만 (멤버는 children으로)
        auto node = toDocumentSymbol(doc, s);
        if (s.kind == DocSymbolKind::Class) {
            for (const auto& member : doc.symbols) {
                if (member.container != s.name) continue;
                if (member.kind != DocSymbolKind::Field && member.kind != DocSymbolKind::Method) continue;
                node["children"].push_back(toDocumentSymbol(doc, member));
            }
        }
        result.push_back(std::move(node));
    }
    return result;
}

} // namespace lsp::features
```

`lsp/server.cpp` — 등록 + 핸들러 (+include 2개):

```cpp
    d.onRequest("textDocument/definition", [this](const nlohmann::json& p) { return onDefinition(p); });
    d.onRequest("textDocument/documentSymbol", [this](const nlohmann::json& p) { return onDocumentSymbol(p); });
```

```cpp
nlohmann::json Server::onDefinition(const nlohmann::json& params) {
    const auto* doc = docFor(params);
    if (!doc) return nullptr;
    const auto& pos = params.at("position");
    return features::definition(*doc, pos.at("line"), pos.at("character"));
}

nlohmann::json Server::onDocumentSymbol(const nlohmann::json& params) {
    const auto* doc = docFor(params);
    if (!doc) return nlohmann::json::array();
    return features::documentSymbols(*doc);
}
```

CMake: `lsp/features/definition.cpp`, `lsp/features/symbols.cpp` 추가.

- [ ] **Step 4: 통과 확인** — 신규 테스트 PASS + 전체 ctest PASS
- [ ] **Step 5: 커밋** — `git add -A && git commit -m "feat(lsp): go-to-definition and documentSymbol outline"`

---

### Task 11: hongik-lsp 실행 파일

**Files:**
- Create: `lsp/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `hongik-lsp` 실행 파일 (stdio LSP 서버). Task 12 클라이언트가 실행.

- [ ] **Step 1: 구현** (엔트리포인트 — 로직은 전부 기존 테스트로 커버된 모듈, main은 조립만)

`lsp/main.cpp`:

```cpp
#include "jsonrpc.h"
#include "server.h"

#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main() {
#ifdef _WIN32
    // CRLF 변환이 Content-Length 바이트 계산을 깨뜨린다 — 바이너리 모드 필수 (spec 리스크 4)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    lsp::Dispatcher dispatcher;
    lsp::Server server(std::cout);
    server.bind(dispatcher);

    while (!server.shouldExit()) {
        auto body = lsp::readMessage(std::cin);
        if (!body) break;  // EOF — 클라이언트 종료
        auto msg = nlohmann::json::parse(*body, nullptr, /*allow_exceptions=*/false);
        if (msg.is_discarded()) continue;  // 손상 프레임 — 다음 메시지 계속
        dispatcher.handle(msg, std::cout);
    }
    return 0;
}
```

CMake (HongIkLspLib 정의 아래):

```cmake
    add_executable(hongik-lsp lsp/main.cpp)
    target_link_libraries(hongik-lsp HongIkLspLib)
    hongik_apply_sanitizers(hongik-lsp)
```

- [ ] **Step 2: 빌드 + 스모크 테스트** (Git Bash에서)

```bash
cmake --build cmake-build-debug --target hongik-lsp
BODY='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
printf "Content-Length: ${#BODY}\r\n\r\n${BODY}" | ./cmake-build-debug/hongik-lsp
```

Expected: `Content-Length: ...` 헤더 + `"capabilities"` 포함 JSON 응답 출력 후 EOF로 정상 종료 (exit 0).

- [ ] **Step 3: 전체 회귀 확인** — `ctest --test-dir cmake-build-debug` 전체 PASS
- [ ] **Step 4: 커밋** — `git add -A && git commit -m "feat(lsp): hongik-lsp executable entry point"`

---

### Task 12: VSCode 클라이언트 + 문서

**Files:**
- Modify: `vscode-extension/package.json`
- Create: `vscode-extension/src/extension.ts`, `vscode-extension/tsconfig.json`, `vscode-extension/.gitignore`, `vscode-extension/DEVELOPMENT.md`
- Modify: `CHANGELOG.md`, `README.md` (LSP 섹션 추가)

**Interfaces:**
- Consumes: Task 11 `hongik-lsp` 바이너리.
- Produces: `hongik.lsp.path` 설정으로 서버를 찾는 VSCode 확장 v0.3.0.

- [ ] **Step 1: package.json 갱신** — 기존 contributes(languages/grammars/snippets)는 유지하고 다음을 추가/변경:

```json
{
    "version": "0.3.0",
    "main": "./out/extension.js",
    "activationEvents": ["onLanguage:hongik"],
    "contributes": {
        "configuration": {
            "title": "Hong-Ik",
            "properties": {
                "hongik.lsp.path": {
                    "type": "string",
                    "default": "",
                    "description": "hongik-lsp 실행 파일 경로. 비어 있으면 PATH에서 'hongik-lsp'를 찾습니다."
                }
            }
        }
    },
    "scripts": {
        "compile": "tsc -p .",
        "watch": "tsc -w -p ."
    },
    "dependencies": {
        "vscode-languageclient": "^9.0.1"
    },
    "devDependencies": {
        "@types/node": "^20.11.0",
        "@types/vscode": "^1.75.0",
        "typescript": "^5.4.0"
    }
}
```

- [ ] **Step 2: extension.ts + tsconfig 작성**

`vscode-extension/tsconfig.json`:

```json
{
    "compilerOptions": {
        "module": "commonjs",
        "target": "ES2020",
        "outDir": "out",
        "rootDir": "src",
        "strict": true,
        "sourceMap": true,
        "skipLibCheck": true
    },
    "include": ["src"]
}
```

`vscode-extension/src/extension.ts`:

```typescript
import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions, ServerOptions } from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(_context: vscode.ExtensionContext): void {
    const configured = vscode.workspace.getConfiguration('hongik').get<string>('lsp.path');
    const command = configured && configured.length > 0 ? configured : 'hongik-lsp';

    const serverOptions: ServerOptions = { command, args: [] };
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ language: 'hongik' }],
    };

    client = new LanguageClient('hongik-lsp', 'Hong-Ik Language Server', serverOptions, clientOptions);
    client.start().catch((err: unknown) => {
        void vscode.window.showWarningMessage(
            `hongik-lsp 시작 실패: ${String(err)} — 'hongik.lsp.path' 설정을 확인하세요. (하이라이팅은 계속 동작합니다)`
        );
    });
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}
```

`vscode-extension/.gitignore`:

```
node_modules/
out/
```

- [ ] **Step 3: 컴파일 확인**

Run: `cd vscode-extension && npm install && npm run compile`
Expected: `out/extension.js` 생성, tsc 에러 0

- [ ] **Step 4: E2E 수동 검증 시나리오 작성** — `vscode-extension/DEVELOPMENT.md`

```markdown
# 개발·검증 가이드

## 빌드
1. 서버: 저장소 루트에서 `cmake --build cmake-build-debug --target hongik-lsp`
2. 클라이언트: `cd vscode-extension && npm install && npm run compile`

## Extension Host로 검증 (F5)
1. VSCode에서 `vscode-extension/` 폴더를 열고 F5 (Run Extension)
2. 설정에서 `hongik.lsp.path`를 `<repo>/cmake-build-debug/hongik-lsp(.exe)` 절대 경로로 지정
3. `.hik` 파일을 열고 아래 체크리스트 확인

## 체크리스트 (v1 기능 5종)
- [ ] 진단: `정수 x = ;` 입력 → 빨간 밑줄 + 한글 에러 메시지, 고치면 사라짐
- [ ] 진단(타입): `정수 x = 모름` → 타입 체커 경고/에러 표시
- [ ] 호버: 선언된 변수 위에 마우스 → `정수 x` 타입 표시, `출력` 위 → 내장 함수 시그니처
- [ ] 자동완성: Ctrl+Space → 키워드(만약/함수/클래스…), 내장 함수 45종, 문서 내 심볼
- [ ] 정의로 이동: 변수 사용처에서 F12 → 선언 줄로 점프
- [ ] 아웃라인: 탐색기 아웃라인에 함수/클래스(필드·메서드 children)/전역 변수 표시
- [ ] 한글 위치 정확성: `정수 한글이름 = 1` 에서 호버/밑줄 범위가 글자 단위로 정확한지

## 알려진 v1 한계 (spec v1.1)
- 자동완성 스코프 필터링 없음 (문서 전체 심볼 노출)
- 렉서 에러(알 수 없는 문자)는 해당 파일 진단이 1건만 표시됨
- 멀티 파일(`가져오기`) 심볼은 미지원
```

- [ ] **Step 5: E2E 수동 검증 실행** — DEVELOPMENT.md 체크리스트를 실제 VSCode에서 수행. **사용자(성민) 확인 필요 항목** — 에이전트는 여기까지 준비하고 사용자에게 검증 요청.

- [ ] **Step 6: 문서 갱신 + 커밋** — `CHANGELOG.md`에 v1 LSP 항목(Added), `README.md`에 "에디터 지원(LSP)" 섹션(빌드 방법 + hongik.lsp.path 설정) 추가.

```bash
git add -A && git commit -m "feat(vscode): LSP client wired to hongik-lsp (extension v0.3.0)"
```

---

## 태스크 의존성

```
Task 1 (진단 구조화) ─┬→ Task 6 (DocumentStore) → Task 7 (Server+진단) → Task 8 (hover) → Task 10 (definition/symbols)
Task 2 (nameToken)  ─→ Task 4 (SymbolCollector) ↗                        Task 9 (completion) ↗
Task 3 (CMake+position) ─→ Task 5 (jsonrpc) → Task 7                     Task 11 (main) → Task 12 (VSCode)
```

Task 1·2·3은 상호 독립(병렬 가능). Task 8·9는 Task 7 이후 상호 독립. Task 11은 Task 7~10 완료 후.
