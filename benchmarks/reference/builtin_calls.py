"""builtin_calls.hik 등가 스크립트 — 내장 len(a) 호출 100000회 누적."""

import time


def main() -> int:
    a: list[int] = [1, 2, 3, 4, 5]
    total: int = 0
    i: int = 0
    while i < 100000:
        total += len(a)
        i += 1
    return total


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
