# VM Optimization Design

Date: 2026-04-01

## Goal

Optimize the bytecode VM for both practical performance gains and compiler quality. Three layers of optimization, each independently completable across sessions.

## Non-Goals

- Changing the tree-walking evaluator
- Modifying external interfaces (object.h, parser, evaluator)
- JIT compilation
- Full project-wide tagged union migration (future phase)

## Architecture: Layered Optimization

```
Layer 1: Quick Wins (no structural changes)
    ↓
Layer 2: Compiler Optimization Passes
    ↓
Layer 3: VM Internal Tagged Value
```

Each layer is independently testable and deployable. Benchmark before/after each layer.

---

## Layer 1: Quick Wins

### 1-1. Boolean/Null Singleton Caching

**Problem:** `OP_TRUE`, `OP_FALSE`, `OP_NULL` and comparison results in `binaryOp` allocate new heap objects every time.

**Solution:** Pre-create singleton instances in VM constructor and reuse them.

**Changes:**
- `vm/vm.h` — Add `shared_ptr<Object> TRUE_OBJ, FALSE_OBJ, NULL_OBJ` members
- `vm/vm.cpp` — Initialize in constructor, use in `OP_TRUE`/`OP_FALSE`/`OP_NULL` and `binaryOp` comparison results

### 1-2. O(n^2) Argument Collection Fix

**Problem:** Builtin function calls use `args.insert(args.begin(), peek(i))` which is O(n) per insertion.

**Solution:** Pre-allocate vector and fill by index.

**Changes:**
- `vm/vm.cpp` — `OP_CALL` builtin path (~line 375) and `OP_INVOKE` builtin path (~line 630)

Replace:
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

### 1-3. String UTF-8 Code Point Caching

**Problem:** `OP_INDEX_GET` and `OP_SLICE` call `utf8::toCodePoints()` on every access, re-parsing the entire string.

**Solution:** Add lazy-computed code point cache to `String` class.

**Changes:**
- `object/object.h` — Add to `String`:
  ```cpp
  mutable std::vector<std::string> codePointCache_;
  mutable bool codePointCacheValid_ = false;

  const std::vector<std::string>& codePoints() const {
      if (!codePointCacheValid_) {
          codePointCache_ = utf8::toCodePoints(value);
          codePointCacheValid_ = true;
      }
      return codePointCache_;
  }
  ```
- `vm/vm.cpp` — Replace `utf8::toCodePoints(str->value)` calls with `str->codePoints()` in `OP_INDEX_GET`, `OP_ITER_INIT`, and `OP_SLICE`

### 1-4. Small Integer Pooling (-128 to 127)

**Problem:** Every integer operation allocates a new `Integer` object on the heap, even for small frequently-used values.

**Solution:** Pre-allocate a pool of Integer objects for the range [-128, 127].

**Changes:**
- `vm/vm.h` — Add `shared_ptr<Integer> intPool[256]` and `shared_ptr<Object> makeInt(long long val)` helper
- `vm/vm.cpp` — Initialize pool in constructor, use `makeInt()` in `binaryOp` and `OP_CONSTANT` for integer results

---

## Layer 2: Compiler Optimization Passes

### 2-1. Constant Folding Enhancement

**Current state:** `tryConstantFold` in `compiler.cpp` handles `+`, `-`, `*`, `/` for integer, float, and string concatenation.

**Enhancement:** Extend to:
- Comparison operators: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Modulo and power: `%`, `**`
- Unary constant folding: `-5` → constant, `!true` → `OP_FALSE`

**Changes:**
- `vm/compiler.cpp` — Extend `tryConstantFold` to handle comparison/modulo/power
- `vm/compiler.cpp` — Add `tryUnaryConstantFold` in `compilePrefix`

### 2-2. Dead Code Elimination

**Problem:** Code after `리턴`, `중단`, `계속` is compiled but never executed, wasting bytecode space.

**Solution:** Track reachability in the compiler. After emitting `OP_RETURN`, `OP_JUMP` (from break/continue), set an `isUnreachable` flag. Skip emitting subsequent statements until the flag is reset (at new block/branch entry).

**Changes:**
- `vm/compiler.h` — Add `bool isUnreachable` to `CompilerState`
- `vm/compiler.cpp` — Set after `OP_RETURN`/break/continue, check in `compileStatement`, reset at block/branch boundaries

### 2-3. Peephole Optimization

**Problem:** The compiler emits suboptimal bytecode patterns that could be simplified.

**Solution:** Post-compilation pass over bytecode to recognize and replace patterns.

Patterns to optimize:
- `OP_JUMP` to next instruction → remove (no-op jump)
- Consecutive `OP_POP` → could be batched (future: `OP_POPN`)
- `OP_SET_LOCAL N; OP_POP; OP_GET_LOCAL N` → `OP_SET_LOCAL N` (value stays on stack)

**Changes:**
- `vm/compiler.h` — Add `void optimize(CompiledFunction& fn)` declaration
- `vm/compiler.cpp` — Implement `optimize()` as a separate pass after compilation, called from `Compile()`

---

## Layer 3: VM Internal Tagged Value

### 3-1. VMValue Type Definition

New file `vm/vm_value.h`:

```cpp
enum class ValueTag : uint8_t {
    INT,      // long long, inline
    FLOAT,    // double, inline
    BOOL,     // bool, inline
    NULL_V,   // no data
    OBJECT    // shared_ptr<Object> for String, Array, HashMap, Function, etc.
};

struct VMValue {
    ValueTag tag;
    union {
        long long intVal;
        double floatVal;
        bool boolVal;
    };
    std::shared_ptr<Object> objVal; // only used when tag == OBJECT

    // Constructors
    static VMValue Int(long long v);
    static VMValue Float(double v);
    static VMValue Bool(bool v);
    static VMValue Null();
    static VMValue Obj(std::shared_ptr<Object> o);

    // Conversion
    std::shared_ptr<Object> toObject() const;
    static VMValue fromObject(std::shared_ptr<Object> o);
};
```

Primitive types (int, float, bool, null) are stored inline — zero heap allocation.
Complex types (string, array, hashmap, function, instance, etc.) use existing `shared_ptr<Object>`.

### 3-2. VM Stack Replacement

- `vector<shared_ptr<Object>>` → `vector<VMValue>`
- `push()`, `pop()`, `peek()` change signature to use `VMValue`
- `globals` map: `map<string, VMValue>`

### 3-3. binaryOp Hot Path

Replace `dynamic_cast` chains with `tag` comparison:

```cpp
VMValue VM::binaryOp(OpCode op, const VMValue& left, const VMValue& right, long long line) {
    if (left.tag == ValueTag::INT && right.tag == ValueTag::INT) {
        // Direct arithmetic, no heap allocation
        switch (op) {
        case OP_ADD: return VMValue::Int(left.intVal + right.intVal);
        ...
        }
    }
    // Float, String, etc. paths
}
```

### 3-4. VM Boundary Conversion

Builtin functions still use `vector<shared_ptr<Object>>` interface. Convert at call boundary:

```cpp
// OP_CALL builtin path:
vector<shared_ptr<Object>> args(argCount);
for (int i = 0; i < argCount; i++) {
    args[i] = peek(argCount - 1 - i).toObject();
}
auto result = builtin->function(args);
push(VMValue::fromObject(result));
```

Constant pool remains `vector<shared_ptr<Object>>`. Convert on load:
```cpp
case OP_CONSTANT: {
    uint16_t idx = readUint16();
    push(VMValue::fromObject(frame.function->constants[idx]));
    break;
}
```

### 3-5. Scope of Changes

- **New file:** `vm/vm_value.h`
- **Heavy modification:** `vm/vm.h`, `vm/vm.cpp` (most of the file)
- **Light modification:** `vm/chunk.h` (constant access patterns)
- **No changes:** `object/object.h`, `evaluator/`, `parser/`, `ast/`

### Expected Performance Improvement

- Arithmetic loops: 2-5x (zero heap allocation + no dynamic_cast)
- Boolean-heavy code: 3-5x (inline bool + no allocation)
- Mixed code: 1.5-2x (boundary conversion overhead for complex types)

---

## Benchmark & Verification

### Benchmark Scenarios

In `tests/benchmark.cpp`:

1. **Arithmetic loop**: 1M integer additions — measures allocation overhead
2. **Fibonacci recursive**: `fib(30)` — measures call overhead
3. **String processing**: Korean string repeated indexing — measures UTF-8 caching
4. **Array manipulation**: Array creation + index assignment loop — measures object allocation

Run each scenario in both VM and tree-walking mode for comparison.

### Verification Strategy

- All 331+ existing tests must pass after each layer
- Benchmark run before/after each layer, record numbers
- Layer 3 requires extra attention: internal representation changes but external behavior must be identical

---

## Implementation Order

Each layer is a separate implementation cycle (spec → plan → execute):

1. **Layer 1: Quick Wins** — 4 independent optimizations, low risk
2. **Layer 2: Compiler Passes** — 3 optimization passes, medium risk
3. **Layer 3: Tagged Value** — major internal refactor, high risk

Layers can be done across different sessions. Each layer should be committed and tested independently.

## Files Changed Summary

| Layer | File | Changes |
|-------|------|---------|
| 1 | `vm/vm.h` | Singleton cache, int pool |
| 1 | `vm/vm.cpp` | Use cached objects, fix O(n^2) args |
| 1 | `object/object.h` | String code point cache |
| 2 | `vm/compiler.h` | `optimize()`, `isUnreachable` |
| 2 | `vm/compiler.cpp` | Constant folding, DCE, peephole |
| 3 | `vm/vm_value.h` | New: VMValue tagged union |
| 3 | `vm/vm.h` | Stack type change |
| 3 | `vm/vm.cpp` | Full rewrite of value handling |
| 3 | `vm/chunk.h` | Constant access patterns |
| All | `tests/benchmark.cpp` | Performance benchmarks |
| All | `tests/vm_test.cpp` | Verify no regressions |
