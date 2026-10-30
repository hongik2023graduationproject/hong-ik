# 런타임 일관성 (evaluator/VM 시맨틱 통일) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** spec `2026-06-12-runtime-consistency-design.md`의 결정 테이블대로 evaluator/VM 동작을 통일하고, 타입 체커의 both-reject 면제를 해제한다.

**Architecture:** 이슈별로 골든 패리티 픽스처(`tests/fixtures/golden/` — PhaseZeroGoldens가 양 백엔드 실행) 선행 → red 확인 → 어긋난 백엔드 수정 → both green (spec D6). VM 수정은 새 opcode 2종(OP_DECL_CHECK, OP_ASSERT_BOOL) + binaryOp 조이기 + ITER 검증, evaluator 수정은 블록 스코프·문자열 비교·서브타입 선언.

**Tech Stack:** C++17/26, CMake(풀패스), Google Test, MSVC. 작업 경로: `E:\dev\projects\school\hongik\hong-ik`. 베이스라인 테스트 476개.

**코드 앵커 (2026-06-12 정찰):**
- `vm/compiler.cpp:673` compileInitialization (타입 토큰 버림), `:589-616` &&/|| 점프 컴파일, `:816` compileForEach
- `vm/vm.cpp:134` binaryOp (중앙 디스패치), `:1063` opTypeCheck (typeMap 패턴 재사용)
- `evaluator/evaluator.cpp:155-175` 선언 타입 검사, `:273/:285` evalIf/evalWhile
- `object/object.h:239-273` ClassDef(parent 체인)/Instance(classDef) — 양 백엔드 공용
- `vm/opcode.h` opcode 추가 위치

---

## Task 0: 사전 실측 — ✅ 완료 (2026-06-12)

- [x] 선언 매트릭스 6타입×7값 양 백엔드: **VM은 36개 교차 조합 전부 통과(검사 부재), eval 전부 거부** → OP_DECL_CHECK 도입 근거
- [x] 이항 연산자 784 매트릭스·논리 단락·foreach·블록 스코프: Phase B spec 부록 A/B (2026-06-11)
- [x] 골든 픽스처는 양 백엔드를 이미 통과하므로 **블록 스코프 누수에 의존하는 골든 없음** (VM이 블록 스코프인데 통과 중)
- [ ] **잔여 (Task 7에서):** evaluator_test.cpp에 if/while 블록 내 선언→밖 사용 케이스가 있는지는 블록 스코프 수정 후 테스트 red로 검출 → 의도 갱신

### Task 1: OP_DECL_CHECK — VM 선언 타입 검사 (D1)

**Files:**
- Create: `tests/fixtures/golden/decl_check.hik` + `decl_check.expected.txt`
- Modify: `vm/opcode.h`, `vm/compiler.cpp:673`, `vm/vm.cpp`, `vm/vm.h`, `util/type_utils.h`
- Test: 골든 패리티 (자동)

- [ ] **Step 1: 패리티 픽스처 작성** — `decl_check.hik`:

```
시도:
    정수 a = "문자"
실패 오류:
    출력("거부1")
시도:
    실수 b = 1
실패 오류:
    출력("거부2")
시도:
    정수 c = 없음
실패 오류:
    출력("거부3")
정수 d = 10
실수 e = 1.5
문자 f = "ok"
논리 g = true
배열 h = [1]
사전 i = {"k": 1}
정수? j = 없음
정수? k = 5
출력("통과")
```

`decl_check.expected.txt`:

```
거부1
거부2
거부3
통과
```

- [ ] **Step 2: red 확인** — 빌드 후 전체 ctest: `PhaseZeroGoldens/GoldenTest.VM/decl_check` FAIL (VM이 거부 안 함), `.Eval/decl_check` PASS.
- [ ] **Step 3: 공용 헬퍼** — `util/type_utils.h`에 추가:

```cpp
// 선언 타입명(한글) ↔ ObjectType — 선언 호환 규칙 (런타임 일관성 spec D1).
// Optional/클래스 검사는 호출측에서 처리.
inline bool declTypeMatches(const std::string& typeName, ObjectType valueType) {
    switch (valueType) {
    case ObjectType::INTEGER: return typeName == "정수";
    case ObjectType::FLOAT: return typeName == "실수";
    case ObjectType::STRING: return typeName == "문자";
    case ObjectType::BOOLEAN: return typeName == "논리";
    case ObjectType::ARRAY: return typeName == "배열";
    case ObjectType::HASH_MAP: return typeName == "사전";
    default: return false;
    }
}
```

- [ ] **Step 4: opcode + 컴파일** — `vm/opcode.h`에 `OP_DECL_CHECK` 추가 (끝부분, 기존 나열 형식 따름). `compiler.cpp:673` compileInitialization에서 value 컴파일 직후:

```cpp
void Compiler::compileInitialization(InitializationStatement* stmt) {
    long long line = stmt->type ? stmt->type->line : 0;
    compileExpression(stmt->value.get());

    // 선언 타입 검사 (런타임 일관성 D1) — 타입명 상수 + Optional 플래그
    std::string typeName = stmt->type ? stmt->type->text : "";
    if (stmt->isOptional) typeName += "?";
    uint16_t typeIdx = identifierConstant(typeName);
    chunk().emitOpAndUint16(OpCode::OP_DECL_CHECK, typeIdx, line);

    if (current->scopeDepth > 0) {
        declareLocal(stmt->name);
    } else {
        uint16_t nameIdx = identifierConstant(stmt->name);
        chunk().emitOpAndUint16(OpCode::OP_DEFINE_GLOBAL, nameIdx, line);
    }
}
```

- [ ] **Step 5: VM 핸들러** — `vm.h`에 `void opDeclCheck();` 선언, `vm.cpp` run() 디스패치에 `case OpCode::OP_DECL_CHECK: opDeclCheck(); break;` 추가, 구현 (opTypeCheck 옆):

```cpp
// 선언 타입 검사 (런타임 일관성 D1). 스택 최상단(선언 값)을 peek — pop하지 않는다
// (이후 DEFINE_GLOBAL/declareLocal이 같은 값을 사용).
void VM::opDeclCheck() {
    uint16_t typeIdx = readUint16();
    auto typeNameObj = checkedConstant(currentFrame().function->constants, typeIdx, currentLine());
    std::string typeName = typeNameObj->ToString();

    bool optional = !typeName.empty() && typeName.back() == '?';
    if (optional) typeName.pop_back();

    const VMValue& value = peek(0);
    if (value.isNull()) {
        if (optional) return;  // 타입? <- 없음 허용
        throw RuntimeException("선언에서 자료형과 값의 타입이 일치하지 않습니다.", currentLine());
    }

    ObjectType valueType;
    switch (value.kind()) {
    case ValueTag::INT: valueType = ObjectType::INTEGER; break;
    case ValueTag::FLOAT: valueType = ObjectType::FLOAT; break;
    case ValueTag::BOOL: valueType = ObjectType::BOOLEAN; break;
    case ValueTag::NULL_V: valueType = ObjectType::NULL_OBJ; break;
    case ValueTag::OBJECT: valueType = value.asObject()->type; break;
    }

    // 클래스 타입 표기: 인스턴스 + 부모 체인 (D1, 서브타입은 Task 2와 공유)
    if (valueType == ObjectType::INSTANCE) {
        auto* inst = static_cast<Instance*>(value.asObject().get());
        for (auto def = inst->classDef; def; def = def->parent) {
            if (def->name == typeName) return;
        }
        throw RuntimeException("선언에서 자료형과 값의 타입이 일치하지 않습니다.", currentLine());
    }
    // 함수/클래스 값 등 비기본형은 기존 관대 동작 유지 (실측 범위 밖 — 거짓 거부 방지)
    if (valueType == ObjectType::FUNCTION || valueType == ObjectType::CLASS_DEF
        || valueType == ObjectType::BUILTIN_FUNCTION || valueType == ObjectType::TUPLE
        || valueType == ObjectType::GENERATOR) {
        return;
    }
    if (!declTypeMatches(typeName, valueType)) {
        throw RuntimeException("선언에서 자료형과 값의 타입이 일치하지 않습니다.", currentLine());
    }
}
```

(`#include "../object/object.h"`는 vm.cpp에 이미 있음. `type_utils.h` include 확인.)

- [ ] **Step 6: both green + 전체 ctest** — decl_check 골든 양 백엔드 통과, 기존 골든·테스트 무회귀 (classes_basic의 `점 p = 점(...)` 인스턴스 선언 포함).
- [ ] **Step 7: Commit** — `feat(vm): declaration type checking via OP_DECL_CHECK (runtime consistency D1)`

### Task 2: 서브타입 대입 허용 — evaluator (D1/#6)

**Files:**
- Create: `tests/fixtures/golden/subtype_assign.hik` + `.expected.txt`
- Modify: `evaluator/evaluator.cpp:155-175` (선언 검사의 INSTANCE 분기)

- [ ] **Step 1: 픽스처** — `subtype_assign.hik`:

```
클래스 동물:
    문자 이름
    생성(문자 이름):
        자기.이름 = 이름
    함수 소개() -> 문자:
        리턴 자기.이름
클래스 강아지 < 동물:
    함수 소리() -> 문자:
        리턴 "멍멍"
동물 a = 강아지("뽀삐")
출력(a.소개())
시도:
    강아지 d = 동물("미아")
실패 오류:
    출력("부모를 자식에 거부")
```

`subtype_assign.expected.txt`:

```
뽀삐
부모를 자식에 거부
```

- [ ] **Step 2: red 확인** — Eval/subtype_assign FAIL (`동물 a = 강아지(...)` 거부), VM은 Task 1의 부모 체인 검사로 PASS.
- [ ] **Step 3: evaluator 수정** — `evaluator.cpp:155-175`의 선언 검사에서 INSTANCE 케이스를 정확 일치 → 부모 체인으로. 기존 코드의 클래스명 비교 지점을 다음 패턴으로 교체 (해당 지점의 실제 변수명에 맞춤):

```cpp
// 클래스 타입 선언: 자손 클래스 인스턴스 허용 (런타임 일관성 D1 — VM opDeclCheck와 동일 규칙)
if (auto* inst = dynamic_cast<Instance*>(value.get())) {
    bool matched = false;
    for (auto def = inst->classDef; def; def = def->parent) {
        if (def->name == declaredTypeName) { matched = true; break; }
    }
    if (!matched) {
        throw RuntimeException("선언에서 자료형과 값의 타입이 일치하지 않습니다.", current_line);
    }
    // 통과
}
```

- [ ] **Step 4: both green + 전체 ctest**
- [ ] **Step 5: Commit** — `feat(eval): subtype-aware declaration check (runtime consistency #6)`

### Task 3: 이항 연산자 통일 (D3) — VM 조이기 + eval 문자열 비교

**Files:**
- Create: `tests/fixtures/golden/binary_strict.hik` + `.expected.txt`, `tests/fixtures/golden/string_compare.hik` + `.expected.txt`
- Modify: `vm/vm.cpp:134` binaryOp, `evaluator/evaluator.cpp` (비교 연산 분기 — `:780-810` 부근 "좌항과 우항의 타입을 연산할 수 없습니다" 경로)

- [ ] **Step 1: 픽스처 2종**

`binary_strict.hik` (VM이 현재 통과시키는 eval-only-reject 대표 조합 — 부록 A 94조합의 카테고리별 대표):

```
시도:
    출력(1.5 + "a")
실패 오류:
    출력("거부1")
시도:
    출력(true - 1.5)
실패 오류:
    출력("거부2")
시도:
    출력([1] * 1.5)
실패 오류:
    출력("거부3")
시도:
    출력(1.5 < true)
실패 오류:
    출력("거부4")
시도:
    출력(1.5 == "a")
실패 오류:
    출력("거부5")
출력(1 + 1.5)
출력(2 ** 3)
```

`binary_strict.expected.txt`:

```
거부1
거부2
거부3
거부4
거부5
2.5
8
```

`string_compare.hik`:

```
출력("a" < "b")
출력("나" > "가")
출력("abc" <= "abc")
출력("b" >= "c")
```

`string_compare.expected.txt` — **VM 현행 출력을 먼저 실행해 채운다** (사전순 기대: true/true/true/false — 실행값으로 박제):

```
true
true
true
false
```

- [ ] **Step 2: red 확인** — VM/binary_strict FAIL, Eval/string_compare FAIL.
- [ ] **Step 3: VM binaryOp 조이기** — `vm.cpp:134` binaryOp를 전체 읽고, 허용 경로(Int×Int, Float 혼합 숫자쌍, 문자쌍의 `+`·비교·등호, 논리쌍·문자쌍의 등호, null 등호)를 **명시 나열**한 뒤 마지막 fall-through를 throw로:

```cpp
throw RuntimeException("좌항과 우항의 타입을 연산할 수 없습니다.", line);
```

기존의 암묵 코어션 경로(있다면 — 예: 비숫자 → 숫자 변환, 문자+비문자 연결)를 제거. 등호의 cross-type(`정수 == 문자` 등)은 이미 throw인지 확인하고 아니면 추가 — 부록 A.1 허용표가 정답.

- [ ] **Step 4: eval 문자열 비교 추가** — evaluator의 비교 분기(`< > <= >=`)에서 양쪽 String이면:

```cpp
// 문자열 사전순 비교 (런타임 일관성 D3 — VM과 동일: std::string byte-wise)
if (auto* ls = dynamic_cast<String*>(left.get())) {
    if (auto* rs = dynamic_cast<String*>(right.get())) {
        switch (op_token_type) {
        case TokenType::LESS_THAN: return make_shared<Boolean>(ls->value < rs->value);
        case TokenType::GREATER_THAN: return make_shared<Boolean>(ls->value > rs->value);
        case TokenType::LESS_EQUAL: return make_shared<Boolean>(ls->value <= rs->value);
        case TokenType::GREATER_EQUAL: return make_shared<Boolean>(ls->value >= rs->value);
        default: break;
        }
    }
}
```

(evaluator의 실제 비교 처리 함수 시그니처에 맞춰 삽입 — `evalInfixExpression` 계열, "좌항과 우항의 타입을..." throw보다 앞에.)

- [ ] **Step 5: both green + 전체 ctest** — 특히 기존 골든 `ops_misc`/`arith_basic` 무회귀.
- [ ] **Step 6: Commit** — `feat(vm,eval): unify binary operator semantics (runtime consistency D3)`

### Task 4: 논리 연산자 논리 전용 — VM (D4)

**Files:**
- Create: `tests/fixtures/golden/logical_bool.hik` + `.expected.txt`
- Modify: `vm/opcode.h` (OP_ASSERT_BOOL), `vm/compiler.cpp:589-616`, `vm/vm.cpp` + `vm/vm.h`

- [ ] **Step 1: 픽스처** — `logical_bool.hik`:

```
출력(true && false)
출력(false || true)
출력(false && true || true)
시도:
    출력(false || 1)
실패 오류:
    출력("거부1")
시도:
    출력(1 && true)
실패 오류:
    출력("거부2")
출력(false && 1)
```

`logical_bool.expected.txt` (`false && 1`은 단락 평가로 우항 미평가 → false — 실측 합치):

```
false
true
true
거부1
거부2
false
```

- [ ] **Step 2: red 확인** — VM/logical_bool FAIL (`false || 1` → 1 출력).
- [ ] **Step 3: OP_ASSERT_BOOL** — opcode 추가, 핸들러:

```cpp
// 평가된 논리 연산자 피연산자는 논리여야 한다 (런타임 일관성 D4). peek — pop 안 함.
void VM::opAssertBool() {
    if (!peek(0).isBool()) {
        throw RuntimeException("논리 연산자(&&, ||)의 피연산자는 논리 타입이어야 합니다.", currentLine());
    }
}
```

compiler.cpp의 &&/|| 분기에서 `compileExpression(expr->left.get());`와 `compileExpression(expr->right.get());` **각각 직후에** `chunk().emitOp(OpCode::OP_ASSERT_BOOL, line);` 삽입 (좌항 1곳 + 우항 1곳 × && / || = 총 4곳). 단락 점프 구조는 그대로 — 미평가 경로는 검사 없음 (eval 시맨틱 합치).

- [ ] **Step 4: eval 메시지 합치 확인** — eval의 기존 비논리 거부 메시지를 확인해 픽스처가 양쪽에서 같은 출력(거부N)으로 수렴하는지 확인 (try/catch가 메시지를 삼키므로 출력은 "거부N" — 메시지 문구 통일은 선택).
- [ ] **Step 5: both green + 전체 ctest** (기존 단락 평가 의존 골든 — loops_break/if_else — 무회귀)
- [ ] **Step 6: Commit** — `feat(vm): bool-only logical operators via OP_ASSERT_BOOL (runtime consistency D4)`

### Task 5: foreach 엄격화 — VM (D5)

**Files:**
- Create: `tests/fixtures/golden/foreach_strict.hik` + `.expected.txt`
- Modify: `vm/compiler.cpp:816` compileForEach, `vm/vm.cpp` OP_ITER_INIT/OP_ITER_VALUE 핸들러

- [ ] **Step 1: 픽스처** — `foreach_strict.hik`:

```
각각 정수 n [1, 2] 에서:
    출력(n)
각각 문자 c "ab" 에서:
    출력(c)
시도:
    각각 정수 x 5 에서:
        출력(x)
실패 오류:
    출력("거부1")
시도:
    각각 문자 k {"k": 1} 에서:
        출력(k)
실패 오류:
    출력("거부2")
시도:
    각각 정수 y "ab" 에서:
        출력(y)
실패 오류:
    출력("거부3")
```

`foreach_strict.expected.txt`:

```
1
2
a
b
거부1
거부2
거부3
```

- [ ] **Step 2: red 확인** — VM/foreach_strict FAIL (거부1·2는 침묵 0회, 거부3은 통과).
- [ ] **Step 3: VM 수정** — ① `OP_ITER_INIT` 핸들러: iterable이 Array/String 외면 `RuntimeException("순회할 수 없는 타입입니다.", ...)` (eval 메시지 확인 후 동일 문구 사용 — eval의 foreach 거부 메시지를 grep해서 채택). ② 원소 타입: compileForEach에서 `stmt->elementType->text`를 상수로 동봉하는 operand를 OP_ITER_VALUE에 추가하거나, 더 단순하게 — OP_ITER_VALUE 직후 Task 1의 `OP_DECL_CHECK`를 재사용해 `emitOpAndUint16(OpCode::OP_DECL_CHECK, identifierConstant(stmt->elementType->text), line)` 삽입 (원소가 스택 최상단에 있는 시점). eval의 원소 타입 검사 시맨틱과 동일해지는지 픽스처로 확인.
- [ ] **Step 4: both green + 전체 ctest** (for_each 골든 무회귀)
- [ ] **Step 5: Commit** — `feat(vm): strict foreach — reject non-iterables and element type mismatch (runtime consistency D5)`

### Task 6: 블록 스코프 — evaluator (D2)

**Files:**
- Create: `tests/fixtures/golden/block_scope.hik` + `.expected.txt`
- Modify: `evaluator/evaluator.cpp:273` evalIf, `:285` evalWhile (+ foreach/forrange/try 블록 평가 지점 — 같은 파일 내 BlockStatement 평가 경로 확인)

- [ ] **Step 1: 픽스처** — `block_scope.hik`:

```
정수 바깥 = 1
만약 true 라면:
    정수 안쪽 = 10
    바깥 = 바깥 + 안쪽
출력(바깥)
시도:
    출력(안쪽)
실패 오류:
    출력("블록밖 거부")
```

`block_scope.expected.txt`:

```
11
블록밖 거부
```

- [ ] **Step 2: red 확인** — Eval/block_scope FAIL (안쪽이 11 다음 10으로 출력됨), VM PASS.
- [ ] **Step 3: evaluator 수정** — evalIf/evalWhile/evalForEach/evalForRange/evalTryCatch의 블록 본문 평가를 새 enclosed Environment로 감싼다. 기존 Environment 생성 패턴(클래스/함수 호출부에서 사용 중인 방식)을 따름:

```cpp
// 블록 스코프 (런타임 일관성 D2 — VM 시맨틱 합치): 블록 내 선언은 밖에서 안 보임
auto blockEnv = std::make_shared<Environment>(environment);  // 기존 enclosed 생성 방식 확인 후 동일하게
eval(statement->consequence.get(), blockEnv.get());
```

(Environment의 enclosed 생성자/팩토리 시그니처는 environment.h 확인 후 그대로 사용. while/forrange는 반복마다 새 env가 아니라 **루프 전체에 1개** — VM 동작과 비교 실측해 합치시킬 것: 간단 픽스처 `반복 ... 동안: 정수 t = 1`을 VM에서 돌려 회차별 재선언 허용 여부 확인 후 동일하게.)

- [ ] **Step 4: red 잔여 정리** — evaluator_test 중 누수 의존 케이스가 깨지면 의도 갱신 (수정 내역 커밋 메시지에 명시). 골든은 Task 0에서 무의존 확인됨.
- [ ] **Step 5: both green + 전체 ctest**
- [ ] **Step 6: Commit** — `feat(eval): block scoping for if/while/foreach/forrange/try (runtime consistency D2)`

### Task 7: 타입 체커 면제 해제 (D7)

**Files:**
- Modify: `analyzer/type_checker_expr.cpp` (TC601 우항·문자쌍 비교 결과, TC302), `analyzer/type_checker.cpp` (TC301 실수)
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성**

```cpp
// ---- 런타임 일관성 D7: 면제 해제 ----

TEST(TypeCheckerTest, TC601_LogicalRightNowChecked) {
    TypeChecker tc;
    // 런타임 통일로 우항도 논리 강제 — 면제 해제
    expectSingleDiagnostic(checkSource(tc, "출력(true && 1)\n"), "TC601");
}

TEST(TypeCheckerTest, StringComparisonIsBoolNow) {
    TypeChecker tc;
    EXPECT_TRUE(checkSource(tc, "논리 b = \"a\" < \"b\"\n").diagnostics.empty());
    TypeChecker tc2;
    expectSingleDiagnostic(checkSource(tc2, "정수 n = \"a\" < \"b\"\n"), "TC001");  // 결과 논리
}

TEST(TypeCheckerTest, TC302_NonIterableForeach) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "각각 정수 n 5 에서:\n"
        "    출력(n)\n");
    expectSingleDiagnostic(result, "TC302");
}

TEST(TypeCheckerTest, TC302_HashMapForeach) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "사전 d = {\"k\": 1}\n"
        "각각 문자 k d 에서:\n"
        "    출력(k)\n");
    expectSingleDiagnostic(result, "TC302");
}

TEST(TypeCheckerTest, TC301_FloatBoundNowChecked) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "반복 정수 i = 0.5 부터 3 까지:\n"
        "    출력(i)\n");
    expectSingleDiagnostic(result, "TC301");
}
```

기존 테스트 3건 의도 갱신: `TC601_LogicalRightUnchecked` 삭제(위 LogicalRightNowChecked로 대체), `TC601_StringComparisonExempt` → 0건 유지하되 주석을 "정식 허용"으로, `TC301_FloatBoundExempt` 삭제(FloatBoundNowChecked로 대체).

- [ ] **Step 2: red 확인**
- [ ] **Step 3: 구현**
  - `type_checker_expr.cpp` 논리 분기: 우항 검사 추가 — `if (dynamic_cast<PrimType*>(right.get()) && !isPrimKind(*right, ObjectType::BOOLEAN)) warnBinaryIncompatible(...)` (좌항 검사 옆).
  - 비교 분기: `if (strPair) return makePrim(ObjectType::BOOLEAN);` (Any → 논리 정밀화, 면제 주석 제거).
  - `type_checker.cpp` ForEach 분기에 TC302 (iterable 추론 결과가 PrimType이고 ARRAY/STRING이 아니면):

```cpp
auto iterableType = inferExpression(forEach->iterable);
if (dynamic_cast<PrimType*>(iterableType.get())
    && !isPrimKind(*iterableType, ObjectType::ARRAY)
    && !isPrimKind(*iterableType, ObjectType::STRING)) {
    warn(currentLine_, "TC302",
         "'" + iterableType->toKorean() + "' 타입은 순회할 수 없습니다. 배열 또는 문자만 가능합니다.");
}
```

  - forRange checkBound에 `|| isPrimKind(*t, ObjectType::FLOAT)` 추가.
- [ ] **Step 4: 전체 ctest + 골든 스윕 clean**
- [ ] **Step 5: Commit** — `feat(types): lift both-reject exemptions after runtime unification (D7)`

### Task 8: 종합 검증 + 문서

- [ ] **Step 1: 784 매트릭스 재실행** — B-1 Task 0과 동일한 생성 스크립트로 16연산자×7×7을 양 백엔드 실행, **불일치 0건** 확인 (문자쌍 비교 4조합은 both-OK로 이동했는지 확인). 선언 42 매트릭스도 재실행 — 불일치 0.
- [ ] **Step 2: 벤치 비교** — `HongIkBench`를 수정 전 커밋과 현 HEAD에서 실행, OP_DECL_CHECK/ASSERT_BOOL로 인한 유의미한 회귀(>5%) 없는지 확인.
- [ ] **Step 3: CHANGELOG** — `### 변경 (Changed)` 섹션: 선언 타입 검사 도입(웹/VM 포함), 논리 연산자 논리 전용(JS식 값 반환 제거), foreach 비순회형 에러화, 블록 스코프(`--eval`), 문자열 사전순 비교(`--eval`에 추가), 서브타입 선언 허용(`--eval`).
- [ ] **Step 4: Phase B spec 부록 B 갱신** — #1~#6에 "해소 (2026-06-12 런타임 일관성 작업)" 표기.
- [ ] **Step 5: Commit + plan 체크박스 갱신**

---

## 완료 기준 (Definition of Done)

1. 신규 패리티 픽스처 6종이 양 백엔드 동일 출력 (PhaseZeroGoldens 자동)
2. 784 이항 매트릭스 + 42 선언 매트릭스 재실행 시 백엔드 불일치 **0건**
3. 기존 전체 테스트 통과 (의도 갱신분은 커밋 메시지에 명시)
4. 체커 면제 해제 4건 반영 + 테스트
5. parser/lexer/ast/object(빌트인 .cpp) 무변경 — evaluator/vm/util(type_utils)만
6. 벤치 회귀 없음, CHANGELOG Changed 공지, Phase B spec 부록 B 해소

## Task 의존 그래프

```
Task 0 (실측, 완료)
Task 1 (VM 선언 검사) → Task 2 (eval 서브타입)   [둘 다 D1 규칙 공유]
Task 3 (이항 연산자) — 독립
Task 4 (논리 연산자) — 독립
Task 5 (foreach) — Task 1의 OP_DECL_CHECK 재사용
Task 6 (블록 스코프) — 독립 (가장 위험 → 뒤쪽 배치)
Task 7 (체커 면제 해제) ← Task 3, 4, 5 완료 후
Task 8 (종합) ← 전부
```
