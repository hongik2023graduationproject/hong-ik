# hong-ik 벤치마크 스위트 v1 설계

Date: 2026-07-08

> **진행 방식 노트:** 사용자가 성능 벤치마크 트랙까지 자율 진행을 위임함 ("너가 체크해보고 성능 벤치마크까지 자율적으로 진행해"). 설계 승인 게이트는 생략하되, 통상 브레인스토밍에서 물었을 결정들을 아래 "결정과 근거"에 대안·트레이드오프와 함께 기록한다. **범위는 벤치마크 스위트 + 베이스라인 측정 + 최적화 후보 보고까지** — VM 최적화 실행(Layer 2 잔여 등)은 수치를 보고 사용자와 재논의.

**Goal:** 재현 가능한 성능 측정 인프라를 만들어 ① VM vs 트리워킹 비교 수치(쇼케이스 서사), ② 최적화 전후 비교 기반(회귀 추적), ③ 다음 최적화 후보의 데이터 근거를 확보한다.

**배경:** VM 최적화는 Layer 1(싱글턴/정수 풀/UTF-8 캐시/O(n²) 수정, 85ee666)과 Layer 3(VMValue tagged union, bffca6f)이 완료됐고, Layer 2는 상수 폴딩(산술·비교·모듈로·거듭제곱)까지 구현, peephole 패스는 placeholder다. 그러나 측정 도구는 `tests/benchmark.cpp`(HongIkBench) 수준 — 시나리오 3개, 3회 산술평균, 워밍업·통계·결과 영속화 없음. ms 단위 소규모 워크로드라 타이머 노이즈에 취약하고, "최적화가 실제로 효과 있었는가"를 신뢰 있게 답할 수 없다.

---

## 결정과 근거

### D1. 하네스: Google Benchmark (FetchContent, native 전용)

- **대안 A — 자체 하네스 개선**: 의존성 0이지만 중앙값/이상치/반복 결정/JSON 출력/전후 비교를 전부 재발명해야 함.
- **대안 B — Google Benchmark (채택)**: 반복 횟수 자동 결정, `--benchmark_format=json`, `--benchmark_filter`, repetitions+집계(중앙값/CV), 그리고 **`compare.py`로 두 JSON의 통계적 비교(p-value)** — "Layer 2 적용 전후 비교"라는 이 트랙의 핵심 사용례를 공짜로 얻는다. C++ 업계 표준이라 쇼케이스 신호도 양호.
- 적대적 검토: end-to-end 스크립트 실행(수십 ms 단위) 측정에 GBench가 과한가? — 과하지 않음. GBench는 iteration당 비용이 큰 벤치도 자동 반복 산정으로 처리하며, 수동 하네스로 같은 신뢰도를 얻는 비용이 더 크다. 의존성 리스크는 googletest·nlohmann과 동일 패턴(FetchContent, native 분기 한정)으로 관리.

### D2. 시나리오 소스: `benchmarks/scenarios/*.hik` 실제 파일

- C++ 문자열 임베드 대비 데모 가치(실제 hong-ik 스크립트)와 Python 등가물 병치가 가능. 경로는 CMake compile definition으로 소스 디렉터리를 베이크하고, 파일 로드 실패 시 즉시 명확한 에러로 중단.
- 파일명·벤치마크 이름은 **ASCII**(예: `arith_loop.hik`) — Windows cp949 콘솔 출력 사고 방지. 스크립트 내용은 한글.

### D3. 측정 대상 축

1. **실행 백엔드 비교**: 시나리오별 VM(기본)과 트리워킹(`--eval` 경로) 모두 측정 — 속도 비율이 쇼케이스 핵심 수치.
2. **프론트엔드**: 대형 소스의 렉스+파스, 컴파일 시간 별도 그룹 — LSP 반응성과 직결.
3. **CPython 3.14 대비**: 등가 `.py`를 `benchmarks/reference/`에 두고 **수동 1회 측정해 결과 문서에 기록**. 스위트에는 미포함(환경 의존 → flaky). 교육용 언어가 CPython보다 느린 건 당연하므로 정직하게 기록하고, 서사는 "VM이 트리워킹 대비 N배"에 둔다.

### D4. 시나리오 목록 (각각 런타임 특성 축 하나씩)

| 이름 | 내용 | 측정 축 |
|---|---|---|
| `arith_loop` | 정수 산술 루프 (100만 회 누적) | 기본 연산·지역변수 |
| `fib_recursive` | 재귀 피보나치 | 함수 호출·스택 프레임 |
| `string_concat` | 문자열 누적 결합 | 문자열 할당 |
| `string_index` | 한글 문자열 반복 인덱싱 | UTF-8 코드포인트 캐시 |
| `array_ops` | 배열 추가·인덱스 갱신 루프 | 컬렉션 |
| `hashmap_ops` | 사전 삽입·조회 루프 | 해시 |
| `class_method` | 인스턴스 생성 + 메서드 호출 루프 | OOP 디스패치 |
| `builtin_calls` | 내장 함수 반복 호출 | 빌트인 경계 변환 |
| `frontend_parse` | 대형 소스 렉스+파스 | 프론트엔드 |
| `frontend_compile` | 대형 소스 컴파일 | 프론트엔드 |

워크로드 크기는 구현 중 캘리브레이션(iteration당 대략 10ms+ 목표, 세부는 GBench 자동 반복에 위임). 재귀 깊이·컬렉션 크기는 트리워킹에서도 완주 가능한 수준으로.

### D5. 정확성 가드

런타임 시나리오 8종의 기대 결과값을 `HongIkTests`에 스모크 테스트로 추가(VM·트리워킹 모두 — 프론트엔드 2종은 실행하지 않으므로 제외). 벤치마크 시나리오가 조용히 잘못된 값을 계산하면서 빨라지는 회귀를 차단한다. 이것이 이 트랙의 TDD 앵커 — 벤치 수치 자체는 테스트 대상이 아니다.

### D6. 기존 HongIkBench 대체

`tests/benchmark.cpp`와 `HongIkBench` 타깃은 삭제하고 새 `HongIkBenchmarks`로 대체. 두 벤치 시스템 병존은 혼란만 낳는다(히스토리는 git에 남음).

### D7. CI

벤치 타깃은 CI에서 **빌드만** 수행(컴파일 회귀 방지, build.yml의 기존 `cmake --build`에 자연 포함). 타이밍 실행·게이팅은 CI 러너 노이즈 때문에 제외 — 수치는 로컬 측정으로 문서화. **주의: 새 CMake 타깃은 반드시 MSVC `/utf-8` 플래그(add_compile_options) 이후에 정의** — 49bcff5 회귀의 교훈.

---

## 컴포넌트

| 경로 | 역할 |
|---|---|
| `benchmarks/scenarios/*.hik` | 벤치 시나리오 스크립트 (ASCII 파일명, 한글 내용) |
| `benchmarks/reference/*.py` | CPython 등가 스크립트 (수동 비교용) |
| `benchmarks/main.cpp` | GBench 엔트리 — 시나리오 로드, VM/Eval 벤치 등록 |
| `benchmarks/runner.{h,cpp}` | 시나리오 파일 로드 + 파이프라인 구동 (repl 파이프라인 준용: Utf8Converter → Lexer → appendMissingBlockClosers → Parser → Compiler/VM 또는 Evaluator) |
| `tests/benchmark_scenarios_test.cpp` | 시나리오별 기대 결과값 스모크 (VM + Eval) |
| `docs/benchmarks/2026-07-08-baseline.md` | 베이스라인 수치 + CPython 대비 + 분석 |
| CMakeLists.txt | benchmark FetchContent + `HongIkBenchmarks` 타깃 (native 분기, /utf-8 이후) |

**runner 인터페이스** (벤치·테스트 공용):

```cpp
// benchmarks/runner.h
struct ScenarioProgram { std::shared_ptr<Program> program; };
std::string loadScenarioSource(const std::string& name);        // 원문 로드 (프론트엔드 벤치 setup용), 실패 시 throw
ScenarioProgram loadScenario(const std::string& name);          // 원문 로드+파스, 실패 시 throw
std::shared_ptr<Object> runVm(const ScenarioProgram&);          // 매 호출 새 Compiler/VM
std::shared_ptr<Object> runEval(const ScenarioProgram&);        // 매 호출 새 Evaluator
```

파싱은 벤치 측정 밖(setup)에서 1회, 실행만 측정. 프론트엔드 벤치는 반대로 소스 텍스트를 setup으로 두고 렉스+파스(+컴파일)를 측정.

## 에러 처리

- 시나리오 파일 부재/파스 에러 → 벤치 등록 시점에 stderr 메시지 + 해당 벤치 skip이 아니라 **즉시 실패**(잘못된 수치 방지).
- 실행 결과가 스모크 기대값과 다르면 테스트 실패 — 벤치 바이너리는 결과 검증하지 않음(측정 오염 방지).

## 테스트 전략

- TDD 대상: runner(loadScenario/runVm/runEval)의 동작, 시나리오 기대값 스모크. GBench 등록부(main.cpp)는 선언적 코드라 테스트 제외.
- 기존 536 테스트 무회귀. WASM 분기 무영향(전부 native 분기).

## v1 범위 제외 (YAGNI)

CI 타이밍 게이팅 · 벤치 결과 대시보드 · 메모리 프로파일링 · 멀티 파일 워크로드 · JS/Lua 등 추가 언어 비교 · perf/vtune 연동. 필요해지면 v2.

## 산출물 (Definition of Done)

1. `HongIkBenchmarks` 타깃 빌드·실행 — 런타임 시나리오 8종 × (VM/Eval) + 프론트엔드 2종(parse/compile).
2. 스모크 테스트 포함 전체 테스트 green.
3. `docs/benchmarks/2026-07-08-baseline.md` — 수치 표, VM/트리워킹 비율, CPython 대비, 관찰된 병목과 다음 최적화 후보 목록.
4. CI green (벤치 타깃 빌드 포함).
