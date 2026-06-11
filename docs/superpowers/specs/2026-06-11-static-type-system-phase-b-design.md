# 정적 타입 시스템 Phase B 설계

Revision: v1 (2026-06-11)

**선행 spec:** `2026-05-19-static-type-system-design.md` (Phase A, 구현 완료 — 커밋 a7f4199..db9bdd5)

**Goal:** Phase A 검사기를 두 방향으로 확장한다. ① 표현식 정밀화 — AST 줄 번호, 이항 연산자 검사(TC601), Optional 좁히기, foreach/forrange 원소·범위 검사. ② 클래스 본문 분석 — InstanceType 도입, 필드/메서드 검사(TC201). warn 모드 유지, strict 기본화는 Phase C.

**구현 구성: spec 1개(본 문서) + plan 2개.**
- **plan B-1 (표현식 정밀화):** D1~D4. 기존 타입 모델 내 확장.
- **plan B-2 (클래스 본문):** D5~D6. 타입 모델 확장(InstanceType) 포함.

B-1과 B-2는 독립적으로 머지 가능하며 B-1을 먼저 한다 (B-2의 진단 메시지가 D1 줄 번호의 수혜를 받음).

---

## 핵심 원칙 (Phase A에서 승계)

1. **안전망 원칙:** 정적 검사 통과 → 모든 런타임 통과. 역으로 **두 런타임(evaluator·VM)이 모두 거부하는 경우에만 진단**한다. 한쪽만 거부하는 불일치 조합은 진단하지 않고 "런타임 일관성 이슈 목록"에 박제한다.
2. **기존 예제 진단 0건:** 골든 픽스처·README 예제가 `--type-check=warn`에서 새 진단을 내면 안 된다 (의도적 에러 픽스처 제외).
3. **검사 실패 시에도 선언(표기) 타입으로 진행.** dedup 없음.

## 호환성 가드 (Phase A에서 완화)

| 영역 | Phase A | Phase B |
|---|---|---|
| `ast/` | 읽기만 | **`Node`에 `line` 필드 추가 허용 (D1)** — additive only |
| `parser/` | 읽기만 | **노드 생성 시 `line` 채우는 변경 허용 (D1)** — 동작 변경 없음 |
| evaluator/vm/environment/object(시그니처 테이블 제외) | 읽기만 | **변동 없음 — 여전히 읽기만** |

evaluator/VM의 기존 테스트 + 골든 픽스처 전체 통과가 매 task의 게이트.

---

## D1. AST 줄 번호 (plan B-1)

**문제:** `IdentifierExpression`, `CallExpression`, `AssignmentStatement` 등 다수 노드에 토큰/줄 정보가 없어 진단이 `[줄 0]`으로 출력된다 (Phase A 실측: `error_undefined.hik`). evaluator도 같은 한계를 `current_line` 폴백으로 우회 중.

**결정:**
- `ast/node.h`의 `Node`에 `long long line = 0;` 추가.
- parser의 각 노드 생성 지점에서 해당 노드의 대표 토큰(첫 토큰) line을 채운다.
- TypeChecker의 `nodeLine()` 헬퍼는 `node->line`을 우선 사용하고, 0이면 기존 dynamic_cast 체인 폴백 유지.
- evaluator/VM은 **변경하지 않는다** (기존 `current_line` 방식 유지 — 별도 정리 기회에 흡수).

**게이트:** `정수 x = 미선언변수` 류 진단이 정확한 줄 번호로 출력. 파서/평가기/VM 기존 테스트 전체 통과.

## D2. 이항 연산자 검사 — TC6xx 신설 (plan B-1)

D8 진단 카테고리에 **TC6xx (연산자)** 를 추가한다.

| 코드 | 검사 |
|---|---|
| TC601 | 이항 연산자 피연산자 타입 비호환 (두 런타임 모두 거부하는 조합) |

- **실측 완료 (2026-06-11, 부록 A):** 16 연산자 × 7×7 타입 전수 실행, 거부/결과 규칙을 부록 A.1/A.2에 박제. 한쪽만 거부하는 94 조합은 진단하지 않고 부록 B에 기록.
- **결과 타입 정밀 추론:** 부록 A.2 규칙 그대로. 선행 spec 1.1.2 초안 대비 보정점: `&&`/`||` 혼합은 Any (VM이 우항 값을 그대로 반환 — 부록 B #4), 문자쌍 비교는 Any.
- Optional 피연산자의 TC501은 기존대로 (TC601과 중복 발화하지 않음 — Optional이면 TC501만).
- NeverType cascade 규칙 유지: 한쪽 Never → 결과 Never, 추가 진단 없음.
- PrefixExpression(`!x`, `-x`)도 같은 원칙으로 결과 타입 추론 (`!` → 논리, `-` → 피연산자 타입). 단항 비호환 진단은 비포함 (가치 대비 실측 비용).

**게이트:** `정수 x = 1 + 2` 통과(결과 정수), `문자 s = 1 + 2` TC001, 두-런타임-거부 조합 TC601, 기존 예제 0건.

## D3. Optional 좁히기 (plan B-1)

**범위:** 분기-한정(branch-scoped) 좁히기만.

- `만약 <cond> 라면:`의 `<cond>`가 다음 패턴일 때:
  - `x != 없음` (또는 `없음 != x`) → **then 분기에서 x를 inner 타입으로 좁힘**
  - `x == 없음` (또는 `없음 == x`) → **else(아니면) 분기에서 좁힘**
  - `<a> && <b>` → 양쪽을 재귀 분석해 then 분기에서 둘 다 좁힘 (`||`는 미지원)
  - x는 단순 IdentifierExpression만 (멤버/인덱스 접근은 미지원)
- **구현:** 분기 진입 시 `pushScope()` + 좁힌 타입으로 shadow declare + **narrowed 마커**. 분기 종료 시 pop.
  - **재대입 해제:** `AssignmentStatement`가 narrowed 마커가 있는 항목에 닿으면 ① 원(Optional) 타입 기준으로 TC002 검사 ② shadow 항목을 원 타입으로 되돌림(좁힘 해제). `x = 없음`이 분기 안에서 오탐을 내지 않기 위함.
- **사전 검증 결과 (2026-06-11 실측 완료):** if-블록 스코프가 백엔드 간 **정반대** — evaluator는 블록 선언이 바깥으로 누수(통과), VM은 블록 스코프(거부). → 부록 B 이슈 #2. 누수 허용(evaluator) 시맨틱을 깨지 않기 위해 **narrow 오버레이 맵으로 확정**: 분기 진입 시 스코프를 push하지 않고, 좁힘 항목만 오버레이 스택에 기록한다. `lookup()`은 오버레이를 우선 조회하고, 분기 내 신규 선언은 기존 스코프 규칙 그대로 따른다.
- **비포함:** early-return 패턴(`만약 x == 없음 라면: 리턴` 이후 좁힘), `||`, 멤버 접근 좁히기, while 조건 좁히기.

**게이트:** `만약 x != 없음 라면:` 내부 `x + 1` 진단 0건 (Phase A 테스트 `TC501_NoNarrowingInPhaseA` 갱신), 분기 내 `x = 없음` 후 `x + 1` TC501 재발화, 분기 밖은 여전히 TC501.

## D4. forrange 범위 검사 (plan B-1, TC3xx 선사용) — 실측 반영 v1.1

D8의 TC3xx(컬렉션/원소)를 Phase B에서 1개 선사용한다. (카테고리 자체는 Phase E 소유)

| 코드 | 검사 |
|---|---|
| TC301 | forrange의 start/end 타입이 {문자, 논리, 없음, 배열, 사전} (두 런타임 모두 거부 — 2026-06-11 실측) |

- **실수 경계는 진단하지 않는다**: `반복 정수 i = 0.5 부터 2 까지:`를 VM은 0.5 스텝으로 순회, evaluator는 거부 → 불일치 (부록 B #4).
- forrange varType 검사 없음: `반복 실수 k = 0 부터 2 까지:`는 양쪽 통과 (실측).
- **TC302(foreach 순회 불가) 폐기**: VM이 비순회형(정수)·사전 iterable을 **조용히 0회 순회로 통과**시키고 evaluator만 거부 → both-reject 원칙상 진단 불가. foreach 원소 타입(문자 iterable + 정수 원소)도 동일 불일치. 부록 B #5 — VM 패리티 버그 성격이라 런타임 일관성 spec의 우선 항목.

**게이트:** 기존 for_each/forrange 골든 0건, `반복 정수 i = "a" 부터 10 까지:` TC301, `0.5 부터` 진단 없음.

## D5. 클래스 본문 분석 — InstanceType + TC2xx (plan B-2)

**타입 모델 확장:**

```cpp
// util/type.h 추가
class InstanceType final : public Type {
public:
    std::string className;
    // isAssignableFrom: 같은 이름 또는 other가 자신의 자손 (부모 체인 추적은 TypeChecker의 클래스 정보 참조)
};
```

- `ClassType` = "클래스 그 자체" (호출 가능 대상, `classTypes_` 등록용) — 기존 유지.
- `InstanceType` = 인스턴스 값. **생성자 호출 결과와 클래스명 타입 표기(`점 p`)가 모두 InstanceType이 된다** (Phase A에서는 각각 Any/ClassType이었음 — typeFromToken과 inferCallExpression의 ClassType 분기 변경).
- 서브타이핑: `InstanceType(부모).isAssignableFrom(InstanceType(자식)) == true`. 부모 체인은 TypeChecker가 보유한 클래스 정보로 판단 — Type 계층은 체인 정보를 직접 들지 않고, 판단 콜백 또는 TypeChecker 측 헬퍼로 처리 (Type을 runtime 정보와 분리 유지). 구체 메커니즘은 plan에서 결정하되 Type 클래스에 classTypes_ 포인터를 넣지 않는 것을 원칙으로 한다.

**TypeChecker 측 클래스 정보 (classInfos_):**

```cpp
struct ClassInfo {
    std::string name;
    std::string parentName;                                  // 없으면 빈 문자열
    int constructorArity;
    std::map<std::string, std::shared_ptr<Type>> fields;     // 필드명 → 타입 (부모 병합은 lookup 시 체인)
    std::map<std::string, std::shared_ptr<FunctionType>> methods;
};
```

**검사 항목:**

| 코드 | 검사 |
|---|---|
| TC201 | 존재하지 않는 필드/메서드 접근 (`p.없는것`, `자기.없는것`) — 부모 체인까지 탐색 후 |

- `ClassStatement` 처리 확장: 필드 등록 → 생성자 본문 검사(파라미터 스코프 + `자기` = 자신의 InstanceType) → 각 메서드 본문 검사 (FunctionStatement 검사 재사용: TC101/TC102/TC103 그대로).
- `자기.x = a` (생성자/메서드 내): **선언된 필드**면 필드 타입 vs 값 타입 → TC002 재사용. **미선언 필드 쓰기는 동적 필드 등록으로 처리 (TC201 발화 금지)** — 실측(부록 C): 생성자·메서드 모두에서 동적 필드 추가가 양 백엔드에서 허용됨. 동적 필드의 타입은 첫 대입 값의 추론 타입.
- `p.x` / `p.합()` (MemberAccessExpression / MethodCallExpression): 좌항이 InstanceType이면 ClassInfo lookup → 필드 타입 반환 / 메서드 시그니처로 TC101/TC102 검사 + ret 반환. 미지 멤버 TC201. 좌항이 Any/Never면 기존대로 통과.
- `부모` 키워드: 부모 클래스의 InstanceType으로 처리. super-call(`부모(...)`) 문법은 언어에 없음(부록 C) — 호출 대상 처리 불필요.
- 메서드 내 재귀·상호 참조: 클래스의 필드/메서드 시그니처를 **본문 검사 전에 전부 등록**한 후 본문들을 검사한다 — 실측(부록 C)으로 확인 완료 (앞 메서드가 뒤 메서드 호출 가능, 양 백엔드 동일).
- 생성자 미정의 자식 클래스는 부모 생성자 arity 상속 (Phase A 오탐 핫픽스 bc723fb 반영 — ClassInfo도 동일 규칙).
- **비포함:** override 시그니처 검사, 필드 가시성(private 등 — 언어에 없음), 클래스 정적 멤버, 제네릭.

**구조 정리 (B-2에 포함):** `analyzer/type_checker.cpp`가 Phase B-2 완료 시 900줄+ 예상 → `type_checker.cpp`(스코프/진입점/문장) + `type_checker_expr.cpp`(표현식 추론) + `type_checker_class.cpp`(클래스)로 분리. 헤더는 단일 유지.

**게이트:** classes_basic 골든 0건 유지, `p.없는멤버` TC201, `자기.x = "문자"` (정수 필드) TC002, `동물 a = 강아지()` 통과(서브타이핑), 미지 멤버 접근이 Any 좌항에서는 침묵.

## D6. 진단 코드 요약 (Phase B 추가분)

| 코드 | 카테고리 | 검사 | plan |
|---|---|---|---|
| TC201 | 클래스 | 존재하지 않는 필드/메서드 | B-2 |
| TC301 | 컬렉션 | forrange 범위 타입 | B-1 |
| TC302 | 컬렉션 | foreach 순회 불가 타입 | B-1 |
| TC601 | 연산자 (신설 카테고리) | 이항 연산자 피연산자 비호환 | B-1 |

메시지 형식·출력 채널(stderr, `type-warning[TCxxx] [줄 N]`)은 Phase A와 동일.

## D7. 명시적 비포함 (Phase B 전체)

- 빌트인 다중 오버로드 (`skipArgTypeCheck=false` 경로 활성화) — B-3 후보
- 제너레이터 흐름 분석 (`생산` 함수의 반환 타입 의미론)
- early-return / `||` / 멤버 접근 좁히기
- override 시그니처 검사
- 컬렉션 원소 타입 (`배열<T>`, Phase E), 패턴 망라성 (Phase F), 타입 추론 (Phase G)
- strict 기본화 (Phase C — D7 진입 조건은 선행 spec 참조)
- 런타임(evaluator/VM) 동작 변경 일체 — `실수<-정수` 불일치 등 런타임 일관성 이슈는 별도 spec

## D8. 위험 및 완화

| 위험 | 영향 | 완화 |
|---|---|---|
| parser line 채우기 누락 노드 → 여전히 줄 0 | 저 (품질) | 진단 경로에 쓰이는 노드부터 우선 적용 + 폴백 유지 |
| 클래스 본문 검사가 기존 코드에 신규 진단 | 중 | 원칙 2 (예제 0건 게이트) + 두-런타임-거부 원칙 |
| 좁히기 scope 방식이 evaluator 시맨틱과 충돌 (선언 누수) | 중 | B-1 Task 0 실측 후 오버레이 방식으로 전환 가능하게 설계 |
| 이항 연산자 실측 매트릭스 불완전 (조합 수 ~16 op × 7×7) | 중 | 대표 조합 자동 생성 스크립트로 실측, 결과를 spec 부록에 박제 |
| InstanceType 도입으로 Phase A 테스트 의미 변화 (생성자 결과 Any → Instance) | 중 | Phase A 테스트는 결과 0건/코드 기준이라 대부분 무영향. ClassConstructorOk 등 영향 테스트는 의도 갱신으로 명시 |
| 동적 필드 추가(`자기.새필드`)가 언어 시맨틱상 허용일 가능성 | 중 | B-2 Task 0 실측 후 TC201 발화 위치 결정 (본 spec D5에 옵션 박제) |

---

## 부록 A. 이항 연산자 실측 결과 (2026-06-11, HongIk db9bdd5)

16개 연산자 × 7타입 × 7타입 = 784 조합을 try/catch 래핑 `.hik`으로 두 백엔드에서 전수 실행.
결과: **both-reject 604 / both-OK 86 / evaluator만 거부 94 / VM만 거부 0**. VM이 일관되게 더 관대하다.

### A.1 TC601 거부 규칙 (두 런타임 모두 거부하는 조합 — 진단 대상)

피연산자 타입은 정적 추론 결과 기준. **Optional → TC501 우선(TC601 미발화), Any/Never → 검사 생략.**

| 연산자 | 허용 (진단 안 함) | 거부 (TC601) |
|---|---|---|
| `+` | 숫자쌍(정·실 혼합 포함), 문자쌍 | 그 외 전부 |
| `-` `*` `/` `**` | 숫자쌍 | 그 외 전부 |
| `%` `&` `\|` | 정수쌍 | 그 외 전부 |
| `<` `>` `<=` `>=` | 숫자쌍 / **문자쌍은 진단 면제(불일치 #3)** | 그 외 전부 |
| `==` `!=` | 숫자쌍, 문자쌍, 논리쌍, **한쪽이 없음이면 항상** | 그 외 (예: `정수 == 문자`, `배열 == 배열`) |
| `&&` `\|\|` | **좌항이 논리면 우항 무검사** (단락 평가로 값 의존 — 불일치 #4) | 좌항이 논리 아님 |

### A.2 결과 타입 추론 규칙 (both-OK 86 조합 기준)

| 패턴 | 결과 |
|---|---|
| `정수 op 정수` (`+ - * / % ** & \|`) | 정수 |
| 숫자 혼합/실수쌍 (`+ - * / **`) | 실수 |
| `문자 + 문자` | 문자 |
| `< > <= >=` 숫자쌍 | 논리 |
| `== !=` | 논리 (항상 — Phase A 유지) |
| `논리 && 논리`, `논리 \|\| 논리` | 논리 |
| 그 외 (불일치·미허용 조합 포함) | Any |

NeverType cascade(한쪽 Never → Never, 진단 차단)는 모든 규칙에 우선.

## 부록 B. 런타임 일관성 이슈 목록 (2026-06-11 갱신)

> **✅ #1~#6 해소됨 (2026-06-12, `2026-06-12-runtime-consistency-design.md` 구현 완료)** — 이항 784조합·선언 42조합 재검증 불일치 0건. 잔여: **#7 (신규)** VM이 제너레이터 iterable을 침묵 0회 순회 (evaluator는 지원) — 별도 작업 필요.

1. `실수 b = 1` — evaluator 거부 / VM 통과 (Phase A Task 0 발견)
2. **if-블록 스코프 정반대** — evaluator는 블록 내 선언이 바깥으로 누수(통과), VM은 블록 스코프(거부). `만약 true 라면: 정수 x = 1` 후 `출력(x)`: eval=1, VM=식별자 에러.
3. **이항 연산자 94 조합** evaluator만 거부: 실수×비숫자 산술/비교 전반, `문자 < 문자`(문자열 비교 — VM 허용/eval 거부), `논리 && 비논리`(좌항 true일 때) 등. 전체 목록은 실측 로그 참조.
4. **`\|\|` 단락 평가 + 우항 값 반환** — `false \|\| 1`: VM은 `1`(정수!) 반환, evaluator는 거부. VM의 `\|\|`는 JS류 값 반환 시맨틱.
5. **foreach 비순회형 침묵 통과** — `각각 정수 n 5 에서:`/사전 iterable: VM은 조용히 0회 순회, evaluator는 거부. 문자 iterable의 원소 타입 표기도 VM은 무시. VM 패리티 버그 성격 — 우선 수정 권장.
6. **서브타입 대입 정반대** (2026-06-11 클래스 실측) — `동물 a = 강아지(...)`: VM 통과, evaluator는 선언 타입 검사에서 거부(클래스명 정확 일치만 인정). D5의 서브타이핑 설계는 VM(기본 백엔드) 시맨틱을 따른다.

## 부록 C. 클래스 시맨틱 실측 (2026-06-11, plan B-2 입력)

두 백엔드 동작이 일치한 항목 (이슈 #6 제외):

| 항목 | 결과 | D5 반영 |
|---|---|---|
| 미지 필드 읽기 `p.없는것` | **both reject** | TC201 발화 ✓ |
| 미지 메서드 호출 `p.없는것()` | **both reject** | TC201 발화 ✓ |
| 생성자/메서드에서 `자기.새필드 = ...` (선언 안 된 필드) | **both pass** (동적 필드 허용) | `자기.x` **쓰기에는 TC201 금지** — 동적 필드로 등록 |
| 메서드 상호 참조 (앞 메서드가 뒤 메서드 호출) | **both pass** | 시그니처 전체 등록 후 본문 검사 ✓ (D5 설계대로) |
| 생성자 미정의 자식 클래스 | 부모 생성자 상속 (`강아지("뽀삐", 3)` 동작) | **Phase A TC101 오탐 — 핫픽스 완료** (arity 부모 체인 상속) |
| super-call (`부모(...)`) | 문법 없음 (docs/inheritance.md — 자식 생성자가 부모 필드 직접 세팅) | `부모` 처리 단순화 |
