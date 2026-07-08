"""array_ops.hik 등가 스크립트 — append 10000회 후 인덱스 순회 합 (sum/comprehension 최적화 금지)."""

import time


def main() -> int:
    a: list[int] = []
    i: int = 0
    while i < 10000:
        a.append(i)
        i += 1
    total: int = 0
    j: int = 0
    while j < 10000:
        total += a[j]
        j += 1
    return total


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
