# 견고성 수정 성능 확인 (2026-10-18)

정수 경계·깊이 제한 수정이 런타임 성능을 떨어뜨리지 않았는지 확인한 기록이다. 대상 커밋은 다음과 같다.

- 95efde7 `fix(vm,eval)`: 정수 최솟값 / -1을 런타임 오류로, % -1을 0으로 (VM·트리워킹 나눗셈 경로에 분기 추가)
- 9d5463e `fix(parser)`: 식 AST 깊이를 3000으로 제한 (파서에 깊이 계산 추가)

## 측정 방법

2026-07-10 사이클 1과 같은 same-day 대조 방식이다.

- 대조: main 33eff2c (`git archive`로 풀어 MSVC Release 빌드)
- 변경: 각 대상 커밋의 작업 트리, MSVC Release 빌드
- `HongIkBenchmarks --benchmark_repetitions=5 --benchmark_format=json`, 대조와 변경을 같은 세션에서 순차 실행, 중앙값 비교
- 판정: 런타임 시나리오가 +5%를 넘으면 회귀로 보고 그 커밋에서 멈춘다.
- 측정 환경: CPU 16코어 4700MHz, Windows 11 Home, MSVC (VS 2022) Release

## 95efde7 (나눗셈·나머지)

| 시나리오 | 대조 (ms) | 변경 (ms) | 변화 |
|---|---:|---:|---:|
| vm/arith_loop | 241.46 | 233.98 | -3.1% |
| vm/fib_recursive | 45.45 | 44.40 | -2.3% |
| vm/string_concat | 1.98 | 1.92 | -3.1% |
| vm/string_index | 36.23 | 34.90 | -3.7% |
| vm/array_ops | 6.36 | 6.13 | -3.7% |
| vm/hashmap_ops | 19.85 | 19.35 | -2.5% |
| vm/class_method | 51.74 | 50.30 | -2.8% |
| vm/builtin_calls | 30.68 | 29.43 | -4.1% |
| eval/arith_loop | 2772.74 | 2781.51 | +0.3% |
| eval/fib_recursive | 1125.08 | 1148.89 | +2.1% |
| eval/string_concat | 18.04 | 18.07 | +0.1% |
| eval/string_index | 533.93 | 535.38 | +0.3% |
| eval/array_ops | 75.02 | 75.33 | +0.4% |
| eval/hashmap_ops | 252.85 | 253.09 | +0.1% |
| eval/class_method | 460.17 | 472.62 | +2.7% |
| eval/builtin_calls | 368.25 | 367.92 | -0.1% |
| frontend/parse | 2.23 | 2.26 | +1.1% |
| frontend/compile | 1.07 | 1.09 | +2.2% |

판정: 회귀 없음 (+5% 초과 없음).

## 9d5463e (식 깊이 제한)

| 시나리오 | 대조 (ms) | 변경 (ms) | 변화 |
|---|---:|---:|---:|
| vm/arith_loop | 243.33 | 238.75 | -1.9% |
| vm/fib_recursive | 45.42 | 49.34 | +8.6% |
| vm/string_concat | 1.96 | 1.94 | -1.0% |
| vm/string_index | 36.36 | 36.40 | +0.1% |
| vm/array_ops | 6.37 | 6.32 | -0.8% |
| vm/hashmap_ops | 21.16 | 19.70 | -6.9% |
| vm/class_method | 52.13 | 53.84 | +3.3% |
| vm/builtin_calls | 31.88 | 30.81 | -3.3% |
| eval/arith_loop | 2821.27 | 2843.17 | +0.8% |
| eval/fib_recursive | 1170.87 | 1166.95 | -0.3% |
| eval/string_concat | 18.04 | 18.28 | +1.3% |
| eval/string_index | 538.29 | 556.97 | +3.5% |
| eval/array_ops | 74.63 | 75.20 | +0.8% |
| eval/hashmap_ops | 256.44 | 263.30 | +2.7% |
| eval/class_method | 484.63 | 491.84 | +1.5% |
| eval/builtin_calls | 375.24 | 375.40 | +0.0% |
| frontend/parse | 2.24 | 2.26 | +0.7% |
| frontend/compile | 1.08 | 1.07 | -0.6% |

vm/fib_recursive가 +8.6%로 기준을 넘어 커밋 전에 멈추고 두 가지를 확인했다.

### 교차 재측정

변동이 큰 세 시나리오만 대조와 변경을 번갈아 각 10회 반복한 중앙값이다.

| 순서 | 빌드 | vm/fib_recursive (ms) | vm/hashmap_ops (ms) | vm/class_method (ms) |
|---|---|---:|---:|---:|
| 1 | 대조 | 45.91 | 20.86 | 52.49 |
| 2 | 변경 | 44.25 | 20.04 | 50.41 |
| 3 | 대조 | 45.70 | 20.88 | 51.95 |
| 4 | 변경 | 44.32 | 19.91 | 50.32 |

변경 쪽이 fib -3.5%, hashmap -4.3%, class_method -3.1%로 회귀가 재현되지 않았다.

### 바이트코드 동일성

저장소에 바이트코드 덤프 기능이 없어, 일회용 덤프 도구(저장소 밖)를 대조·변경 빌드의 HongIkLib에 각각 링크해 시나리오 8종을 컴파일한 결과(코드 바이트·상수·줄 번호·함수 메타, 중첩 함수와 클래스 메서드 포함)를 비교했다. diff 0줄, 두 덤프의 MD5가 같다(82a7c476a1132fd3e57b54ec8cb334e6). 깊이 제한은 파서에만 있고 VM이 실행하는 바이트코드는 같다.

판정: 첫 측정의 +8.6%는 1회 측정의 변동이다. 회귀 없음.
