"""string_index.hik 등가 스크립트 — 5자 문자열 인덱싱 100000회, i%5==0 위치가 "가"."""

import time


def main() -> int:
    s: str = "가나다라마"  # "가나다라마"
    count: int = 0
    i: int = 0
    while i < 100000:
        if s[i % 5] == "가":  # "가"
            count += 1
        i += 1
    return count


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
