"""fib_recursive.hik 등가 스크립트 — 재귀 피보나치(25) (메모이제이션 금지)."""

import time


def fib(n: int) -> int:
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)


def main() -> int:
    return fib(25)


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
