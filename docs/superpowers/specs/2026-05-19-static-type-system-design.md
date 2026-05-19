# 정적 타입 시스템 도입 설계 (Static Type System Design)

Date: 2026-05-19
Revision: v3 (2차 셀프 리뷰 + evaluator 동작 검증 결과 반영 — hoisting 거부, AST 노드 6종 처리 추가, 빌트인 실측, paramOptionals 잉여 제거 등)

## 배경

`2026-04-01-type-system-design.md`은 hong-ik의 타입 표기(annotation)에 **런타임 검사**를 도입했고, 그 결과는 다음 형태로 완료되어 있다.

- `Environment::typeMap` + `SetType/GetType/HasType` (`environment/environment.h:17,35-37`)
- `Environment::optionalVars` + `IsOptional` (`environment/environment.h:16,27-29`)
- `CompiledFunction::paramTypeChecks` / `returnTypeCheck` / Optional 플래그 (`vm/chunk.h:25-28`)
- Optional 문법 (`정수?`, `클래스명?`)의 lexer→parser→evaluator→vm 통합 (`parser/parser.cpp:101-482`)

타입 오류는 `evaluator/evaluator.cpp:207`, `:218`, `:1118`, `:1126`, `vm/vm.cpp:619`, `:623` 등 **실행 중**에 `RuntimeException` 형태로 발생한다.

본 spec은 이 자리에서 시작하여, hong-ik을 "타입 표기형 동적 언어"에서 **"타입형(statically typed) 언어"** 로 격상한다. 그 결과로 사용자는 코드를 실행하지 않고도 타입 오류를 받게 되며, 외부적으로 "Rust/Kotlin/TypeScript 계열의 진짜 타입 언어"로 인식 가능해진다.

## 목표 (Goal)

1. 파서 직후, 평가/컴파일 직전에 **TypeChecker 패스**를 신설하여 타입 오류를 정적으로 검출한다.
2. 타입을 토큰이 아닌 **1급 객체 모델(`Type` 계층)** 로 표현한다.
3. 런타임 검사는 **일단 보존**한다. 정적 검사가 안정화되는 Phase C 이후 단계적으로 제거하여 성능을 회수한다.
4. 도입은 **점진적**으로 한다: 처음엔 경고(warning), 다음에 옵트인 에러(`--strict`), 마지막에 기본 에러로 승격.

## 범위 밖 (Non-Goals)

다음 항목은 본 spec에서 다루지 않으며, 후속 spec으로 분리한다.

- **제네릭 / 파라미터화 컬렉션** (`배열<정수>`, `사전<문자, 정수>`) — Phase E.
- **트레이트 / 인터페이스 / 추상 클래스** (`규약`) — Phase F.
- **합집합 타입** (`정수 | 문자`) — Nullable(`정수?`)만 지원.
- **타입 추론** (`var x = 10`) — 본 spec 범위에서는 모든 타입 표기를 명시 요구. 추론은 Phase G 별도 검토.
- **소유권/빌림 검사기** — 도입하지 않는다 (한글 교육 언어 정체성 보존).
- **사용자 정의 예외 클래스** — 별도 spec에서 다룬다 (예외 시스템 격상).
- **이항 연산자의 좌/우항 타입 호환성** — Phase B. 단 Optional 미해제 검사는 TC501의 일부로 Phase A에서 처리.
- **비교 연산자(`==`/`!=`)의 좌/우항 비교 가능성** — Phase A에서는 무조건 통과.
- **클래스 메서드/필드 접근 분석** — Phase B. Phase A는 클래스 본문을 skip한다 (D6 참조).
- **컬렉션 원소 타입 검사** — Phase E.
- **함수 hoisting / forward reference** — evaluator가 top-down이라 TypeChecker도 top-down (D3.1 참조). 재귀만 가능.
- **MatchStatement의 분기 망라성, RangePattern/TypePattern의 정확성** — Phase F.

---

## 핵심 설계 결정

### D1. 검사기 위치

`analyzer/` 디렉토리에 `TypeChecker` 클래스를 추가한다. 현재 `analyzer/`는 신택스 하이라이팅용 토큰 정보 추출만 담당(`analyzer/analyzer.h:27`)인데, 같은 디렉토리에 정적 분석 패스 일반의 거점을 두는 것이 자연스럽다.

```
파서 결과(AST) → analyzer::TypeChecker (신설) → evaluator 또는 vm::compiler
```

**장기 분리 가능성:** Phase B 이후 다른 정적 분석(unused var, dead code, exhaustiveness)이 추가될 경우 `analyzer/highlight/` + `analyzer/types/` 같은 하위 분리로 갈 수 있다. Phase A는 단일 파일로 유지.

### D2. Type 1급 객체 모델

기존: `Function::parameterTypes`가 `std::vector<std::shared_ptr<Token>>` (`object/object.h:113`). 토큰을 그대로 들고 다님 → Optional·함수타입·빌트인 다형성·향후 제네릭을 표현 불가.

신규: `util/type.h`에 `Type` 계층 도입.

```cpp
class Type {
public:
    virtual ~Type() = default;
    virtual bool isAssignableFrom(const Type& other) const = 0;
    virtual std::string toKorean() const = 0;
    virtual bool equals(const Type& other) const = 0;
};

class PrimType : public Type {       // 정수/실수/문자/논리/배열/사전/튜플/없음
    ObjectType kind;
};

class OptionalType : public Type {    // T?
    std::shared_ptr<Type> inner;
};

class FunctionType : public Type {    // 사용자 정의 함수, 단일 시그니처
    std::vector<std::shared_ptr<Type>> params;  // 각 param 타입 (Optional은 OptionalType으로 래핑)
    std::shared_ptr<Type> ret;
    std::vector<bool> paramHasDefault;          // 기본값 보유 여부 (호출 인자 개수 검사용)
};

class BuiltinFunctionType : public Type {  // 빌트인 다형성 표현 — D2.1 참조
    std::string name;
    int minArity;
    int maxArity;                    // 가변일 경우 INT_MAX
    bool skipArgTypeCheck;           // true면 인자 타입 검증 생략 (다형성 빌트인)
    std::shared_ptr<Type> ret;       // 반환 타입 (다형이면 AnyType)
};

class ClassType : public Type {
    std::string name;
    int constructorArity;            // 생성자 인자 개수 (Phase A는 이거만 사용)
    std::shared_ptr<ClassDef> def;   // Phase A 미사용, Phase B에서 활용
};

class AnyType : public Type { };     // 검사 불가/escape hatch
class NeverType : public Type { };   // 분석 실패 후 전파 차단용
```

**잉여 필드 제거 (Z8):** v2의 `paramOptionals`는 잉여 — 매개변수가 `정수?`이면 params[i]가 `OptionalType(PrimType(INTEGER))`로 자체 표현됨. 별도 플래그 불필요.

#### D2.1 빌트인 다형성 처리 (C1 보강)

hong-ik 빌트인 46개 중 다수는 다형 — `길이`는 문자/배열/사전 모두 받고, `포함`은 컬렉션·원소 모두 다형. 단일 `FunctionType`으로는 표현 불가.

**Phase A 접근:** `BuiltinFunctionType.skipArgTypeCheck = true`로 두어 인자 타입 검증을 생략하고 **인자 개수만 검사**한다. 반환 타입이 입력에 의존하면 `AnyType`으로 둔다. 입력 독립이면 정확한 타입 사용.

**가변 인자 빌트인 (Task 0 실측 결과, 2026-05-19):**

| 빌트인 | minArity | maxArity | 출처 |
|---|---|---|---|
| `출력` | 0 | `INT_MAX` (가변) | `object/builtins/io.cpp:13-54` — for 루프, size 0도 통과 |
| `입력` | 0 | 1 | `object/builtins/io.cpp:56-79` — `size > 1` 거부 |

그 외 44개는 모두 `parameters.size() != N` 형식의 고정 arity. 전체 매핑은 plan Task 3의 시그니처 테이블 참조.

**확정 이슈 — Dead builtin `반복`:** 빌트인 `반복`(Repeat, `string.cpp:123`)이 키워드 `반복`(while/for-range, `token_type.h:58`, `lexer.cpp:19`)과 동명. Lexer는 `반복`을 항상 키워드 토큰으로 분류하고, parser는 statement 시작 위치에서만 키워드로 처리(`parser.cpp:189`). 표현식 위치(`출력(반복("ab", 3))`)에서는 식별자 fallback이 없어 **사용자 텍스트에서 호출 불가능**. `evaluator.cpp:62`와 `vm.cpp:59`의 등록은 닿지 않는 코드, 테스트만 AST 직접 생성으로 우회. README의 `반복("ab", 3)` 표기는 잘못된 문서. **Phase A 결정: 시그니처 테이블에서 제외 (45개로 축소).** hong-ik이 키워드 충돌을 해소하면 그때 추가.

그 외 빌트인은 모두 고정 arity. 전체 46개 시그니처는 plan Task 3에서 `object/builtin_signatures.h`(신규)로 작성하되, **각 항목 옆에 출처 파일·줄 명시** (예: `정렬: 1인자 ← array.cpp:84` 형식).

**Phase A에서 모든 다형 빌트인은 `skipArgTypeCheck=true`로 시작.** Phase B에서 `BuiltinFunctionType`을 다중 오버로드(`vector<FunctionType>`) 지원하도록 확장.

#### D2.2 TupleType — Phase A 범위 결정

Phase A에서 **튜플은 `PrimType(TUPLE)`로 평면 처리**한다. 원소 타입 검사는 하지 않는다.

근거: spec D2의 Type 계층에 TupleType을 추가하면 원소 타입 추론·인덱스 접근 시 타입 결정 등 부가 작업이 따라옴. 이는 Phase E(컬렉션 원소 타입)와 묶어야 일관성 있음. Phase A는 "튜플이다/아니다"까지만 본다.

빈 배열 `[]`, 혼합 배열 `[1, "a"]`도 동일한 평면 처리. 원소 타입 무시.

#### D2.3 runtime ObjectType 의존

`PrimType.kind`가 `ObjectType` enum 사용 → 정적 분석 모델이 런타임 enum에 결합. **Phase A 의도된 결합.** Phase D(런타임 검사 제거) 이후 ObjectType의 실행 제어용 값(BREAK_SIGNAL, RETURN_VALUE 등)을 분리하면서 독립 `TypeKind` enum 도입을 검토 (Future considerations).

`Token` 기반 표기는 **TypeChecker 내부에서만 `Type`으로 변환**해 사용한다. AST 노드 정의(`ast/`)는 본 spec에서 건드리지 않는다 (기존 동작 호환성).

### D3. 런타임 검사 보존 + 단계적 제거

본 spec 완료 시점에는 다음 두 검사가 **동시에** 동작한다.

- TypeChecker (정적, 신규)
- evaluator/vm의 RuntimeException 기반 검사 (기존)

이중 검사는 의도된 안전망이다. 정적 검사가 통과시킨 케이스만 런타임에 도달하므로, 둘이 충돌할 경우 정적 검사기에 버그가 있다는 명백한 신호가 된다. **단, warn 모드에서는 정적 진단이 흘러도 실행이 계속되어 런타임 검사가 다시 발화 가능하다 — 사용자에게 동일 오류가 두 줄로 보일 수 있음.** 진단 코드 prefix(`type-warning[TCxxx]` vs 런타임의 `[줄 N]`)로 구분되도록 한다. 안정화 후 Phase D에서 런타임 검사를 제거한다.

#### D3.1 Hoisting / Forward reference 금지 (Z1)

`evaluator/evaluator.cpp:446-457`을 통해 확인된 바, evaluator는 `FunctionStatement`를 만나는 순간에만 `environment->Set(name, function)`. 즉 **top-down 평가, hoisting 없음.** 다음 패턴은 런타임 실패:

```
정수 x = 나중에()     // 런타임 에러: 나중에 미선언
함수 나중에() -> 정수:
    리턴 1
```

TypeChecker도 동일하게 **top-down 검사.** 재귀는 별도 처리: `FunctionStatement` 만나면 **본문 진입 직전에 시그니처를 declare** 하여 본문에서 자기 자신 참조 가능. Forward reference는 의도된 TC006.

이 정책은 D3의 "정적 통과 → 런타임 통과" 안전망을 지키기 위한 것. v2에 있던 hoisting pre-pass 안은 폐기.

### D4. 점진적 도입 모드

`--type-check=` CLI 플래그로 모드 선택:

| 모드 | 기본 동작 | Phase |
|---|---|---|
| `off` | TypeChecker 호출하지 않음 | (현재 동작 유지용) |
| `warn` | TypeChecker 실행, 오류를 stderr 경고로만 출력, 실행 계속 | **Phase A 기본값** |
| `strict` | TypeChecker 실행, 오류 시 실행 중단(종료 코드 1) | Phase B 이후 |

Phase A에서는 `strict`도 CLI에서 받지만, REPL에서는 `warn`으로 강등하고 **첫 줄에 한 번 안내**: `[알림] REPL에서는 strict 모드가 warn으로 동작합니다.`

Phase C 진입 조건은 D7에 명시.

### D5. 위치 정보

TypeChecker 진단(diagnostic)은 다음 필드를 포함한다:

```cpp
struct TypeDiagnostic {
    long long line;
    long long column;     // 가능하면, 없으면 0
    Severity severity;    // WARNING | ERROR
    std::string code;     // "TC001"~ — 안정적 ID, D8 카테고리 참조
    std::string message;  // 한글 메시지
};
```

기존 `RuntimeException`의 `[줄 N]` 포맷과 동일한 줄 표기를 사용해 출력 정렬 유지.

### D6. 클래스 선언 처리 (Phase A 한정)

Phase A는 클래스 본문 내부의 타입 분석을 **수행하지 않는다.** 단:

1. `ClassStatement` 발견 시 이름을 `ClassType`(name, constructorArity)로 현재 스코프와 `classTypes_`에 등록.
2. **생성자 호출**(`동물("뽀삐", 3)`)의 **인자 개수**는 `ClassType.constructorArity`로 검사 (TC101). 인자 타입은 검사하지 않음.
3. **메서드 본문**은 traverse하지 않음 — 내부의 모든 변수/리턴/호출에 대해 진단 발생 0건.
4. **`자기.속성` / `인스턴스.속성` / `인스턴스.메서드()`** 접근은 모두 `AnyType`로 추론. 진단 발생 0건.
5. **상속 관계의 타입 호환성** (`동물 a = 강아지(...)`): 생성자 호출 결과는 AnyType이므로 `declared(ClassType).isAssignableFrom(AnyType) = true`로 자동 통과. 정확한 서브타이핑은 Phase B.
6. **상속 부모 미선언:** `클래스 강아지 < 모름:`에서 `모름`이 classTypes_에 없으면 TC006.

이 단순화는 Phase A의 범위를 "기본 자료형의 변수·함수 호출"에 묶기 위한 의도적 결정. Phase B에서 클래스 분석을 본격 도입.

### D6.5 그 외 AST 노드 처리 정책 (Z2)

evaluator/ast 분석 결과, 다음 AST 노드들은 Phase A에서 다음 정책으로 처리한다. 미처리 시 변수 미선언 진단(TC006)이 평범한 사용자 코드 전반에 도배되므로 필수.

| AST 노드 | 출처 | Phase A 처리 |
|---|---|---|
| `ForEachStatement` (`각각 정수 원소 [..] 에서:`) | `ast/statements.h:130`, `evaluator.cpp:351` | 새 스코프 push → `elementName`을 `elementType`으로 declare → iterable inferExpression → body traverse → 스코프 pop |
| `ForRangeStatement` (`반복 정수 i = 0 부터 N 까지:`) | `ast/statements.h:212`, `evaluator.cpp:402` | 새 스코프 push → `varName`을 `varType`(보통 정수)으로 declare → startExpr/endExpr inferExpression(타입 일치 검사 안 함, 런타임에서 잡음) → body traverse → 스코프 pop |
| `TryCatchStatement` (`시도/실패 오류:`) | `ast/statements.h:144` | tryBody traverse → 새 스코프 push → `errorName`을 `AnyType`으로 declare → catchBody traverse → 스코프 pop |
| `MatchStatement` (`비교/경우/기본`) | `ast/statements.h:188` | subject inferExpression → 각 caseValue/caseGuard inferExpression → 각 caseBody traverse → defaultBody traverse. 분기 망라성은 검사하지 않음(Phase F) |
| `YieldStatement` (`생산 x`) | `ast/statements.h:248` | expression inferExpression → 결과 무시. 제너레이터 흐름 분석은 Phase B |
| `IndexAssignmentStatement` (`a[i] = v`) | `ast/statements.h:257` | collection/index/value 모두 inferExpression → 타입 일치 검사 안 함 (컬렉션 원소 타입은 Phase E) |

위 6종은 모두 **본문 traverse + 변수 declare 정책**이라 구조적으로 단순. Task 6에서 일괄 구현.

### D7. Phase C 진입 조건 (strict 기본값 전환)

Phase C는 다음 세 조건이 모두 충족될 때 진입:

1. 기존 `examples/`(또는 동등 위치)의 모든 `.hik` 예제가 `--type-check=warn`에서 **진단 0건** 으로 통과.
2. Phase A·B 누적 후 **14일 이상** warn 모드로 dogfooding (실사용 또는 내부 테스트).
3. CHANGELOG에 마이그레이션 노트 작성 (사용자 코드에서 발생 가능한 패턴과 우회법).

조건 충족 판단은 프로젝트 운영자가 수행. 조건 부족 시 Phase C 보류.

### D8. 진단 코드 카테고리

확장 대비 카테고리 분리. Phase A는 TC0xx~TC5xx 범위 중 7개만 사용.

| 범위 | 카테고리 | Phase A 사용? |
|---|---|---|
| TC0xx | 변수/식별자 | ✓ (TC001, TC002, TC006) |
| TC1xx | 함수/호출/리턴 | ✓ (TC101, TC102, TC103) |
| TC2xx | 클래스/메서드/필드 | (Phase B) |
| TC3xx | 컬렉션/원소 | (Phase E) |
| TC4xx | 패턴 매칭/망라성 | (Phase F) |
| TC5xx | 옵셔널/널 안전성 | ✓ (TC501) |
| TC9xx | 일반/구조 | — |

Phase A 진단 코드 (D8 적용):

| 코드 | 검사 |
|---|---|
| TC001 | 변수 선언 타입 불일치 |
| TC002 | 변수 재대입 타입 불일치 |
| TC006 | 미선언 식별자 |
| TC101 | 함수/클래스 생성자 호출 인자 개수 |
| TC102 | 함수 호출 인자 타입 불일치 |
| TC103 | 함수 리턴 타입 불일치 |
| TC501 | Nullable 미해제 사용 |

---

## 1. Type 모델 구체

### 1.1 isAssignableFrom 규칙

```
T  <- T         : true (동일 타입)
T? <- T         : true (Non-null → Optional 좁히기)
T? <- 없음       : true
T  <- 없음       : false (Non-null은 null 거부)
T? <- T?        : true
any <- anything : true
anything <- any : true (검사 통과)
never <- *      : true (단일 방향, 분석 실패 전파)
* <- never      : true (다른 분석으로부터의 신호 흡수)
```

**`실수 <- 정수` 자동 승격: 거부 (Phase A).** Task 0 실측 결과(2026-05-19 검증):
- Tree-walker(`HongIk.exe tmp.hik`): **거부** — "선언에서 자료형과 값의 타입이 일치하지 않습니다"
- VM(`HongIk.exe --vm tmp.hik`): **통과** — 1 출력
- evaluator와 VM 사이 런타임 동작 불일치가 존재. Phase A는 D3 안전망 원칙(정적 통과 → 모든 런타임 통과)을 지키기 위해 **엄격한 쪽(tree-walker) 따라 거부**. 사용자는 `실수변환()` 명시 변환 필요.
- VM의 관대한 동작은 별도 이슈(런타임 일관성 spec)로 분리 — 본 spec 범위 밖.

#### 1.1.1 NullLiteral과 Optional 대입 의사코드 (Z10)

`OptionalType.isAssignableFrom(other)`의 구현:
```
if other is PrimType with kind == NULL_OBJ:
    return true
if other is OptionalType:
    return inner.isAssignableFrom(other.inner) || other.inner is NULL_OBJ
return inner.isAssignableFrom(other)
```

`PrimType.isAssignableFrom(other)`:
```
if other is NeverType: return true
if other is AnyType: return true
if other is PrimType with same kind: return true
// NULL_OBJ는 OptionalType에서만 허용 (Non-null PrimType은 거부)
return false
```

#### 1.1.2 NeverType cascade 규칙 (Z11)

이항 연산자나 함수 호출의 결과 타입 추론 시 한쪽이 NeverType이면 **결과도 NeverType**. 추가 진단 차단:

```
inferBinary(left, op, right):
    if left.type == Never or right.type == Never:
        return Never
    // 그 외 Phase A는 BinaryOp 일반 타입 검사 안 함 → 단순화 결과 추론만
    if op in [==, !=]: return PrimType(BOOLEAN)
    if both are PrimType(INTEGER): return PrimType(INTEGER)
    if both/either are PrimType(FLOAT) or PrimType(INTEGER): return PrimType(FLOAT)
    if op == + and either is PrimType(STRING): return PrimType(STRING)
    return AnyType
```

### 1.2 함수 타입 동등성

함수 타입은 **공변(covariant)** 리턴, **반공변(contravariant)** 매개변수 — 단, Phase A에서는 단순화하여 **모든 위치 invariant**로 시작한다. 함수가 일급 값으로 다뤄지는 곳(고차함수 인자)에서만 의미가 있는데, hong-ik에서는 아직 그 사용이 제한적이라 invariant도 충분.

### 1.3 BuiltinFunctionType 호출 규칙

호출 시:
1. 인자 개수가 `[minArity, maxArity]` 범위 밖이면 TC101.
2. `skipArgTypeCheck == false`이면 일반 `FunctionType`처럼 인자 타입 검사 (TC102).
3. `skipArgTypeCheck == true`이면 인자 타입 검사 생략.
4. 결과 타입은 `ret`.

---

## 2. TypeChecker 패스

### 2.1 클래스 골격

```cpp
// analyzer/type_checker.h
class TypeChecker {
public:
    struct Result {
        std::vector<TypeDiagnostic> diagnostics;
        bool hasErrors() const;
    };

    TypeChecker();   // 생성자에서 빌트인 시그니처 prepopulate (D2.1)

    // 한 Program AST를 검사. 글로벌/클래스 스코프는 누적 보존됨 (REPL 다중 호출 대응).
    Result check(std::shared_ptr<Program> program);

    // REPL에서 세션 종료 시 호출, 진단만 비움 (스코프는 유지).
    void clearDiagnostics();

private:
    struct Scope {
        std::map<std::string, std::shared_ptr<Type>> vars;
    };
    std::vector<Scope> scopes_;                        // 함수/블록 스코프 push/pop
    std::map<std::string, std::shared_ptr<Type>> globalTypes_;  // 누적 보존
    std::map<std::string, std::shared_ptr<ClassType>> classTypes_;  // 누적 보존
    std::vector<TypeDiagnostic> diagnostics_;
    std::shared_ptr<Type> currentReturnType_;

    void checkStatement(std::shared_ptr<Statement> stmt);
    std::shared_ptr<Type> inferExpression(std::shared_ptr<Expression> expr);

    void pushScope();
    void popScope();
    void declare(const std::string& name, std::shared_ptr<Type> type);
    std::shared_ptr<Type> lookup(const std::string& name);

    void error(long long line, const std::string& code, const std::string& msg);
    void warn(long long line, const std::string& code, const std::string& msg);

    std::shared_ptr<Type> typeFromToken(const std::shared_ptr<Token>& tok, bool optional);
    void registerBuiltins();  // 생성자에서 호출
};
```

**Phase A에서 검사 실패 시 변수 타입 처리:** 실패한 선언은 **declared(표기) 타입을 그대로 사용**하여 스코프에 등록. NeverType 사용은 cascade 진단을 잃을 위험 있어 채택하지 않음. 같은 변수에 대한 후속 진단은 dedup하지 않는다 (각 줄에 분리된 사용자 의도 가정).

**REPL 함수 재선언 (Z13):** 사용자가 같은 이름의 함수를 REPL에서 두 번 입력하면 globalTypes_에서 **덮어쓰기 허용, 별도 안내 없음**. evaluator도 Environment::Set이 덮어쓰기이므로 동일.

### 2.2 진입점 통합

`repl/repl.cpp`의 파일/REPL 모드에서, 파서 호출 직후·평가 직전에:

```cpp
auto program = parser.parse(tokens);
if (typeCheckMode != Mode::Off) {
    auto result = typeChecker.check(program);  // 세션 단일 인스턴스 (REPL)
    printDiagnostics(result.diagnostics);
    if (typeCheckMode == Mode::Strict && result.hasErrors()) {
        return ExitCode::TypeError;
    }
}
evaluator.eval(program);
```

VM 경로(`--vm`)도 동일한 위치에 삽입.

**REPL 상태 관리:** TypeChecker 인스턴스는 REPL 세션 동안 **단 하나** 만든다. 매 입력 후 `check()`를 호출하면 globalTypes_와 classTypes_는 누적되어 후속 입력에서 그대로 보임. 함수/블록 스코프만 push/pop. 파일 모드는 어차피 한 번만 check하므로 동일.

### 2.3 함수 검사 흐름 (top-down, 재귀 허용)

```cpp
checkFunctionStatement(stmt):
    funcType = buildFunctionTypeFromStmt(stmt)
    // 본문 진입 직전 declare → 본문에서 자기 자신 참조 가능 (재귀 OK)
    declare(stmt->name, funcType)

    pushScope()
    for (i = 0; i < stmt->parameters.size(); i++):
        declare(stmt->parameters[i]->name, funcType->params[i])
    prevReturnType = currentReturnType_
    currentReturnType_ = funcType->ret
    traverse(stmt->body)
    currentReturnType_ = prevReturnType
    popScope()
```

Hoisting 없음. Forward reference는 declare 전이라 lookup 실패 → TC006.

---

## 3. Phase A에서 검사하는 항목

본 spec의 Phase A 구현 범위. 나머지 항목은 Phase B 이후로 분리.

| # | 검사 | AST 노드 | 진단 코드 |
|---|---|---|---|
| 1 | 변수 선언: 표기 타입 vs 초기값 타입 | `InitializationStatement` | TC001 |
| 2 | 변수 재대입: 기존 타입 vs 새 값 타입 | `AssignmentStatement` | TC002 |
| 3 | 함수/생성자 호출: 인자 개수 | `CallExpression` | TC101 |
| 4 | 함수 호출: 인자 타입 vs 매개변수 타입 | `CallExpression` | TC102 |
| 5 | 함수 리턴: 선언 리턴 타입 vs 리턴 값 타입 | `ReturnStatement` | TC103 |
| 6 | 미선언 식별자 사용 | `IdentifierExpression` | TC006 |
| 7 | Nullable 미해제 사용 | `MemberExpression`, `IndexExpression`, 이항 연산자 좌/우항 | TC501 |

**TC501 범위 명확화:** Optional 타입 값이 다음 위치에 직접 등장하면 진단.
- 멤버 접근 좌항: `x.속성` (x가 T?)
- 인덱스 접근 좌항: `x[i]` (x가 T?)
- 이항 연산자 좌/우항: `x + 1` (x가 T?) — Phase A에서 이항 연산자의 **일반 타입 호환성은 검사하지 않으나**, Optional 미해제 검사만은 TC501로 다룬다.
- **단, `==`/`!=` 비교는 예외** (null 체크 패턴 `x == 없음` 자주 사용).

좁히기(narrowing) 미구현: `만약 x != 없음 라면:` 안에서도 여전히 OptionalType으로 본다. 사용자 우회: `정수 y = x` (이건 TC001로 잡힘) 또는 명시 변환. Phase B에서 좁히기 추가.

검사 안 하는 항목(향후 Phase):
- 클래스 메서드/필드 접근 타입 (Phase B — D6 참조)
- foreach/forrange 원소·범위 타입 (Phase B — 런타임에서 잡힘)
- 이항 연산자의 **일반 타입 호환성** (Phase B — Optional 검사만 Phase A)
- 컬렉션 원소 타입 (Phase E와 묶음)
- 패턴 매칭(`비교/경우`)의 분기 망라성 (Phase F)
- 함수 hoisting / forward reference (D3.1 — 의도된 미지원)

---

## 4. 출력 형식

```
type-warning[TC001] [줄 5] '정수' 타입 변수 'x'에 '문자' 값을 대입할 수 없습니다.
type-warning[TC102] [줄 12] 함수 '더하기'의 매개변수 'a'는 '정수' 타입이지만 '문자' 값이 전달되었습니다.
type-error[TC103] [줄 18] 함수 '계산'의 반환 타입 '정수'에 '실수' 값을 반환할 수 없습니다.
```

REPL은 진단 메시지 후 프롬프트 복귀. 파일 모드는 `warn`이면 계속, `strict`면 종료.

stderr로 출력. stdout(프로그램 출력)과 섞이지 않도록.

---

## 5. 구현 순서 (Phase A 한정)

| 단계 | 결과물 | 의존 |
|---|---|---|
| A0 | evaluator 사전 검증 (plan Task 0): hoisting=top-down 재확인, `실수 <- 정수` 동작, 빌트인 46개 arity 실측 | 없음 |
| A1 | `util/type.h`, `util/type.cpp` — Type 계층, isAssignableFrom (A0 결과 반영) | A0 |
| A2 | `object/builtin_signatures.h` — 빌트인 46개 시그니처 테이블 (A0 실측 인용) | A1 |
| A3 | `analyzer/type_checker.h`, `.cpp` 골격 + 빌트인 prepopulate | A1, A2 |
| A4 | CLI 플래그 `--type-check=` + 진입점 통합 + REPL 안내 | A3 |
| A5 | 검사 TC001/TC002 (변수 선언/대입) + Z2의 AST 노드 6종 처리 | A3 |
| A6 | 검사 TC006 + 식별자 추론 | A3 |
| A7 | 검사 TC101/TC102/TC103 (함수 호출/리턴) — top-down, 재귀 허용 | A5, A6 |
| A8 | 검사 TC501 (Nullable 미해제) | A5, A6 |
| A9 | 클래스 처리 (D6) | A7 |
| A10 | `tests/type_checker_test.cpp` 종합 케이스 | A5+ |

A5/A6/A8은 독립적이라 병렬 가능. A7은 A5/A6 의존.

---

## 6. Phase 로드맵 (참고)

본 spec은 Phase A까지만 구현. 이후는 별도 spec/plan으로 분리.

| Phase | 내용 | 비고 |
|---|---|---|
| **A** | 정적 검사기 골격 + 기본 검사 (본 spec) | warn 모드 |
| B | 검사 항목 확장 (메서드, foreach/forrange 원소, 이항 연산자 일반, Optional 좁히기) | warn 모드 |
| C | `--strict` 기본값 전환 | D7 진입 조건 충족 시 |
| D | 런타임 검사 제거, 성능 회수 | benchmark 비교 |
| E | 제네릭 컬렉션 (`배열<T>`) + TupleType 원소화 | 문법 확장 |
| F | 트레이트(`규약`) / 인터페이스 + 패턴 망라성 | OOP 모델 재정렬 |
| G | (선택) 타입 추론 (`var x = ...`) | 표기 의무 완화 |

---

## 7. 변경 파일 요약 (Phase A)

| 파일 | 변경 |
|---|---|
| `util/type.h` (신규) | Type 계층 선언 |
| `util/type.cpp` (신규) | isAssignableFrom 외 구현 |
| `object/builtin_signatures.h` (신규) | 빌트인 46개 시그니처 테이블 (D2.1 + 실측 인용) |
| `analyzer/type_checker.h` (신규) | TypeChecker 인터페이스 |
| `analyzer/type_checker.cpp` (신규) | 검사 로직 + 빌트인 prepopulate |
| `repl/repl.cpp` | 파서 후 TypeChecker 호출 분기, REPL 단일 인스턴스 |
| `main.cpp` | `--type-check=` CLI 파싱 |
| `CMakeLists.txt` | 신규 파일 등록 |
| `tests/type_checker_test.cpp` (신규) | 진단 코드별 케이스 |
| `tests/CMakeLists.txt` | 테스트 등록 (필요 시) |
| `CHANGELOG.md` | Phase A 진입 기록 |

기존 evaluator/vm/object(이외 builtin_signatures.h 제외)/ast/token/parser/environment는 **변경 없음**. 호환성 우려를 최소화한다.

---

## 8. 위험 및 완화

| 위험 | 영향 | 완화 |
|---|---|---|
| TypeChecker 버그로 정상 코드를 false positive | warn 모드라 실행은 계속 | Phase A는 warn 고정, 출력만 분석 |
| 런타임 검사와의 메시지 형식 차이로 사용자 혼란 | 중 | `[줄 N]` 포맷 통일, `type-warning[TCxxx]` prefix로 정적/런타임 구분 |
| 빌트인 다형성 표현 불충분 | 사용자가 잘못된 인자 줘도 진단 안 됨 | Phase A는 명시적 양보. Phase B에서 다중 오버로드 도입 |
| Type 모델 도입이 evaluator/vm에 침투 | 회귀 위험 | Phase A에선 TypeChecker 내부 전용. AST/runtime은 손대지 않음 |
| `정수→실수` 동작 spec과 evaluator 불일치 | 사용자 혼란 | plan Task 0에서 사전 검증 |
| 클래스 본문 skip으로 OOP 코드의 타입 안전성 0 | 중 | Phase A 명시적 양보. Phase B에서 클래스 본격 분석 |
| AST 노드 처리 누락으로 평범한 코드에 TC006 도배 | **고** (Phase A 사용 불가) | D6.5에 6종 노드 처리 정책 박제, Task 6에서 일괄 구현 |
| Phase C에서 strict 기본값 전환 시 사용자 코드 깨짐 | 중 | D7 진입 조건 충족 필수, CHANGELOG·README 안내 |
| REPL 단일 인스턴스가 글로벌 누적 → 메모리 누수 | 저 | 세션 종료 시 자연 소멸. 장시간 REPL에서 비정상 누적 시 `:reset` 명령 같은 escape는 향후 추가 |
