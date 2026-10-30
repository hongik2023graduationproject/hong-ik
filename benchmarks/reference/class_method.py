"""class_method.hik 등가 스크립트 — 매 반복 인스턴스 생성 + 메서드 호출 50000회."""

import time


class Counter:
    value: int

    def __init__(self, start: int) -> None:
        self.value = start

    def increment(self) -> int:
        self.value = self.value + 1
        return self.value


def main() -> int:
    total: int = 0
    i: int = 0
    while i < 50000:
        c = Counter(i)
        total += c.increment()
        i += 1
    return total


start = time.perf_counter()
result = main()
elapsed_ms = (time.perf_counter() - start) * 1000
print(f"result={result} elapsed_ms={elapsed_ms:.2f}")
