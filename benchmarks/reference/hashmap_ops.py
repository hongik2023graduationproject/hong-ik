"""hashmap_ops.hik 등가 스크립트 — dict 3키 갱신·조회 20000회."""

import time


def main() -> int:
    d: dict[str, int] = {"a": 0, "b": 0, "c": 0}
    total: int = 0
    i: int = 0
    while i < 20000:
        d["a"] = i
        d["b"] = i + 1
        d["c"] = i + 2
        total += d["a"] + d["b"] + d["c"]
        i += 1
    return total


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
