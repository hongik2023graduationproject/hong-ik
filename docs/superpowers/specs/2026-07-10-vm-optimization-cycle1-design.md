# VM 최적화 사이클 1 — 빌트인·컬렉션 경계 축소 + Peephole 패스

- 날짜: 2026-07-10
- 상태: 승인 (사용자 자율 진행 위임 — 결정은 아래 적대적 검토 근거와 함께 기록)
- 근거 데이터: `docs/benchmarks/2026-07-08-baseline.md` (VM/CPython 최악 지점: builtin_calls 16.8x, array_ops 15.1x, hashmap_ops 13.4x)
- 선행: 벤치마크 스위트 v1 (e287422). 효과는 `compare.py`로 베이스라인 JSON 대비 증명한다.

## 범위

베이스라인 문서의 최적화 후보 ①(빌트인 경계 변환 축소)과 ②(Layer 2 peephole)를 한 사이클로 실행한다.

**이번 사이클에서 하지 않는 것** (후보로만 기록):
- 전역 변수 인라인 캐시 — arith_loop의 지배 비용은 `OP_GET_GLOBAL`/`OP_SET_GLOBAL`의 매 접근 `unordered_map<string>` 해시 조회임을 탐색에서 확인했다. 효과가 크겠지만 별도 사이클로 분리 (rebind·erase 불변식 검토 필요).
- 디스패치 루프 타이트닝 (frame/ip 로컬 캐싱) — 전 핸들러 침습, 별도 사이클.
- 컬렉션 원소의 VMValue 내부 표현 — 베이스라인 후보 ③, Layer 3급 침습, 별도 스펙.

## 결정 사항

### D1. Builtin::function 시그니처: by-value → const 참조

현재 `virtual shared_ptr<Object> function(vector<shared_ptr<Object>> args)`가 값 전달이라, VM이 인자 버퍼를 재사용해도 **호출 시 vector 복사가 일어나 이득이 사라진다**. `const vector<shared_ptr<Object>>&`로 변경한다.

- 영향: `object/object.h`, `object/built_in.h`, `object/builtins/*.cpp` 7파일의 오버라이드 ~45개 + 호출부 4곳(evaluator 2, VM 2). 기계적 변경.
- 적대적 검토: diff가 넓다는 게 유일한 반론. virtual 오버라이드는 시그니처 불일치 시 추상 클래스 인스턴스화가 컴파일 에러로 터지므로 누락이 정적으로 검출된다. 대안(값 전달 유지 + move)은 버퍼 capacity를 매번 빼앗겨 재사용이 무의미. 기각 사유 명확.
- 부수 이득: 트리워킹 evaluator의 빌트인 호출 복사도 함께 제거된다.

### D2. VM 빌트인 호출 경로 정비 (OP_CALL / OP_INVOKE)

- `dynamic_cast<Builtin*>` 2회 → 1회 (결과 재사용).
- 인자 버퍼를 VM 멤버(`builtinArgs_`)로 재사용 — D1과 결합해 정상 상태에서 힙 할당 0. 빌트인은 VM으로 재진입하지 않는 leaf 함수이므로 재진입 안전 (스펙에 불변식으로 명시, 위반 시 assert).
- 인자 pop 루프 → `stack.resize()` 한 번.

### D3. `길이` 네이티브 fast path

builtin_calls(16.8x)의 핵심. VM 생성자에서 `builtins["길이"]` 포인터를 캐시하고, OP_CALL에서 callee가 그 포인터이고 argCount==1이며 인자가 String/Array/HashMap이면 VMValue::Int를 직접 push — 인자 vector·결과 Integer 힙 할당·std::function 디스패치 전부 생략.

- **폴스루 원칙**: 위 조건이 하나라도 안 맞으면 (튜플, 잘못된 타입, 인자 수 오류 등) 기존 제네릭 경로로 넘긴다. 에러 메시지·엣지 시맨틱이 자동으로 registry와 동일해진다.
- String 길이는 `utf8::codePointCount` 대신 `str->codePoints().size()`를 쓰지 않는다 — registry 구현(`collection.cpp:16`)과 동일하게 `codePointCount(str->value)` 사용. codePoints() 캐시를 새로 만드는 부작용을 피하고 등가성을 소스 수준에서 유지.
- 적대적 검토: VM에 빌트인 시맨틱이 중복되는 divergence 위험 → 등가성 테스트(fast path 결과 == `Length::function` 결과, 타입별)로 고정하고 fast path 목록은 길이 1개로 최소화. `추가` 등 추가 후보는 D1/D2의 제네릭 개선으로 충분하다고 판단 (YAGNI).

### D4. OP_INDEX_GET / OP_INDEX_SET의 불필요한 toObject 제거

현재 `opIndexGet`은 정수 인덱스를 `toObject()`로 **매 접근마다 Integer를 힙 할당**한 뒤 dynamic_cast로 되읽는다 (array_ops 15.1x, hashmap_ops 13.4x, string_index 10.1x의 주요 비용). VMValue 태그를 직접 검사하도록 재작성한다:

- 인덱스: `index.isInt()` → `asInt()` 직접 사용 (할당 0).
- 컬렉션: `collection.isObject()` 확인 후 raw 포인터 dynamic_cast (toObject 불필요 — OBJECT 태그면 어차피 포인터 반환이지만 스칼라였다면 할당이 일어났음).
- 에러 메시지·음수 인덱스·범위 검사 시맨틱은 바이트 단위로 동일 유지. opIndexSet의 value는 컬렉션에 Object로 저장되므로 `toObject()` 유지 (이 할당 제거는 후보 ③의 몫).

### D5. Peephole 패스 — 명령 재조립 방식

`vm/peephole.{h,cpp}` 신설. 파이프라인: **decode(전체 opcode 피연산자 테이블) → 점프 타깃 절대주소 수집 → 패턴 융합(타깃 가드) → 재조립(점프 오프셋·lines 재매핑)**.

- 융합 패턴 (모두 동형): `OP_SET_LOCAL/OP_SET_GLOBAL/OP_SET_UPVALUE/OP_SET_MEMBER + OP_POP` → 신설 `*_POP` 4종. 대입문마다 1디스패치+1스택왕복 제거 — 모든 시나리오의 루프 본문에 등장.
- **가드**: 뒤 명령(OP_POP)이 점프 타깃이면 융합하지 않는다. 점프 종류: OP_JUMP/OP_JUMP_IF_FALSE/OP_ITER_NEXT(전방), OP_LOOP(후방), OP_TRY_BEGIN(catchOffset). 모두 uint16 상대 — 축소만 일어나므로 오버플로 불가.
- OP_CLOSURE는 가변 길이(uint16 + count + 3바이트×count) — decode 테이블에 명시 처리.
- 재귀 적용: 중첩 CompiledFunction 상수, CompiledClassDef의 생성자·메서드에도 적용.
- 적대적 검토 — 대안 비교:
  - (a) 동일 길이 NOP 치환: 재매핑 불필요하지만 NOP 디스패치가 이득을 잠식, 인프라 가치 없음. 기각.
  - (b) emit 시점 융합(뒤돌아보기): 재조립 불필요하나 "이 POP이 미래 patchJump의 타깃인가"를 emit 시점에 증명하기 어렵다 — 안전성이 암묵적. 기각.
  - (c) 채택안: 점프 타깃 집합으로 가드가 명시적·증명 가능, decode 인프라는 이후 패턴 추가(점프 스레딩 등)와 디스어셈블러 재료로 재사용.
- 컴파일 시간 증가는 frontend/compile 벤치(0.9ms)로 감시 — LSP 실시간성에 여유 큼.
- 정리: 미사용 opcode `OP_GET_BUILTIN`(emit도 핸들도 없음)은 decode 테이블 작성 시 함께 제거.

### D6. WASM/이식성 제약

`vm/peephole.cpp`는 `HONGIK_CORE_SOURCES`에 추가되어 **Emscripten C++20으로도 컴파일**되어야 한다 (C++26 전용 기능 금지). MSVC `/utf-8` 순서 이슈 재발 방지: 새 타깃은 없지만, CMakeLists 변경 시 MSVC 재현 빌드 1회를 리뷰 게이트에 포함 (49bcff5 교훈).

### D7. 검증·측정 (DoD)

1. 전체 테스트 green (기존 549 + 신규: peephole 단위 테스트, 길이 등가성, 융합 가드 엣지 — if/while/break/continue/try/foreach 안 대입문의 VM·Evaluator 결과 일치).
2. 골든 테스트·백엔드 parity 무변경.
3. `compare.py` 재측정: builtin_calls·array_ops·hashmap_ops 유의미 개선(정량 목표는 사전 약속하지 않고 측정으로 보고), **어느 시나리오도 중앙값 +5% 초과 회귀 없음**.
4. WASM CI green + 로컬 MSVC 재현 빌드 1회.
5. 재측정 결과를 `docs/benchmarks/2026-07-10-cycle1.md`로 문서화 (before/after 표 + compare.py 출력).

## 예상 효과 (정직한 사전 추정 — 약속 아님)

- builtin_calls: fast path로 큰 폭 개선 기대 (루프 제어 비중 ~절반이므로 시나리오 전체로는 상한 ~2x).
- array_ops/hashmap_ops/string_index: 인덱스 접근당 힙 할당 1회 제거 — 중간 폭.
- arith_loop/fib: SET+POP 융합만 해당 — 소폭. arith_loop의 지배 비용(전역 해시 조회)은 다음 사이클.
