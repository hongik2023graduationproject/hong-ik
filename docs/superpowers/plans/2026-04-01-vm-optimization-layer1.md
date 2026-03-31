# VM Optimization Layer 1: Quick Wins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce heap allocations and fix algorithmic inefficiencies in the VM runtime without structural changes.

**Architecture:** Four independent optimizations targeting the VM's hottest paths: singleton caching for boolean/null, O(n²) argument collection fix, String UTF-8 code point caching, and small integer pooling. Each is independently testable.

**Tech Stack:** C++17, CMake, Google Test

**Spec:** `docs/superpowers/specs/2026-04-01-vm-optimization-design.md` (Layer 1 section)

**Build & Test:**
```bash
cmake --build cmake-build-debug
cmake-build-debug/HongIkTests --gtest_filter="VMTest.*"
ctest --test-dir cmake-build-debug
```

---

## Task 1: Boolean/Null Singleton Caching + O(n²) Arg Fix + String Cache + Int Pool

All 4 optimizations in one task since they are small, independent changes to `vm/vm.h`, `vm/vm.cpp`, and `object/object.h`.

**Files:**
- Modify: `vm/vm.h:53-64` — add cached singletons, int pool, makeInt helper
- Modify: `vm/vm.cpp:18-68` — initialize caches in constructor
- Modify: `vm/vm.cpp:127-218` — use cached objects in binaryOp
- Modify: `vm/vm.cpp:236-238` — use singletons for OP_TRUE/FALSE/NULL
- Modify: `vm/vm.cpp:375-381, 671-678` — fix O(n²) arg collection
- Modify: `object/object.h:72-83` — add code point cache to String
- Modify: `vm/vm.cpp:513-519` — use String::codePoints() in OP_INDEX_GET
- Modify: `tests/vm_test.cpp` — add benchmark-style verification test

### Part A: Singleton Caching

- [ ] **Step 1: Add singleton members to VM**

In `vm/vm.h`, add to the private section (after line 64):

```cpp
// Cached singletons
std::shared_ptr<Object> CACHED_TRUE;
std::shared_ptr<Object> CACHED_FALSE;
std::shared_ptr<Object> CACHED_NULL;
```

- [ ] **Step 2: Initialize singletons in VM constructor**

In `vm/vm.cpp`, at the end of `VM::VM()` (before the closing `}`), add:

```cpp
CACHED_TRUE = make_shared<Boolean>(true);
CACHED_FALSE = make_shared<Boolean>(false);
CACHED_NULL = make_shared<Null>();
```

- [ ] **Step 3: Use singletons in OP_TRUE/FALSE/NULL**

In `vm/vm.cpp`, replace lines 236-238:

```cpp
case OpCode::OP_NULL: push(CACHED_NULL); break;
case OpCode::OP_TRUE: push(CACHED_TRUE); break;
case OpCode::OP_FALSE: push(CACHED_FALSE); break;
```

- [ ] **Step 4: Use singletons in binaryOp for boolean results**

In `vm/vm.cpp`, in `binaryOp`, replace all `make_shared<Boolean>(...)` with cached versions. For example:

```cpp
// Null comparison (lines 131-132)
if (op == OpCode::OP_EQUAL) return (ln && rn) ? CACHED_TRUE : CACHED_FALSE;
if (op == OpCode::OP_NOT_EQUAL) return (ln && rn) ? CACHED_FALSE : CACHED_TRUE;

// Integer comparison (lines 158-163)
case OpCode::OP_EQUAL: return (lv == rv) ? CACHED_TRUE : CACHED_FALSE;
case OpCode::OP_NOT_EQUAL: return (lv != rv) ? CACHED_TRUE : CACHED_FALSE;
case OpCode::OP_LESS: return (lv < rv) ? CACHED_TRUE : CACHED_FALSE;
case OpCode::OP_GREATER: return (lv > rv) ? CACHED_TRUE : CACHED_FALSE;
case OpCode::OP_LESS_EQUAL: return (lv <= rv) ? CACHED_TRUE : CACHED_FALSE;
case OpCode::OP_GREATER_EQUAL: return (lv >= rv) ? CACHED_TRUE : CACHED_FALSE;
```

Apply the same pattern to float comparisons (lines 184-189), boolean operations (lines 198-201), and string comparisons (lines 211-212).

- [ ] **Step 5: Build and run tests**

Run: `cmake --build cmake-build-debug && cmake-build-debug/HongIkTests --gtest_filter="VMTest.*"`
Expected: All existing tests PASS (singletons are semantically identical)

- [ ] **Step 6: Commit**

```bash
git add vm/vm.h vm/vm.cpp
git commit -m "perf: cache Boolean/Null singletons in VM to reduce heap allocations"
```

### Part B: O(n²) Argument Collection Fix

- [ ] **Step 7: Fix builtin call arg collection in OP_CALL**

In `vm/vm.cpp`, around line 374-381, replace:

```cpp
vector<shared_ptr<Object>> args;
for (int i = argCount - 1; i >= 0; i--) {
    args.insert(args.begin(), peek(i));
}
```

With:

```cpp
vector<shared_ptr<Object>> args(argCount);
for (int i = 0; i < argCount; i++) {
    args[i] = peek(argCount - 1 - i);
}
```

- [ ] **Step 8: Fix builtin call arg collection in OP_INVOKE**

In `vm/vm.cpp`, around line 670-678, apply the same fix. There are two patterns here:

First pattern (around line 671-674):
```cpp
vector<shared_ptr<Object>> args(argCount);
for (int i = 0; i < argCount; i++) {
    args[i] = peek(argCount - 1 - i);
}
```

Second pattern includes `target` insertion (around line 677). Replace `args.insert(args.begin(), target)` with pre-allocating `args(argCount + 1)` and placing `target` at index 0:
```cpp
vector<shared_ptr<Object>> args(argCount + 1);
args[0] = target;
for (int i = 0; i < argCount; i++) {
    args[i + 1] = peek(argCount - 1 - i);
}
```

- [ ] **Step 9: Build and run tests**

Run: `cmake --build cmake-build-debug && cmake-build-debug/HongIkTests --gtest_filter="VMTest.*"`
Expected: All PASS

- [ ] **Step 10: Commit**

```bash
git add vm/vm.cpp
git commit -m "perf: fix O(n²) argument collection in VM builtin calls"
```

### Part C: String UTF-8 Code Point Caching

- [ ] **Step 11: Add code point cache to String class**

In `object/object.h`, modify the `String` class (lines 72-83):

```cpp
class String final : public Object {
public:
    String(std::string value) : value(std::move(value)) {
        type = ObjectType::STRING;
    }

    std::string value;

    // Lazy-computed UTF-8 code point cache
    const std::vector<std::string>& codePoints() const {
        if (!codePointCacheValid_) {
            codePointCache_ = utf8::toCodePoints(value);
            codePointCacheValid_ = true;
        }
        return codePointCache_;
    }

    std::string ToString() override {
        return value;
    }

private:
    mutable std::vector<std::string> codePointCache_;
    mutable bool codePointCacheValid_ = false;
};
```

Add `#include "../util/utf8_utils.h"` at the top of `object/object.h` if not already present.

- [ ] **Step 12: Use String::codePoints() in VM**

In `vm/vm.cpp`, replace `utf8::toCodePoints(str->value)` calls with `str->codePoints()`:

1. `OP_INDEX_GET` string branch (around line 513):
   ```cpp
   auto& cps = str->codePoints();
   ```

2. `OP_ITER_INIT` (around line 691):
   ```cpp
   iter->codePoints = str->codePoints();
   ```

- [ ] **Step 13: Build and run tests**

Run: `cmake --build cmake-build-debug && cmake-build-debug/HongIkTests --gtest_filter="VMTest.*"`
Expected: All PASS

- [ ] **Step 14: Commit**

```bash
git add object/object.h vm/vm.cpp
git commit -m "perf: add lazy UTF-8 code point cache to String objects"
```

### Part D: Small Integer Pooling

- [ ] **Step 15: Add integer pool to VM**

In `vm/vm.h`, add to private section:

```cpp
// Small integer pool [-128, 127]
static constexpr int INT_POOL_MIN = -128;
static constexpr int INT_POOL_MAX = 127;
static constexpr int INT_POOL_SIZE = INT_POOL_MAX - INT_POOL_MIN + 1;
std::shared_ptr<Integer> intPool[INT_POOL_SIZE];

std::shared_ptr<Object> makeInt(long long val);
```

- [ ] **Step 16: Initialize pool and implement makeInt**

In `vm/vm.cpp`, at end of `VM::VM()`:

```cpp
for (int i = 0; i < INT_POOL_SIZE; i++) {
    intPool[i] = make_shared<Integer>(INT_POOL_MIN + i);
}
```

Add `makeInt` implementation:

```cpp
shared_ptr<Object> VM::makeInt(long long val) {
    if (val >= INT_POOL_MIN && val <= INT_POOL_MAX) {
        return intPool[val - INT_POOL_MIN];
    }
    return make_shared<Integer>(val);
}
```

- [ ] **Step 17: Use makeInt in binaryOp**

In `vm/vm.cpp`, in `binaryOp`, replace all `make_shared<Integer>(...)` with `makeInt(...)`:

```cpp
case OpCode::OP_ADD: return makeInt(lv + rv);
case OpCode::OP_SUB: return makeInt(lv - rv);
case OpCode::OP_MUL: return makeInt(lv * rv);
case OpCode::OP_DIV:
    if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
    return makeInt(lv / rv);
case OpCode::OP_MOD:
    if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
    return makeInt(lv % rv);
case OpCode::OP_POW: {
    if (rv < 0) return make_shared<Float>(std::pow(static_cast<double>(lv), static_cast<double>(rv)));
    long long result = 1;
    for (long long i = 0; i < rv; i++) result *= lv;
    return makeInt(result);
}
case OpCode::OP_BITWISE_AND: return makeInt(lv & rv);
case OpCode::OP_BITWISE_OR: return makeInt(lv | rv);
```

- [ ] **Step 18: Build and run ALL tests**

Run: `cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug`
Expected: All 331+ tests PASS

- [ ] **Step 19: Commit**

```bash
git add vm/vm.h vm/vm.cpp
git commit -m "perf: add small integer pooling [-128, 127] to VM"
```

### Part E: Verification Test

- [ ] **Step 20: Add performance verification test**

In `tests/vm_test.cpp`, add a test that exercises all optimized paths:

```cpp
TEST_F(VMTest, OptimizationVerification) {
    // Exercises: int pool, bool singleton, string cache, builtin args
    auto result = runVM(
        "정수 합 = 0\n"
        "반복 정수 i = 0 부터 100 까지:\n"
        "    합 = 합 + i\n"
        "합\n"
    );
    auto* i = dynamic_cast<Integer*>(result.get());
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value, 4950);
}
```

- [ ] **Step 21: Run full test suite**

Run: `cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug`
Expected: All PASS

- [ ] **Step 22: Commit**

```bash
git add tests/vm_test.cpp
git commit -m "test: add optimization verification test"
```
