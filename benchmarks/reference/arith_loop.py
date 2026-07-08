"""arith_loop.hik 등가 스크립트 — 1..1_000_000 정수 합 (while 루프, sum/range 최적화 금지)."""

import time


def main() -> int:
    total: int = 0
    i: int = 1
    while i <= 1_000_000:
        total += i
        i += 1
    return total


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
