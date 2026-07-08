"""string_concat.hik 등가 스크립트 — 5000회 1자 결합 후 길이 (join 최적화 금지)."""

import time


def main() -> int:
    s: str = ""
    i: int = 0
    while i < 5000:
        s = s + "가"  # "가"
        i += 1
    return len(s)


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
