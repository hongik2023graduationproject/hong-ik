# 벤치마크 스위트

hong-ik 런타임(VM · 트리워킹)과 프론트엔드(렉스+파스 · 컴파일)의 성능을 Google Benchmark로 측정한다.

## 구성

| 경로 | 역할 |
|------|------|
| `scenarios/*.hik` | 측정 대상 시나리오 8종 (실제 소스 파일, 단일 진실의 원천) |
| `runner.{h,cpp}` | 시나리오 로드·파스·컴파일·실행 파이프라인 (테스트와 벤치가 공유) |
| `main.cpp` | Google Benchmark 등록·엔트리포인트 (`HongIkBenchmarks` 타깃) |
| `reference/*.py` | 각 시나리오와 1:1 구조 등가인 CPython 스크립트 (수동 비교용) |

등록되는 벤치마크:
- 시나리오 8종 × 2백엔드 → `vm/<이름>`, `eval/<이름>` (16개)
- 프론트엔드 → `frontend/parse`, `frontend/compile`

정확성은 스위트가 아니라 `tests/benchmark_scenarios_test.cpp`(GoogleTest, `BenchScenarios.*`)가 검증한다. 각 시나리오의 VM/Eval 결과가 등가 Python으로 독립 계산한 기대값과 일치하는지 확인한다.

## 빌드

`cmake`/`ctest`가 PATH에 없으면 CLion 번들을 쓴다:

```bash
CMAKE="/c/Program Files/JetBrains/CLion 2025.2.2/bin/cmake/win/x64/bin/cmake.exe"
CTEST="/c/Program Files/JetBrains/CLion 2025.2.2/bin/cmake/win/x64/bin/ctest.exe"

# 벤치 타깃 빌드
"$CMAKE" --build cmake-build-debug --target HongIkBenchmarks

# 정확성 스모크 (선택)
"$CTEST" --test-dir cmake-build-debug -R BenchScenarios --output-on-failure
```

> 의미 있는 성능 수치는 **최적화(Release) 빌드**로 측정한다. Debug 빌드는 VM/Eval 상대비 파악용으로만 참고할 것.
>
> ```bash
> "$CMAKE" -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
> "$CMAKE" --build cmake-build-release --target HongIkBenchmarks
> ```

## 실행

```bash
# 전체
./cmake-build-release/HongIkBenchmarks

# 특정 벤치만 (정규식 필터)
./cmake-build-release/HongIkBenchmarks --benchmark_filter=vm/arith_loop
./cmake-build-release/HongIkBenchmarks --benchmark_filter='vm/.*'      # VM 전체
./cmake-build-release/HongIkBenchmarks --benchmark_filter='.*fib.*'    # fib 관련

# 반복 측정 (통계 — mean/median/stddev)
./cmake-build-release/HongIkBenchmarks --benchmark_repetitions=5

# JSON 출력 (파일로 저장해 비교·보관)
./cmake-build-release/HongIkBenchmarks --benchmark_format=json > baseline.json
```

시나리오당 iteration은 밀리초 단위(`kMillisecond`)로 보고된다.

## 두 결과 비교 (회귀 판정)

Google Benchmark의 `compare.py`로 두 JSON을 A/B 비교한다. 도구는 이 저장소에 포함되지 않으며, googlebenchmark 소스 트리(`tools/compare.py`)에 있다. FetchContent가 받아둔 위치:

```bash
# FetchContent 캐시 안의 compare.py (경로는 빌드 디렉터리에 따라 다름)
COMPARE=cmake-build-release/_deps/googlebenchmark-src/tools/compare.py
pip install -r cmake-build-release/_deps/googlebenchmark-src/tools/requirements.txt

# before.json → after.json 변화율 표
python "$COMPARE" benchmarks before.json after.json
```

두 파일을 즉석에서 만들어 비교하려면:

```bash
./cmake-build-release/HongIkBenchmarks --benchmark_repetitions=5 --benchmark_format=json > before.json
# ... 코드 변경 후 재빌드 ...
./cmake-build-release/HongIkBenchmarks --benchmark_repetitions=5 --benchmark_format=json > after.json
python "$COMPARE" benchmarks before.json after.json
```

## CPython 참조 스크립트 (`reference/`)

각 `.py`는 대응하는 `.hik`와 **구조 등가**다: 같은 while 루프, 같은 연산 순서, 같은 워크로드 크기. 파이썬 관용 최적화(`sum`/`range`/`join`/컴프리헨션/메모이제이션)는 의도적으로 쓰지 않는다 — 언어 자체 속도가 아니라 "같은 알고리즘을 CPython으로 돌리면 얼마"라는 참고선을 얻기 위함이다. `class_method`는 실제 class·인스턴스 생성, `hashmap_ops`는 `dict`, `string_index`는 `str` 인덱싱으로 매핑했다.

```bash
# 단일 실행 — result=<값> elapsed_ms=<ms> 출력
python benchmarks/reference/arith_loop.py

# 전체 실행
for f in benchmarks/reference/*.py; do echo -n "$f: "; python "$f"; done
```

`result=` 값은 해당 시나리오의 스모크 기대값과 정확히 같다(등가성 증명). `elapsed_ms`는 **수동 1회 측정용 참고 수치**이며, Google Benchmark 스위트에는 포함되지 않는다 — 프로세스 시작·인터프리터 워밍업이 섞인 단발 값이라 스위트의 통계적 측정과 직접 비교하지 말 것. 문서용 수치가 필요하면 5회 실행 후 중앙값을 쓴다. 로컬 검증은 Python 3.14 기준.

## 시나리오 추가 체크리스트

새 시나리오 `<name>`을 추가할 때 아래를 **모두** 갱신한다 (하나라도 빠지면 등록 실패 또는 등가성 깨짐):

1. `benchmarks/scenarios/<name>.hik` — 실제 소스. 마지막 줄 개행 포함, 결정적 결과(정수)로.
2. `tests/benchmark_scenarios_test.cpp` — `BenchScenarios.<Name>` 스모크 테스트 추가. 기대값은 등가 Python 한 줄 계산으로 교차 검증.
3. `benchmarks/reference/<name>.py` — 1:1 구조 등가 스크립트. `result=` 가 스모크 기대값과 일치해야 함.
4. `benchmarks/main.cpp` — `kScenarios` 목록에 `"<name>"` 추가.

문법이 불확실하면 `tests/vm_test.cpp`·`tests/evaluator_test.cpp`에서 실제 사용례를 grep해 확인한다. 벤치 이름·파일명은 ASCII만 (Windows cp949 콘솔 사고 방지).
