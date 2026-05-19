# 정적 타입 시스템 Phase A 구현 Plan

Revision: v3 (2차 셀프 리뷰 + evaluator 동작 검증 결과 반영 — top-down 검사, AST 노드 6종 처리, 빌트인 실측 가이드 등)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Spec:** `docs/superpowers/specs/2026-05-19-static-type-system-design.md` (v3)

**Goal:** 파서 결과(AST)에 대해 동작하는 정적 타입 검사기 `analyzer::TypeChecker`를 신설하고, 변수 선언/대입/함수 호출/리턴/미선언 식별자/Nullable 미해제까지 7개 진단(TC001/TC002/TC006/TC101/TC102/TC103/TC501)을 경고(warning) 모드로 출력한다. 기존 evaluator·vm·런타임 검사 동작은 **변경 0**. `--type-check=` CLI 플래그로 활성화. REPL은 항상 warn 모드(단일 인스턴스).

**Architecture:** 새 파일만 추가하는 추출형(non-invasive) 디자인. 기존 AST/Object/Environment/VM에 손대지 않는다. 통합 지점은 `repl/repl.cpp` 파서 직후, `main.cpp` CLI 파싱 두 곳 한정.

**Tech Stack:** C++17/26, CMake 3.28+, Google Test, MSVC. 작업 경로: `E:\dev\projects\school\hongik\hong-ik`.

---

## 사전 결정 사항 (spec v3 기준)

### 디렉토리·파일 신설

```
util/
  type.h                    (신규)   - Type 계층 선언
  type.cpp                  (신규)   - isAssignableFrom 등 구현
object/
  builtin_signatures.h      (신규)   - 빌트인 46개 시그니처 테이블 (D2.1)
analyzer/
  type_checker.h            (신규)   - TypeChecker 인터페이스
  type_checker.cpp          (신규)   - 검사 로직 + 빌트인 prepopulate
tests/
  type_checker_test.cpp     (신규)
```

### Type 계층 (Phase A 한정 — spec D2)

```cpp
class Type { ... };
class PrimType : public Type { ObjectType kind; };       // 정수/실수/문자/논리/배열/사전/튜플/없음
class OptionalType : public Type { std::shared_ptr<Type> inner; };
class FunctionType : public Type {
    std::vector<std::shared_ptr<Type>> params;  // Optional은 OptionalType으로 래핑
    std::shared_ptr<Type> ret;
    std::vector<bool> paramHasDefault;
};
class BuiltinFunctionType : public Type {
    std::string name;
    int minArity, maxArity;
    bool skipArgTypeCheck;
    std::shared_ptr<Type> ret;
};
class ClassType : public Type {
    std::string name;
    int constructorArity;
    std::shared_ptr<ClassDef> def;  // Phase A 미사용
};
class AnyType : public Type { };
class NeverType : public Type { };
```

v2에 있던 `paramOptionals` 필드 제거 — OptionalType 래핑으로 표현 (spec D2 Z8 반영).

### 진단 코드 (spec D8)

| 코드 | 검사 | 메시지 템플릿 |
|---|---|---|
| TC001 | 변수 선언 타입 불일치 | `'정수' 타입 변수 'x'에 '문자' 값을 대입할 수 없습니다.` |
| TC002 | 변수 재대입 타입 불일치 | `'정수' 타입 변수 'x'에 '실수' 값을 대입할 수 없습니다.` |
| TC006 | 미선언 식별자 | `식별자 'foo'를 찾을 수 없습니다.` |
| TC101 | 함수/생성자 호출 인자 개수 | `함수 '더하기'는 2개의 매개변수를 받지만 1개가 전달되었습니다.` |
| TC102 | 함수 호출 인자 타입 불일치 | `함수 '더하기'의 매개변수 'a'는 '정수' 타입이지만 '문자' 값이 전달되었습니다.` |
| TC103 | 함수 리턴 타입 불일치 | `함수 '계산'의 반환 타입 '정수'에 '실수' 값을 반환할 수 없습니다.` |
| TC501 | Nullable 미해제 사용 | `Optional 타입 '정수?' 값에 대해 직접 연산할 수 없습니다. null 검사 후 사용하세요.` |

### 핵심 규칙 (spec 1.1)

- `T <- T` 동일 OK
- `T? <- T` OK (좁히기)
- `T? <- 없음` OK
- `T <- 없음` 거부 (TC001/TC002/TC102)
- `any <-> *` 항상 OK
- `never` 한쪽에 있으면 통과 (cascade 차단)
- 이항 연산자 한쪽이 NeverType이면 결과 NeverType
- **`실수 <- 정수` 승격은 Task 0에서 evaluator 동작 검증 후 결정**

### 검사 실패 시 환경 갱신 (spec 2.1)

`declared(표기)` 타입을 그대로 스코프에 등록. NeverType 사용하지 않음. 후속 진단 dedup하지 않음.

### Hoisting 미지원 (spec D3.1, Z1)

evaluator가 `FunctionStatement`를 만나는 순간 환경에 등록(`evaluator.cpp:446-457`)하는 top-down 평가이므로, TypeChecker도 동일하게 **top-down 검사**. 재귀는 함수 본문 진입 직전에 시그니처 declare로 처리. Forward reference는 의도된 TC006.

### AST 노드 6종 처리 (spec D6.5, Z2)

| AST 노드 | 처리 |
|---|---|
| `ForEachStatement` | 새 스코프 → `elementName`을 `elementType`으로 declare → iterable infer → body traverse → 스코프 pop |
| `ForRangeStatement` | 새 스코프 → `varName`을 `varType`으로 declare → startExpr/endExpr infer → body traverse → 스코프 pop |
| `TryCatchStatement` | tryBody traverse → 새 스코프 → `errorName`을 AnyType으로 declare → catchBody traverse → 스코프 pop |
| `MatchStatement` | subject infer → 각 caseValue/caseGuard infer → 각 caseBody traverse → defaultBody traverse |
| `YieldStatement` | expression infer → 결과 무시 |
| `IndexAssignmentStatement` | collection/index/value 모두 infer → 타입 일치 검사 안 함 |

### CLI (spec D4)

```
hong-ik [--vm] [--type-check=off|warn|strict] <file.hik>
hong-ik [--type-check=off|warn|strict]               # REPL, strict는 warn으로 강등 + 안내 1회
```

기본값:
- 파일 모드: `warn`
- REPL 모드: `warn` 강제 (strict 받으면 안내 메시지 후 warn으로)
- `off`: TypeChecker 호출 자체 생략

### 진단 출력 형식

```
type-warning[TCxxx] [줄 N] <메시지>     # warn 모드
type-error[TCxxx]   [줄 N] <메시지>     # strict 모드에서만 'error'
```

stderr로 출력.

### 클래스 처리 규칙 (spec D6)

Phase A는 클래스 본문 내부를 traverse하지 않는다.

1. `ClassStatement` 발견 시 이름을 `ClassType(name, constructorArity)`로 현재 스코프와 `classTypes_`에 등록.
2. 생성자 호출의 **인자 개수**만 검사 (TC101). 인자 타입 검사 안 함.
3. 메서드 본문 미진입.
4. `자기.x` / `인스턴스.x` / `인스턴스.f()` 접근은 모두 `AnyType` 추론.
5. 상속 관계는 ClassType 단순 이름 일치 또는 AnyType 우회로 통과시킴.
6. 부모 미선언이면 TC006 (`클래스 강아지 < 모름:` → 모름이 classTypes_에 없으면).

### 호환성 가드

- AST·Object(builtin_signatures.h 제외)·Environment·evaluator·vm·parser는 **읽기만** 한다. 헤더 include는 OK, 정의 변경은 NO.
- 런타임 검사 코드는 **그대로 둔다**. 정적 검사와 이중으로 동작한다.
- 기존 모든 테스트가 통과해야 한다.

---

## Tasks

### Task 0: evaluator 사전 검증

진행 전 사전 조사. spec 1.1의 승격 규칙과 D2.1의 빌트인 시그니처 결정에 필요. **v3 작성 시점에 Z1·Z2는 grep으로 이미 확인됨** (top-down 평가 + 6종 AST 노드 처리 정책 박제). Task 0에서는 Z3(빌트인 arity 실측)와 `실수 <- 정수` 동작 확인이 주.

- [ ] **Z1 재확인(formality):** `evaluator.cpp:446-457`이 `FunctionStatement`를 만나는 순간에만 environment->Set 한다는 사실 재확인. 다른 위치에 pre-pass 코드가 추가되어 있지 않은지 grep.
- [x] **`실수 <- 정수` 동작:** 2026-05-19 실측 완료. **tree-walker 거부, VM 통과 — 불일치.** Phase A는 D3 안전망 원칙에 따라 **거부** 채택. spec 1.1에 박제됨. VM 관대 동작은 별도 이슈로 분리.
- [x] **빌트인 46개 arity 실측 (2026-05-19 완료):** 전 6개 .cpp 파일 코드 확인. Task 3 시그니처 테이블에 카테고리별 분류 박제:
  - **io.cpp** (4): `출력`(0,INT_MAX), `입력`(0,1), `파일읽기`(1,1), `파일쓰기`(2,2)
  - **math.cpp** (16): `절대값`/`제곱근`(1), `최대`/`최소`/`난수`(2), `사인`/`코사인`/`탄젠트`/`로그`/`자연로그`(1), `거듭제곱`(2), `파이`/`자연수e`(0), `반올림`/`올림`/`내림`(1)
  - **string.cpp** (9, `반복` 제외): `분리`(2), `대문자`/`소문자`(1), `치환`(3), `자르기`(1), `시작확인`/`끝확인`(2), `채우기`/`부분문자`(3)
  - **array.cpp** (5): `추가`(2), `정렬`/`뒤집기`(1), `찾기`(2), `조각`(3)
  - **collection.cpp** (5): `길이`/`키목록`(1), `포함`(2), `설정`(3), `삭제`(2)
  - **conversion.cpp** (4): `타입`/`정수변환`/`실수변환`/`문자변환`(1)
  - **json.cpp** (2): `JSON_파싱`/`JSON_직렬화`(1)
  - **확정 이슈 — `반복` 제외 (검증 완료, 2026-05-20):** lexer가 `반복`을 항상 키워드 토큰으로 분류하고 parser는 표현식 위치에 식별자 fallback이 없음(`parser.cpp:189`). 사용자 텍스트에서 호출 불가능한 dead builtin. **Phase A 시그니처 테이블에 등록하지 않음 → 빌트인 총 45개로 축소.** spec D2.1에 박제됨.
- [x] **Z1 (hoisting) 재확인:** `evaluator.cpp`의 `FunctionStatement` 참조는 line 115(line tracking)와 446(eval-on-encounter)뿐. pre-pass 없음 — top-down 확정.
- [x] **결과 박제 완료:** spec 1.1, D2.1, 본 plan Task 1·Task 3에 반영 완료.

**게이트:** 결정만 기록. 코드 변경 없음.

### Task 1: Type 계층 골격 (`util/type.{h,cpp}`)

- [ ] `util/type.h` 생성 — Type 추상 클래스 + PrimType, OptionalType, FunctionType, BuiltinFunctionType, ClassType, AnyType, NeverType 선언 (paramOptionals 없음)
- [ ] `util/type.cpp` 생성 — 각 구체 클래스의 `isAssignableFrom`, `toKorean`, `equals` 구현. spec 1.1.1 의사코드 그대로 따름
- [ ] 헬퍼 함수 `makePrim(ObjectType)`, `makeOptional(shared_ptr<Type>)`, `makeFunction(...)`, `makeBuiltin(...)`, `makeAny()`, `makeNever()`, `makeClass(name, arity)`
- [ ] `CMakeLists.txt`에 새 파일 등록
- [ ] **게이트:** `cmake --build cmake-build-debug` 통과

**검증 포인트:**
- PrimType(INTEGER).isAssignableFrom(PrimType(INTEGER)) == true
- PrimType(INTEGER).isAssignableFrom(PrimType(STRING)) == false
- OptionalType(INTEGER).isAssignableFrom(PrimType(INTEGER)) == true
- OptionalType(INTEGER).isAssignableFrom(PrimType(NULL_OBJ)) == true
- PrimType(INTEGER).isAssignableFrom(PrimType(NULL_OBJ)) == false
- AnyType.isAssignableFrom(any) == true
- anything.isAssignableFrom(AnyType) == true
- NeverType 양방향 == true
- (Task 0 결과에 따라 추가) PrimType(FLOAT).isAssignableFrom(PrimType(INTEGER)) == true/false

### Task 2: Type 단위 테스트

- [ ] `tests/type_checker_test.cpp` 생성 — 우선 `TEST(TypeTest, AssignabilityRules)` 한 테스트로 위 검증 포인트 8~9건 모두 커버
- [ ] `tests/CMakeLists.txt`에 등록 (GLOB 패턴이면 자동, 아니면 수동 추가)
- [ ] **게이트:** `ctest --test-dir cmake-build-debug -R TypeTest` 통과

### Task 3: 빌트인 시그니처 테이블 (`object/builtin_signatures.h`)

spec D2.1 — Phase A의 핵심 데이터 항목. **Task 0 실측 결과를 인용한다 — README 기반 추정 금지.**

- [ ] `object/builtin_signatures.h` 생성
- [ ] `struct BuiltinSignature { const char* name; int minArity; int maxArity; bool skipArgTypeCheck; ObjectType retKind; bool retIsAny; };`
- [ ] **45개** 빌트인 채움(`반복` 제외, Task 0 확정 이슈). 각 항목 옆에 출처 파일·줄 주석 (예: `{"길이", 1, 1, true, INTEGER, false}, // collection.cpp:11`)
- [ ] **카테고리별 기본 분류 (Task 0에서 정밀화):**
  - I/O: `출력` (1, INT_MAX), `입력` (0, 1), `파일읽기` (1, 1), `파일쓰기` (2, 2) — 모두 skipArgTypeCheck=true
  - 변환: `정수변환`/`실수변환`/`문자변환`/`타입` (1, 1) — skipArgTypeCheck=true. ret: 정수변환=INTEGER, 실수변환=FLOAT, 문자변환=STRING, 타입=STRING
  - 컬렉션 조작: `길이` (1, INTEGER), `추가`/`설정`/`삭제` (각자 arity), `키목록` (1, ARRAY), `포함` (2, BOOLEAN)
  - 수학: `절대값` (1), `제곱근` (1), `최대`/`최소`/`난수` (2), `사인`/`코사인`/`탄젠트` (1), `로그`/`자연로그` (1), `거듭제곱` (2), `파이`/`자연수e` (0)
  - 반올림: `반올림`/`올림`/`내림` (1, INTEGER)
  - 문자열: `분리` (2, ARRAY), `대문자`/`소문자` (1, STRING), `치환` (3, STRING), `자르기` (1, STRING), `반복` (2, STRING), `채우기` (3, STRING), `부분문자` (3, STRING), `시작확인`/`끝확인` (2, BOOLEAN)
  - 배열: `정렬` (1, ARRAY), `뒤집기` (1, ret=Any), `찾기` (2, INTEGER), `조각` (3, ARRAY), `매핑`/`걸러내기` (2, ARRAY), `줄이기` (3, ret=Any)
  - JSON: `JSON_파싱` (1, ret=Any), `JSON_직렬화` (1, STRING)
- [ ] **재검증:** `built_in.h`에 등록된 클래스 46개가 모두 시그니처 테이블에도 있는지 점검 (수동 또는 간단한 check 함수). 누락 시 빌드 실패하도록 static_assert 또는 빌드 시 확인 스크립트.
- [ ] **게이트:** built_in.h ↔ builtin_signatures.h 1:1 매칭 확인

### Task 4: TypeChecker 골격 + 빌트인 prepopulate

- [ ] `analyzer/type_checker.h` — `TypeDiagnostic`, `Severity`, `TypeChecker::Result`, `TypeChecker` 클래스 골격 (spec 2.1 그대로)
- [ ] `analyzer/type_checker.cpp` — 빈 `check(Program*)`, `pushScope/popScope/declare/lookup/error/warn` 구현
- [ ] 생성자에서 `registerBuiltins()` 호출 — Task 3의 시그니처 테이블을 순회하며 globalTypes_에 BuiltinFunctionType으로 등록
- [ ] `typeFromToken(Token*, bool optional)` 유틸 — 토큰을 `Type`으로 변환. 기본 키워드(정수/실수/문자/논리/배열/사전)는 PrimType, IDENTIFIER 토큰은 classTypes_에서 lookup. 미등록 클래스 이름은 AnyType + 진단 보류 (**Task 10에서 처리** — 클래스 선언 등록 시 자동 해소되도록)
- [ ] `CMakeLists.txt`에 등록
- [ ] **게이트:** 빌드 통과 + `TEST(TypeCheckerTest, EmptyProgram)` (진단 0건), `TEST(TypeCheckerTest, BuiltinResolved)` (`출력("안녕")` 호출이 TC006 안 냄) 통과

### Task 5: CLI 플래그 + 진입점 통합

- [ ] `main.cpp`에 `--type-check=off|warn|strict` 파싱 추가
- [ ] `repl/repl.cpp`에 진입점 분기:
  - 파일 모드: `if (mode != off) tc.check(program); printDiag(stderr); if (mode==strict && hasErrors) return 1;`
  - REPL 모드: 시작 시 strict 받았으면 안내 1회 + warn 강등. TypeChecker 인스턴스는 **REPL 루프 외부에 단일 생성**, 매 줄 입력마다 `tc.check()` 호출 (globalTypes_/classTypes_ 누적)
- [ ] **게이트:**
  - `./hong-ik --type-check=warn examples/hello.hik` 정상 실행 (진단 0건)
  - `./hong-ik --type-check=off examples/hello.hik` 정상 실행
  - 모든 기존 ctest 통과 (회귀 없음)

### Task 6: TC001/TC002 + Z2 AST 노드 6종 처리

**핵심 — Z2 노드 없이는 평범한 사용자 코드(`각각`, `반복`, `시도` 등)에 TC006이 도배되어 Phase A가 무용지물.**

- [ ] `InitializationStatement` 처리 (TC001):
  1. `stmt->type` 토큰 + `stmt->isOptional`로 선언 Type 만들기
  2. `inferExpression(stmt->value)`로 값 Type 얻기
  3. `declared.isAssignableFrom(value)` 검사 → 실패 시 TC001 진단
  4. 검사 통과/실패 무관하게 스코프에 `declare(name, declared)`
- [ ] `AssignmentStatement` 처리 (TC002): `lookup(name)` 없으면 TC006(미리), 있으면 isAssignableFrom → 실패 시 TC002
- [ ] `inferExpression` 리터럴 (Task 6 시점):
  - IntegerLiteral → PrimType(INTEGER)
  - FloatLiteral → PrimType(FLOAT)
  - StringLiteral → PrimType(STRING)
  - BooleanLiteral → PrimType(BOOLEAN)
  - NullLiteral → PrimType(NULL_OBJ)
  - ArrayLiteral / HashMapLiteral / TupleLiteral → 원소 무시, PrimType(ARRAY/HASH_MAP/TUPLE) 평면 반환
- [ ] **Z2 AST 노드 6종 처리** (spec D6.5):
  - `ForEachStatement`: pushScope → declare(elementName, typeFromToken(elementType)) → inferExpression(iterable) → checkStatement(body) → popScope
  - `ForRangeStatement`: pushScope → declare(varName, typeFromToken(varType)) → inferExpression(startExpr/endExpr) → checkStatement(body) → popScope
  - `TryCatchStatement`: checkStatement(tryBody) → pushScope → declare(errorName, AnyType) → checkStatement(catchBody) → popScope
  - `MatchStatement`: inferExpression(subject) → 각 caseValue infer → 각 caseGuard infer(있으면) → 각 caseBody traverse → defaultBody traverse
  - `YieldStatement`: inferExpression(expression) → 결과 폐기
  - `IndexAssignmentStatement`: inferExpression(collection/index/value) → 타입 일치 검사 안 함
- [ ] 식별자 추론은 Task 7에서.
- [ ] 테스트:
  - `정수 x = "hello"` → TC001 1건
  - `정수 x = 10` → 0건
  - `정수? z = 없음` → 0건
  - `정수 y = 없음` → TC001 1건
  - `배열 a = []` → 0건
  - `배열 a = [1, "x"]` → 0건 (혼합 통과)
  - `각각 정수 원소 [1,2,3] 에서: 출력(원소)` → 0건 (TC006 없음)
  - `반복 정수 i = 0 부터 10 까지: 출력(i)` → 0건
  - `시도: 정수 x = 1 / 0 실패 오류: 출력(오류)` → 0건
  - `비교 x: 경우 1: 출력("일") 기본: 출력("기타")` → 0건

### Task 7: TC006 — 미선언 식별자 + 식별자 추론

- [ ] `inferExpression(IdentifierExpression)`: `lookup(name)` → 있으면 그 Type 반환, 없으면 TC006 진단 + `NeverType` 반환 (cascade 차단)
- [ ] `AssignmentStatement`에서 lookup 실패 시 TC006 (Task 6에서 보류했다면 여기서 마무리)
- [ ] 테스트:
  - `정수 x = 1; x = "a"` → TC002 1건
  - `정수 x = 1; x = 2` → 0건
  - `정수 x = foo` (foo 미선언) → TC006 1건
  - `미선언 + 1` (이항 연산자 한쪽 미선언) → TC006 1건. 결과는 NeverType (이후 BinaryExpression infer 시 추가 진단 없음 — spec 1.1.2)

### Task 8: TC101/TC102/TC103 — 함수 호출/리턴 (top-down)

**Hoisting 미지원 (spec D3.1). 재귀는 시그니처를 본문 진입 직전 declare로 처리.**

- [ ] `FunctionStatement` 처리 (spec 2.3 의사코드 그대로):
  1. funcType 생성 (params/ret을 typeFromToken으로 변환)
  2. `declare(stmt->name, funcType)` ← **본문 진입 직전에 declare하여 재귀 가능**
  3. pushScope, 매개변수 declare, currentReturnType_ 갱신
  4. body traverse
  5. currentReturnType_ 복원, popScope
- [ ] `ReturnStatement` 처리: `currentReturnType_` 대비 `inferExpression(value)` → 실패 시 TC103. `currentReturnType_`이 nullptr(글로벌 스코프)이면 무시
- [ ] `inferExpression(CallExpression)`:
  1. 호출 대상 Type lookup
  2. NeverType이면 NeverType 반환 (추가 진단 없음)
  3. AnyType이면 통과, 결과 AnyType
  4. BuiltinFunctionType이면: arity가 [minArity, maxArity] 밖이면 TC101. `skipArgTypeCheck=false`이면 인자 타입 검사 → 실패 시 TC102. 결과는 `ret`
  5. FunctionType이면: 인자 개수 검사(paramHasDefault 고려) → 실패 시 TC101. 각 인자 타입 vs params[i] 검사 → 실패 시 TC102. 결과는 `ret`
  6. ClassType이면: 인자 개수 vs `constructorArity` → 실패 시 TC101 (생성자 인자 개수). 인자 타입 검사 안 함. 결과는 AnyType (spec D6 #5 — Phase B에서 InstanceType 도입 시 정밀화)
  7. 그 외 → AnyType 통과
- [ ] 테스트:
  - `함수 f(정수 a) -> 정수: 리턴 a; f("x")` → TC102
  - `함수 f(정수 a) -> 정수: 리턴 a; f()` → TC101
  - `함수 f() -> 정수: 리턴 "a"` → TC103
  - `함수 f() -> 정수: 리턴 1; f()` → 0건
  - 재귀: `함수 사실() -> 정수: 리턴 사실()` → 0건 (본문 진입 전 declare 동작)
  - **Forward reference: `정수 x = 나중에(); 함수 나중에() -> 정수: 리턴 1` → TC006 1건** (의도된 동작 — evaluator도 런타임 실패)
  - 미선언 함수: `없는함수(1)` → TC006 1건 (TC101/TC102 추가 없음, Task 7에서 NeverType 반환했으므로)

### Task 9: TC501 — Nullable 미해제

- [ ] `inferExpression(MemberExpression)`: 좌항 Type이 OptionalType이면 TC501 진단 + inner type으로 진행
- [ ] `inferExpression(IndexExpression)`: 동일
- [ ] `inferExpression(BinaryExpression)`:
  - 한쪽이 NeverType → NeverType 반환 (spec 1.1.2)
  - op이 `==`/`!=`이면 TC501 발화 안 함, 결과 PrimType(BOOLEAN)
  - 그 외 op에서 좌/우항 중 OptionalType이 있으면 TC501. 결과는 cascade 규칙 적용
- [ ] 좁히기 미구현: `만약 x != 없음 라면: x + 1` → TC501 (Phase B에서 좁히기 추가)
- [ ] 테스트:
  - `정수? x = 10; x + 1` → TC501 1건
  - `정수? x = 10; x == 없음` → 0건 (== 예외)
  - `클래스 점: 정수 x; 생성(정수 x): 자기.x = x; 점? p = 없음; p.x` → TC501 1건 (멤버 접근 좌항 Optional)
  - `정수? x = 10; 정수 y = x` → TC001 1건 (Optional → Non-null 거부, TC501 아님 — 대입 검사가 먼저)

### Task 10: 클래스 처리 (spec D6)

- [ ] `ClassStatement` 발견 시:
  1. 이름을 `ClassType(name, constructorArity = constructorParams.size())`로 만들어 `classTypes_`와 현재 스코프에 등록
  2. **본문은 traverse하지 않음** — 메서드/필드 선언은 모두 skip
  3. 부모(`parentName`)가 있고 classTypes_에 없으면 TC006 진단
- [ ] `CallExpression`에서 호출 대상이 ClassType인 분기 (Task 8 step 6에서 이미 처리)
- [ ] `MemberExpression`에서 좌항이 ClassType 인스턴스(=AnyType)인 경우는 AnyType의 멤버 접근 → AnyType
- [ ] `자기` 키워드는 Phase A에서 AnyType 처리 — 메서드 본문 미진입이라 도달 안 함, 다만 typeFromToken에서 안전한 fallback 보장
- [ ] 상속(`클래스 강아지 < 동물:`): parent 이름이 classTypes_에 있는지 확인만 (없으면 TC006), 서브타이핑은 ClassType 단순 이름 일치
- [ ] 테스트:
  - `클래스 동물: ... 동물 a = 동물("뽀삐", 3)` → 0건
  - `클래스 동물: ... 동물 a = 동물("뽀삐")` (인자 부족) → TC101 1건
  - 메서드 본문 내 `리턴 자기.이름 + 1` → 0건 (본문 미진입)
  - `클래스 강아지 < 모름:` → TC006 1건 (부모 미선언)

### Task 11: 진단 출력 + REPL 안내

- [ ] `printDiagnostics(const vector<TypeDiagnostic>&, ostream&)` 헬퍼 — `repl/repl.cpp` 또는 `analyzer/type_checker.cpp`
- [ ] REPL 시작 시 strict 받았으면 안내 1회: `[알림] REPL에서는 strict 모드가 warn으로 동작합니다.`
- [ ] **게이트:** `./hong-ik --type-check=warn` REPL에서 `정수 x = "a"` 입력 시 진단 1줄 표시 후 프롬프트 복귀

### Task 12: 종합 테스트 + 골든 케이스

- [ ] TC001/TC002/TC006/TC101/TC102/TC103/TC501 각 코드별 positive(통과) + negative(진단) 최소 2개씩 — 총 14건 이상
- [ ] Z2 AST 노드 6종 각각에 대해 통과 케이스 1건 (TC006 미발화 확인)
- [ ] Hoisting 미지원 회귀 케이스: forward reference → TC006 1건
- [ ] 재귀 케이스: 함수가 본문에서 자기 자신 호출 → 0건
- [ ] 회귀 케이스: 기존 `.hik` 예제(`examples/` 또는 tests 내장)에 대해 `--type-check=warn` 실행 시 진단 0건. **단, forward reference를 사용하는 예제가 있으면 별도 분류** — 0건 통과가 안 되면 hoisting 도입 재고 또는 예제 수정.
- [ ] **게이트:** `ctest --test-dir cmake-build-debug` 전체 통과

### Task 13: 문서 및 CHANGELOG

- [ ] `CHANGELOG.md`에 "Phase A: static type checker (warning mode) 진입" 항목 추가, TC001~TC501 7개 진단 코드 나열
- [ ] `README.md`에 한 줄: `--type-check=warn` 플래그 사용법 (선택)
- [ ] **게이트:** 문서만 변경, 빌드/테스트 영향 없음

---

## 완료 기준 (Definition of Done)

1. `ctest --test-dir cmake-build-debug` 전체 통과 (기존 + 신규)
2. `./hong-ik --type-check=warn` 으로 기존 모든 예제 실행 시 진단 0건 (forward reference 사용 예제 제외, 또는 의도된 TC006)
3. 7개 TC 코드 모두 positive/negative 케이스 테스트 존재
4. Z2 AST 노드 6종 각각 통과 케이스 존재 (TC006 미발화)
5. CLI: `off`/`warn`/`strict` 3개 모드 모두 동작, REPL strict 강등 안내 출력
6. 빌트인 45개 시그니처 모두 prepopulate (`반복`은 dead builtin이라 제외) (`출력`, `길이`, `매핑` 등 단일 호출에 TC006 안 발화), `built_in.h`와 1:1 매칭 확인
7. 함수 검사 top-down 동작: 재귀 OK, forward reference는 TC006
8. 클래스 처리 (D6) 동작 — 본문 미진입, 생성자 arity만 검사
9. CHANGELOG 갱신
10. AST/Object(builtin_signatures.h 제외)/Environment/evaluator/vm/parser 파일 변경 없음 (`git diff --stat`로 확인)
11. Task 0 결과(승격 규칙·빌트인 arity)를 본 plan과 spec에 박제

---

## 비목표 재확인 (작업 중 유혹 차단)

작업 중 다음을 **건드리면 안 된다**. 다른 spec/plan에서 다룬다:

- 클래스 메서드 본문 검사 (Phase B)
- 클래스 필드 접근의 정확한 타입 (Phase B)
- foreach/forrange 원소·범위 타입 검사 (Phase B)
- 이항 연산자(`+`, `-` 등)의 **일반 타입 호환성** (Phase B — Optional 검사만 Phase A)
- 비교 연산자(`==`/`!=`) 좌우항 비교 가능성 (Phase B 또는 Phase F)
- 컬렉션 원소 타입 (`배열<정수>`) (Phase E)
- TupleType 원소화 (Phase E)
- 패턴 매칭(`비교/경우`)의 망라성, RangePattern/TypePattern 정밀 검사 (Phase F)
- 좁히기(`만약 x != 없음 라면:` 안에서 Optional 해제) (Phase B)
- 타입 추론 (`var x = ...`) (Phase G)
- 런타임 검사 제거 (Phase D)
- 사용자 정의 예외 클래스 (별도 spec)
- 트레이트·제네릭 (Phase F/E)
- **함수 hoisting / forward reference** (D3.1 — 의도된 미지원)

이 항목 중 하나라도 손대고 싶어지면 **plan을 멈추고** 별도 spec 작성을 제안하는 게 맞다.

---

## Task 의존 그래프

```
Task 0 (검증) ──┐
                ├─→ Task 1 (Type 계층) ── Task 2 (Type 테스트)
                │            │
                └─→ Task 3 (빌트인 시그니처) ── Task 4 (TypeChecker 골격) ── Task 5 (CLI 통합)
                                                                                   ↓
                                                       ┌──── Task 6 (TC001/TC002 + Z2 노드 6종) ─────┐
                                                       │                                              │
                                                       ├──── Task 7 (TC006 + 식별자 추론) ───────────┤
                                                       │                                              │
                                                       └──── Task 8 (TC101/102/103 + top-down) ──────┤
                                                                                                      ↓
                                                                                          Task 9 (TC501)
                                                                                                      ↓
                                                                                         Task 10 (클래스)
                                                                                                      ↓
                                                                                         Task 11 (출력)
                                                                                                      ↓
                                                                                         Task 12 (종합 테스트)
                                                                                                      ↓
                                                                                         Task 13 (문서)
```

Task 6 / Task 7은 독립적 (Task 5 후 둘 다 시작 가능). Task 8은 Task 6/Task 7 의존. Task 9는 Task 6 의존 (BinaryExpression infer에서 OptionalType 인식). Task 10은 Task 8 의존 (CallExpression의 ClassType 분기).
