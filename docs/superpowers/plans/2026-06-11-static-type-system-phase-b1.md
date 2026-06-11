# 정적 타입 시스템 Phase B-1 (표현식 정밀화) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** TypeChecker에 정확한 줄 번호, 이항 연산자 검사(TC601)·결과 추론, forrange 범위 검사(TC301), Optional 분기 좁히기를 추가한다.

**Architecture:** spec `2026-06-11-static-type-system-phase-b-design.md` D1~D4. 모든 진단 규칙은 spec 부록 A(실측 2026-06-11)에 박제된 both-reject 조합만 따른다. evaluator/vm/environment/object(시그니처 테이블 제외)는 변경 금지, ast/parser는 D1의 additive line 필드만 허용.

**Tech Stack:** C++17/26, CMake 3.28+(풀패스: `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`), Google Test, MSVC. 작업 경로: `E:\dev\projects\school\hongik\hong-ik`. 베이스라인 테스트 434개.

**공통 명령** (아래에서 `$CMAKE`/`$CTEST`로 표기):
```powershell
& "C:\...\cmake.exe" --build hong-ik/cmake-build-debug --target HongIkTests   # 빌드
& "C:\...\ctest.exe" --test-dir hong-ik/cmake-build-debug -R TypeCheckerTest  # 부분 실행
& "C:\...\ctest.exe" --test-dir hong-ik/cmake-build-debug                     # 전체 회귀
```

---

## Task 0: 사전 실측 — ✅ 완료 (2026-06-11, plan 작성 시 선행)

- [x] 이항 연산자 16종 × 7×7 타입 전수 실측 (양 백엔드) → spec 부록 A.1/A.2 박제
- [x] if-블록 스코프 실측 → 정반대 시맨틱 발견(부록 B #2), narrowing은 오버레이 방식 확정
- [x] forrange 경계 타입 실측 → TC301 = {문자, 논리, 없음, 배열, 사전}, 실수는 불일치(#4)라 제외
- [x] foreach 실측 → TC302 폐기 (VM 침묵 통과, 부록 B #5)
- [x] 골든 픽스처에 `실수 <- 정수연산` 패턴 없음 확인 (결과 추론 도입 시 회귀 없음)

### Task 1: AST 줄 번호 (D1)

**Files:**
- Modify: `ast/node.h` (line 필드)
- Modify: `parser/parser.cpp` (48개 make_shared 지점 — 아래 목록)
- Modify: `analyzer/type_checker.cpp` (nodeLine 우선순위)
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/type_checker_test.cpp` 끝의 `ScopePopAfterForEach` 앞에 추가:

```cpp
// ---- plan B-1 Task 1: AST 줄 번호 (spec D1) ----

TEST(TypeCheckerTest, DiagnosticLineNumberIdentifier) {
    TypeChecker tc;
    auto result = checkSource(tc, "출력(1)\n출력(미선언변수)\n");
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "TC006");
    EXPECT_EQ(result.diagnostics[0].line, 2);  // Phase A에서는 1 (직전 리터럴 줄)
}

TEST(TypeCheckerTest, DiagnosticLineNumberAssignment) {
    TypeChecker tc;
    auto result = checkSource(tc, "정수 x = 1\n출력(x)\nx = \"a\"\n");
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "TC002");
    EXPECT_EQ(result.diagnostics[0].line, 3);
}
```

- [ ] **Step 2: 실패 확인** — 빌드 후 `-R TypeCheckerTest` 실행. 기대: 위 2건 FAIL (line이 1 또는 2로 나옴).

- [ ] **Step 3: `ast/node.h`에 line 필드 추가**

```cpp
class Node {
public:
    virtual ~Node() = default;
    long long line = 0;  // 노드 대표(첫) 토큰의 줄 번호. 파서가 채우며 0이면 미상.
    // String 함수는 AST가 잘 생성되었는 지 디버깅 용도
    virtual std::string String() = 0;
};
```

- [ ] **Step 4: parser.cpp 48개 생성 지점에 line 스탬프** — 패턴: 노드를 생성하는 파싱 함수 진입 시점(또는 노드의 앵커 토큰이 확정된 시점)의 토큰 line을 생성 직후 대입. 예시 3곳 (나머지 45곳 동일 패턴):

```cpp
// parser.cpp:236 부근 (InitializationStatement)
auto statement = make_shared<InitializationStatement>();
statement->line = type_token ? type_token->line : 0;   // 타입 토큰이 앵커

// parser.cpp:632 부근 (IdentifierExpression)
auto identifier = make_shared<IdentifierExpression>();
identifier->line = current_token ? current_token->line : 0;

// parser.cpp:784 부근 (CallExpression)
auto call = make_shared<CallExpression>();
call->line = function ? function->line : (current_token ? current_token->line : 0);
```

대상 지점 (grep `make_shared<` 결과, 줄 번호는 변경 전 기준): 37 Program(스탬프 불필요·0 유지 가능), 137·258 AssignmentStatement, 206 Break, 211 Continue, 236 Initialization, 273 IndexAssignment, 276·460·632·735·741·747·935 Identifier, 297 CompoundAssignment, 313 ExpressionStatement, 323 Return, 331 Block, 347 If, 369 While, 384 ForEach, 413 TryCatch, 434 Function, 505 Match, 529 TypePattern, 541 RangePattern, 585 Class, 682 Infix, 699 Prefix, 715 Tuple, 761 MethodCall, 776 MemberAccess, 784 Call, 804·821 Slice, 834 Index, 844 Integer, 855 Float, 866 Boolean, 873 String, 880 Array, 896 HashMap, 916 Null, 926 Lambda, 958 ForRange, 989 Postfix, 997 Import, 1008 Yield.

앵커 선택 규칙: 토큰을 이미 보관하는 노드(리터럴·Infix 등)는 그 토큰, 문장류는 문장 시작 토큰, 후위 구조(Call/Index/Member)는 좌측 피연산자의 `line` 승계.

- [ ] **Step 5: `analyzer/type_checker.cpp`의 `nodeLine()`에 우선순위 추가**

```cpp
long long nodeLine(Node* node) {
    if (node && node->line > 0) return node->line;  // D1: 파서 스탬프 우선
    // (기존 dynamic_cast 체인은 폴백으로 유지)
    if (auto* e = dynamic_cast<InfixExpression*>(node)) return e->token ? e->token->line : 0;
    ...
}
```

- [ ] **Step 6: 빌드 + `-R TypeCheckerTest` PASS 확인, 전체 ctest 회귀 0건 확인** (parser 변경이 lexer/parser/evaluator/vm/golden 테스트에 영향 없어야 함)

- [ ] **Step 7: Commit** — `feat(ast,parser): stamp line numbers on AST nodes (Phase B-1 Task 1)`

### Task 2: 이항 연산자 결과 타입 추론 (D2 — 진단 없이)

**Files:**
- Modify: `analyzer/type_checker.h` (inferInfixExpression 선언)
- Modify: `analyzer/type_checker.cpp` (InfixExpression 분기 교체)
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성**

```cpp
// ---- plan B-1 Task 2: 이항 연산자 결과 추론 (spec 부록 A.2) ----

TEST(TypeCheckerTest, BinaryResultIntInt) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "정수 x = 1 + 2\n").diagnostics.empty());
}

TEST(TypeCheckerTest, BinaryResultIntIntRejectedAsString) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "문자 s = 1 + 2\n"), "TC001");  // 결과 정수
}

TEST(TypeCheckerTest, BinaryResultMixedNumericIsFloat) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "정수 x = 1 + 1.5\n"), "TC001");  // 결과 실수
}

TEST(TypeCheckerTest, BinaryResultFloatOk) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "실수 x = 1 + 1.5\n").diagnostics.empty());
}

TEST(TypeCheckerTest, BinaryResultStringConcat) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "문자 s = \"a\" + \"b\"\n").diagnostics.empty());
}

TEST(TypeCheckerTest, BinaryResultComparisonIsBool) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "논리 b = 1 < 2\n").diagnostics.empty());
}

TEST(TypeCheckerTest, BinaryResultLogicalAndIsBool) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "논리 b = true && false\n").diagnostics.empty());
}
```

- [ ] **Step 2: 실패 확인** — `문자 s = 1 + 2`/`정수 x = 1 + 1.5`가 0건으로 나와 FAIL (현재 Infix 결과 Any).

- [ ] **Step 3: 구현** — `type_checker.h` private에 `std::shared_ptr<Type> inferInfixExpression(InfixExpression& infix);` 추가. `type_checker.cpp`의 기존 InfixExpression 분기를 호출로 교체하고 다음 구현 추가:

```cpp
namespace {
bool isPrimKind(const Type& t, ObjectType k) {
    auto* p = dynamic_cast<const PrimType*>(&t);
    return p && p->kind == k;
}
bool isNumeric(const Type& t) {
    return isPrimKind(t, ObjectType::INTEGER) || isPrimKind(t, ObjectType::FLOAT);
}
} // namespace

// spec 부록 A.2 — 실측(2026-06-11) 기반 결과 타입. 진단(TC601)은 Task 3에서.
std::shared_ptr<Type> TypeChecker::inferInfixExpression(InfixExpression& infix) {
    auto left = inferExpression(infix.left);
    auto right = inferExpression(infix.right);
    if (dynamic_cast<NeverType*>(left.get()) || dynamic_cast<NeverType*>(right.get())) {
        return makeNever();  // cascade (spec 1.1.2)
    }
    TokenType op = infix.token ? infix.token->type : TokenType::ILLEGAL;

    // ==/!=: 항상 논리, TC501 면제 (Phase A 유지)
    if (op == TokenType::EQUAL || op == TokenType::NOT_EQUAL) {
        return makePrim(ObjectType::BOOLEAN);
    }
    // Optional 피연산자: TC501 우선, 결과 Any (Phase A 유지)
    if (auto* opt = dynamic_cast<OptionalType*>(left.get())) {
        warnUnresolvedOptional(*opt);
        return makeAny();
    }
    if (auto* opt = dynamic_cast<OptionalType*>(right.get())) {
        warnUnresolvedOptional(*opt);
        return makeAny();
    }

    const bool numPair = isNumeric(*left) && isNumeric(*right);
    const bool intPair = isPrimKind(*left, ObjectType::INTEGER) && isPrimKind(*right, ObjectType::INTEGER);
    const bool strPair = isPrimKind(*left, ObjectType::STRING) && isPrimKind(*right, ObjectType::STRING);
    const bool boolPair = isPrimKind(*left, ObjectType::BOOLEAN) && isPrimKind(*right, ObjectType::BOOLEAN);

    switch (op) {
    case TokenType::PLUS:
        if (intPair) return makePrim(ObjectType::INTEGER);
        if (numPair) return makePrim(ObjectType::FLOAT);
        if (strPair) return makePrim(ObjectType::STRING);
        return makeAny();
    case TokenType::MINUS:
    case TokenType::ASTERISK:
    case TokenType::SLASH:
    case TokenType::POWER:
        if (intPair) return makePrim(ObjectType::INTEGER);
        if (numPair) return makePrim(ObjectType::FLOAT);
        return makeAny();
    case TokenType::PERCENT:
    case TokenType::BITWISE_AND:
    case TokenType::BITWISE_OR:
        if (intPair) return makePrim(ObjectType::INTEGER);
        return makeAny();
    case TokenType::LESS_THAN:
    case TokenType::GREATER_THAN:
    case TokenType::LESS_EQUAL:
    case TokenType::GREATER_EQUAL:
        if (numPair) return makePrim(ObjectType::BOOLEAN);
        return makeAny();  // 문자쌍 등 — 불일치 면제 (부록 A.1)
    case TokenType::LOGICAL_AND:
    case TokenType::LOGICAL_OR:
        if (boolPair) return makePrim(ObjectType::BOOLEAN);
        return makeAny();  // 혼합은 VM이 우항 값 반환 (부록 B #4)
    default:
        return makeAny();
    }
}
```

- [ ] **Step 4: 빌드 + TypeCheckerTest PASS + 전체 ctest 회귀 확인** (골든 픽스처에 `실수 <- 정수연산` 패턴 없음은 Task 0에서 확인됨)
- [ ] **Step 5: Commit** — `feat(types): precise binary operator result inference (Phase B-1 Task 2)`

### Task 3: TC601 — 이항 연산자 피연산자 비호환 (D2)

**Files:**
- Modify: `analyzer/type_checker.h` (`void warnBinaryIncompatible(const std::string& opText, const Type& left, const Type& right);` private 선언 — `warnUnresolvedOptional` 옆)
- Modify: `analyzer/type_checker.cpp` (inferInfixExpression에 진단 추가)
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성**

```cpp
// ---- plan B-1 Task 3: TC601 (spec 부록 A.1) ----

TEST(TypeCheckerTest, TC601_IntPlusString) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "1 + \"a\"\n"), "TC601");
}

TEST(TypeCheckerTest, TC601_StringMinusString) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "\"a\" - \"b\"\n"), "TC601");
}

TEST(TypeCheckerTest, TC601_EqualityIntString) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "출력(1 == \"a\")\n"), "TC601");
}

TEST(TypeCheckerTest, TC601_EqualityNullAlwaysOk) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "출력([1] == 없음)\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC601_LogicalLeftNonBool) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "출력(1 && true)\n"), "TC601");
}

TEST(TypeCheckerTest, TC601_LogicalRightUnchecked) {
    TypeChecker tc;
    // 단락 평가로 우항은 값 의존 (부록 A.1) — 진단 안 함
    EXPECT_TRUE(checkSource(tc, "출력(true && 1)\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC601_StringComparisonExempt) {
    TypeChecker tc;
    // 문자쌍 비교는 런타임 불일치 (부록 B #3) — 진단 면제
    EXPECT_TRUE(checkSource(tc, "출력(\"a\" < \"b\")\n").diagnostics.empty());
}

TEST(TypeCheckerTest, TC601_BoolComparisonRejected) {
    TypeChecker tc;
    expectSingleDiagnostic(checkSource(tc, "출력(true < false)\n"), "TC601");
}
```

- [ ] **Step 2: 실패 확인** — TC601 케이스들이 0건으로 FAIL.
- [ ] **Step 3: 구현** — `inferInfixExpression`에 진단 추가. **both-PrimType일 때만 발화** (Function/Class/Builtin 피연산자는 실측 범위 밖 → 면제):

```cpp
// (멤버 함수 앞부분에 헬퍼)
void TypeChecker::warnBinaryIncompatible(const std::string& opText,
                                         const Type& left, const Type& right) {
    warn(currentLine_, "TC601",
         "연산자 '" + opText + "'를 '" + left.toKorean() + "'과(와) '"
             + right.toKorean() + "' 타입에 적용할 수 없습니다.");
}
```

```cpp
// inferInfixExpression 내 변경점 (해당 분기에 진단 삽입):
const bool bothPrim = dynamic_cast<PrimType*>(left.get()) && dynamic_cast<PrimType*>(right.get());
const std::string opText = infix.token ? infix.token->text : "?";

// ==/!= 분기:
if (op == TokenType::EQUAL || op == TokenType::NOT_EQUAL) {
    if (bothPrim) {
        bool nullSide = isPrimKind(*left, ObjectType::NULL_OBJ) || isPrimKind(*right, ObjectType::NULL_OBJ);
        bool numPairEq = isNumeric(*left) && isNumeric(*right);
        bool samePrim = strPair || boolPair;  // 문자쌍/논리쌍
        if (!nullSide && !numPairEq && !samePrim) warnBinaryIncompatible(opText, *left, *right);
    }
    return makePrim(ObjectType::BOOLEAN);
}
// 주의: strPair/boolPair 계산을 ==/!= 분기보다 위로 이동해야 함.

// 산술/비트/비교/논리의 기존 `return makeAny();` 폴백을 각각:
//   +            : if (bothPrim) warnBinaryIncompatible(...); return makeAny();
//   - * / **     : 동일
//   % & |        : 동일
//   < > <= >=    : if (bothPrim && !strPair) warnBinaryIncompatible(...); return makeAny();
//   && ||        : 좌항 기준 — if (!isPrimKind(*left, ObjectType::BOOLEAN) && dynamic_cast<PrimType*>(left.get()))
//                    warnBinaryIncompatible(...); 우항 무검사. return makeAny();
```

(논리 연산자 분기는 boolPair 결과 반환 전에 좌항 검사를 수행: 좌항이 PrimType이면서 논리가 아니면 TC601, 결과는 boolPair → 논리 / 그 외 Any.)

- [ ] **Step 4: 빌드 + TypeCheckerTest PASS + 전체 ctest + 골든 0건 재확인**

```powershell
Get-ChildItem hong-ik\tests\fixtures\golden\*.hik | ForEach-Object { $err = & hong-ik\cmake-build-debug\HongIk.exe --type-check=warn $_.FullName 2>&1 | Where-Object { "$_" -match "type-warning" }; "$($_.Name): $(if ($err) { $err -join '; ' } else { 'clean' })" }
```
기대: error_undefined.hik의 TC006 1건 외 전부 clean.

- [ ] **Step 5: Commit** — `feat(types): TC601 binary operator incompatibility checks (Phase B-1 Task 3)`

### Task 4: TC301 — forrange 범위 타입 (D4)

**Files:**
- Modify: `analyzer/type_checker.cpp` (ForRangeStatement 분기)
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성**

```cpp
// ---- plan B-1 Task 4: TC301 (spec D4) ----

TEST(TypeCheckerTest, TC301_StringBound) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "반복 정수 i = \"a\" 부터 3 까지:\n"
        "    출력(i)\n");
    expectSingleDiagnostic(result, "TC301");
}

TEST(TypeCheckerTest, TC301_FloatBoundExempt) {
    TypeChecker tc;
    // 실수 경계는 런타임 불일치 (부록 B #4) — 진단 면제
    auto result = checkSource(tc,
        "반복 정수 i = 0.5 부터 3 까지:\n"
        "    출력(i)\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC301_IntBoundsOk) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "반복 정수 i = 0 부터 10 까지:\n"
        "    출력(i)\n");
    EXPECT_TRUE(result.diagnostics.empty());
}
```

- [ ] **Step 2: 실패 확인** — TC301_StringBound가 0건으로 FAIL.
- [ ] **Step 3: 구현** — ForRangeStatement 분기에서 start/end infer 결과 검사:

```cpp
if (auto* forRange = dynamic_cast<ForRangeStatement*>(stmt.get())) {
    pushScope();
    declare(forRange->varName, typeFromToken(forRange->varType, false));
    auto checkBound = [this](const std::shared_ptr<Type>& t) {
        // spec D4: 두 런타임 모두 거부하는 타입만 (실수는 불일치 — 면제)
        if (isPrimKind(*t, ObjectType::STRING) || isPrimKind(*t, ObjectType::BOOLEAN)
            || isPrimKind(*t, ObjectType::NULL_OBJ) || isPrimKind(*t, ObjectType::ARRAY)
            || isPrimKind(*t, ObjectType::HASH_MAP)) {
            warn(currentLine_, "TC301",
                 "반복 범위는 정수여야 합니다. '" + t->toKorean() + "' 타입은 사용할 수 없습니다.");
        }
    };
    checkBound(inferExpression(forRange->startExpr));
    checkBound(inferExpression(forRange->endExpr));
    checkStatement(forRange->body);
    popScope();
    return;
}
```
(`isPrimKind`는 Task 2에서 익명 namespace에 추가됨 — 람다에서 쓰려면 파일 스코프 자유 함수라 그대로 호출 가능.)

- [ ] **Step 4: 빌드 + 전체 ctest + 골든 clean 확인**
- [ ] **Step 5: Commit** — `feat(types): TC301 forrange bound type checks (Phase B-1 Task 4)`

### Task 5: Optional 분기 좁히기 (D3 — 오버레이 방식)

**Files:**
- Modify: `analyzer/type_checker.h` (오버레이 스택 + collectNarrowings)
- Modify: `analyzer/type_checker.cpp` (IfStatement/lookup/declare/Assignment)
- Test: `tests/type_checker_test.cpp` (신규 + Phase A 테스트 1건 의도 반전)

- [ ] **Step 1: 실패하는 테스트 작성** — 기존 `TC501_NoNarrowingInPhaseA`를 다음으로 교체(의도 반전 — Phase A 한계 문서화 테스트였음):

```cpp
TEST(TypeCheckerTest, NarrowingInThenBranch) {
    TypeChecker tc;
    // Phase B-1: x != 없음 분기 안에서 좁힘 (spec D3)
    auto result = checkSource(tc,
        "정수? x = 10\n"
        "만약 x != 없음 라면:\n"
        "    정수 y = x + 1\n");
    EXPECT_TRUE(result.diagnostics.empty());
}
```

신규 추가:

```cpp
TEST(TypeCheckerTest, NarrowingEqNullInElseBranch) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "정수? x = 10\n"
        "만약 x == 없음 라면:\n"
        "    출력(\"널\")\n"
        "아니면:\n"
        "    정수 y = x + 1\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, NarrowingAndCombination) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "정수? a = 1\n"
        "정수? b = 2\n"
        "만약 a != 없음 && b != 없음 라면:\n"
        "    정수 c = a + b\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, NarrowingCanceledByReassign) {
    TypeChecker tc;
    // 분기 내 재대입은 원(Optional) 타입 기준 + 좁힘 해제 (spec D3)
    auto result = checkSource(tc,
        "정수? x = 10\n"
        "만약 x != 없음 라면:\n"
        "    x = 없음\n"
        "    x + 1\n");
    expectSingleDiagnostic(result, "TC501");  // 재대입 자체는 TC002 아님 (원타입이 Optional)
}

TEST(TypeCheckerTest, NoNarrowingOutsideBranch) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "정수? x = 10\n"
        "만약 x != 없음 라면:\n"
        "    출력(\"ok\")\n"
        "x + 1\n");
    expectSingleDiagnostic(result, "TC501");
}

TEST(TypeCheckerTest, NoNarrowingInElseOfNeq) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "정수? x = 10\n"
        "만약 x != 없음 라면:\n"
        "    출력(\"ok\")\n"
        "아니면:\n"
        "    x + 1\n");
    expectSingleDiagnostic(result, "TC501");
}
```

- [ ] **Step 2: 실패 확인** — Narrowing* 3건이 TC501 발화로 FAIL.
- [ ] **Step 3: 구현** — `type_checker.h` private에 추가:

```cpp
std::vector<std::map<std::string, std::shared_ptr<Type>>> narrowOverlays_;  // 분기 좁힘 (spec D3)
void collectNarrowings(const std::shared_ptr<Expression>& cond, bool forThen,
                       std::map<std::string, std::shared_ptr<Type>>& out);
```

`type_checker.cpp`:

```cpp
// lookup(): 오버레이 우선 (스코프 탐색 앞에 삽입)
std::shared_ptr<Type> TypeChecker::lookup(const std::string& name) {
    for (auto it = narrowOverlays_.rbegin(); it != narrowOverlays_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    // (기존 스코프 → 글로벌 순서 유지)
    ...
}

// declare(): 같은 이름 재선언 시 좁힘 무효화 (선두에 삽입)
void TypeChecker::declare(const std::string& name, std::shared_ptr<Type> type) {
    for (auto& overlay : narrowOverlays_) overlay.erase(name);
    ...
}

// AssignmentStatement 분기: 검사 전에 좁힘 해제 (기존 valueType infer 직후 삽입)
for (auto& overlay : narrowOverlays_) overlay.erase(assign->name);
// → 이후 기존 lookup이 원 타입을 반환하므로 TC002는 원타입 기준

// IfStatement 분기 교체:
if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
    inferExpression(ifStmt->condition);
    std::map<std::string, std::shared_ptr<Type>> thenNarrow, elseNarrow;
    collectNarrowings(ifStmt->condition, true, thenNarrow);
    collectNarrowings(ifStmt->condition, false, elseNarrow);
    narrowOverlays_.push_back(std::move(thenNarrow));
    checkStatement(ifStmt->consequence);
    narrowOverlays_.pop_back();
    narrowOverlays_.push_back(std::move(elseNarrow));
    checkStatement(ifStmt->then);
    narrowOverlays_.pop_back();
    return;
}

// spec D3: `x != 없음`(then) / `x == 없음`(else), `&&`는 then측만 재귀
void TypeChecker::collectNarrowings(const std::shared_ptr<Expression>& cond, bool forThen,
                                    std::map<std::string, std::shared_ptr<Type>>& out) {
    auto* infix = dynamic_cast<InfixExpression*>(cond.get());
    if (!infix || !infix->token) return;
    if (infix->token->type == TokenType::LOGICAL_AND) {
        if (forThen) {
            collectNarrowings(infix->left, true, out);
            collectNarrowings(infix->right, true, out);
        }
        return;
    }
    bool isNeq = infix->token->type == TokenType::NOT_EQUAL;
    bool isEq = infix->token->type == TokenType::EQUAL;
    if (!((forThen && isNeq) || (!forThen && isEq))) return;

    auto* identLeft = dynamic_cast<IdentifierExpression*>(infix->left.get());
    auto* identRight = dynamic_cast<IdentifierExpression*>(infix->right.get());
    auto* nullLeft = dynamic_cast<NullLiteral*>(infix->left.get());
    auto* nullRight = dynamic_cast<NullLiteral*>(infix->right.get());
    IdentifierExpression* ident = (identLeft && nullRight) ? identLeft
                                : (identRight && nullLeft) ? identRight : nullptr;
    if (!ident) return;
    auto type = lookup(ident->name);
    if (!type) return;
    if (auto* opt = dynamic_cast<OptionalType*>(type.get())) {
        out[ident->name] = opt->inner;
    }
}
```

- [ ] **Step 4: 빌드 + TypeCheckerTest PASS + 전체 ctest** (TC501_EqualityExempt 등 기존 Optional 테스트 회귀 주의)
- [ ] **Step 5: Commit** — `feat(types): branch-scoped Optional narrowing (Phase B-1 Task 5)`

### Task 6: 종합 게이트 + 문서

**Files:**
- Modify: `CHANGELOG.md`
- Test: 전체

- [ ] **Step 1: 전체 ctest 통과 확인** (기대: 434 + 신규 ~22 = 456±)
- [ ] **Step 2: 골든 픽스처 `--type-check=warn` 전수 실행** — error_undefined.hik의 TC006(이제 정확한 줄 번호) 외 전부 clean. **회귀 발견 시 plan 중단하고 원인 분석.**
- [ ] **Step 3: REPL 동작 확인** — `정수? x = 10` → `만약 x != 없음 라면:` 블록 입력이 진단 없이 동작.
- [ ] **Step 4: CHANGELOG.md** — Unreleased 추가 섹션에:

```markdown
- 정적 타입 검사기 Phase B-1: 진단에 정확한 줄 번호(AST line), 이항 연산자 검사(`TC601`)와
  결과 타입 정밀 추론, `반복-부터-까지` 범위 타입 검사(`TC301`), `만약 x != 없음 라면:` 분기
  내 Optional 좁히기. 진단은 두 런타임(evaluator·VM)이 모두 거부하는 조합만 발화하며,
  실측에서 발견된 백엔드 불일치 5건은 spec 부록 B에 기록.
```

- [ ] **Step 5: Commit** — `docs: CHANGELOG for Phase B-1` + plan 체크박스 갱신 커밋

---

## 완료 기준 (Definition of Done)

1. 전체 ctest 통과 (기존 434 + 신규 전부)
2. 골든 픽스처 `--type-check=warn` — error_undefined(TC006, 줄 번호 정확) 외 clean
3. TC601/TC301 positive·negative 테스트 존재, 면제 케이스(문자쌍 비교, &&우항, 실수 경계) 테스트로 문서화
4. 좁히기: then/else/&&/재대입 해제/분기 밖 비좁힘 모두 테스트 존재
5. evaluator/vm/environment/object(시그니처 테이블 제외) 파일 변경 없음 (`git diff --stat`)
6. ast/parser 변경은 line 필드·스탬프만 (동작 변경 없음 — 기존 parser/evaluator/vm 테스트 통과로 입증)
7. CHANGELOG 갱신

## Task 의존 그래프

```
Task 0 (실측, 완료) → Task 1 (line) → Task 2 (결과 추론) → Task 3 (TC601)
                                  └→ Task 4 (TC301)   [Task 2의 isPrimKind 의존]
                                  └→ Task 5 (좁히기)   [독립 — Task 1 이후 가능]
Task 6 (종합) ← Task 3, 4, 5
```
