# VM Feature Parity Design

Date: 2026-03-31

## Goal

Bring the bytecode VM to full feature parity with the tree-walking interpreter. The VM currently lacks 5 features that the evaluator supports. This spec covers each gap and the implementation approach.

## Non-Goals

- VM performance optimization (separate phase)
- Type system strengthening (separate phase)
- Error message improvements (separate phase)
- Lazy/coroutine-based generators (future enhancement)

## Feature Gaps

| # | Feature | Current State | Difficulty |
|---|---------|--------------|------------|
| 1 | String UTF-8 handling | Byte-level indexing and iteration, breaks Korean | Low |
| 2 | Default Parameters | Compiler ignores `defaultValues` | Medium |
| 3 | Index Assignment | No `OP_INDEX_SET` opcode, parser doesn't support it | Medium |
| 4 | Import (`가져오기`) | Explicit `throw` in compiler | Medium |
| 5 | Generator/Yield | No support in VM | High |

## 1. String UTF-8 Handling Fix

### Problem

Two locations use byte-level string access instead of UTF-8 code points:

1. **`OP_ITER_VALUE`** (`vm.cpp:714`): `str->value[iter->index]` iterates bytes, not characters. Korean characters (3 bytes each) are split incorrectly.
2. **`OP_INDEX_GET`** (`vm.cpp:510-517`): `string(1, str->value[actualIdx])` and `str->value.size()` for bounds checking. `"안녕"[1]` returns a broken byte instead of "녕".

Note: `OP_SLICE` already uses `utf8::codePointCount` correctly.

### Solution

#### Iterator fix

Pre-compute code points at iterator initialization.

**`object/object.h`** — Add to `IteratorState`:
```cpp
std::vector<std::string> codePoints; // populated for string iterables
```

**`vm.cpp`** — `OP_ITER_INIT`:
```cpp
if (auto* str = dynamic_cast<String*>(iterable.get())) {
    iter->codePoints = utf8::toCodePoints(str->value);
}
```

**`vm.cpp`** — `OP_ITER_NEXT`: use `iter->codePoints.size()` for exhaustion check.

**`vm.cpp`** — `OP_ITER_VALUE`: `push(make_shared<String>(iter->codePoints[iter->index]))`.

#### Index access fix

**`vm.cpp`** — `OP_INDEX_GET` string branch:
```cpp
auto cps = utf8::toCodePoints(str->value);
long long len = static_cast<long long>(cps.size());
if (actualIdx < 0) actualIdx = len + actualIdx;
if (actualIdx < 0 || actualIdx >= len)
    throw RuntimeException("문자열의 범위 밖 인덱스입니다.", currentLine());
push(make_shared<String>(cps[actualIdx]));
```

### Test Cases

- `각각 문자 c "안녕" 에서: 출력(c)` outputs "안" then "녕"
- `각각 문자 c "abc" 에서: 출력(c)` outputs "a", "b", "c" (ASCII still works)
- `"안녕"[0]` returns "안", `"안녕"[1]` returns "녕", `"안녕"[-1]` returns "녕"
- Empty string: no iterations, index access errors

## 2. Default Parameters

### Problem

Tree-walking evaluator fills missing arguments by evaluating `FunctionStatement.defaultValues` **at call time** (`evaluator.cpp:1033-1038`). The VM compiler ignores default values entirely.

### Solution

Compile each default value expression as a small bytecode chunk. At call time, if `argCount < arity`, execute the default chunks for missing arguments and push results onto the stack.

### Changes

**`vm/chunk.h`** — Add to `CompiledFunction`:
```cpp
int defaultCount = 0;
// Compiled default value expressions, size == arity. nullptr for required params.
std::vector<std::shared_ptr<CompiledFunction>> defaultChunks;
```

**`vm/compiler.cpp`** — `compileFunction`:
- For each parameter with a default value in `stmt->defaultValues`:
  - Create a mini `CompilerState`, compile the default expression, emit `OP_RETURN`
  - Store the resulting `CompiledFunction` in `defaultChunks[paramIndex]`
- Set `defaultCount` to the number of non-null defaults

**`vm/vm.cpp`** — All call paths need arity check changes:
- `OP_CALL` Closure path (line 379)
- `OP_CALL` CompiledFunction path (line 393)
- `OP_CALL` CompiledClassDef constructor path (line 412)
- `OP_INVOKE` method call path (line 648)

For each: change `arity != argCount` to:
```cpp
int requiredArgs = fn->arity - fn->defaultCount;
if (argCount < requiredArgs || argCount > fn->arity) {
    throw RuntimeException("...");
}
// Fill missing defaults
for (int i = argCount; i < fn->arity; i++) {
    if (fn->defaultChunks[i]) {
        auto result = executeDefaultChunk(fn->defaultChunks[i]);
        push(result);
    }
}
```

Extract `executeDefaultChunk` helper to avoid code duplication across call paths.

### Test Cases

```
함수 인사(문자 이름, 문자 접미사 = "님") -> 문자:
    리턴 이름 + 접미사

인사("홍길동")        // "홍길동님"
인사("홍길동", "씨")  // "홍길동씨"
```

- All parameters have defaults: `함수 f(정수 a = 1, 정수 b = 2) -> 정수: 리턴 a + b` then `f()` returns 3
- Too few args with no defaults: should still error
- Default value referencing a builtin: `함수 f(정수 n = 길이("abc")) -> 정수: 리턴 n` then `f()` returns 3
- Methods with default params work via `OP_INVOKE`

## 3. Index Assignment (OP_INDEX_SET)

### Problem

No way to do `배열[0] = 값` or `사전["키"] = 값` in VM. Only `OP_INDEX_GET` exists. The parser also does not currently support this syntax — `parseAssignmentStatement` reads a single `IDENTIFIER` token as the target, with no handling for `IDENTIFIER[expr] = expr`.

This is both a parser gap and a VM gap. The tree-walking evaluator also cannot do this today, so this is technically a new language feature rather than a parity issue. However, it is a fundamental operation expected by users and should be added.

### Solution

#### Parser changes

Add a new AST node `IndexAssignmentStatement`:

**`ast/statements.h`**:
```cpp
class IndexAssignmentStatement : public Statement {
public:
    std::shared_ptr<Expression> collection; // the array/map variable
    std::shared_ptr<Expression> index;      // the index expression
    std::shared_ptr<Expression> value;      // the value to assign
    std::string String() override { return "IndexAssignment"; }
};
```

**`parser/parser.cpp`**: In `parseStatement`, when encountering `IDENTIFIER LBRACKET`, parse as `IndexAssignmentStatement`:
- Parse the identifier as an expression
- Parse `[index]`
- Expect `ASSIGN`
- Parse the value expression

#### Compiler changes

**`vm/opcode.h`**: Add `OP_INDEX_SET`.

**`vm/compiler.cpp`**: Add `compileIndexAssignment`:
```cpp
void Compiler::compileIndexAssignment(IndexAssignmentStatement* stmt) {
    compileExpression(stmt->collection.get()); // push collection
    compileExpression(stmt->index.get());      // push index
    compileExpression(stmt->value.get());       // push value
    chunk().emitOp(OpCode::OP_INDEX_SET, 0);
    chunk().emitOp(OpCode::OP_POP, 0);         // discard result (statement)
}
```

#### VM changes

**`vm/vm.cpp`** — `OP_INDEX_SET` handler:
```cpp
case OpCode::OP_INDEX_SET: {
    auto value = pop();
    auto index = pop();
    auto collection = pop();
    if (auto* arr = dynamic_cast<Array*>(collection.get())) {
        auto* idx = dynamic_cast<Integer*>(index.get());
        if (!idx) throw RuntimeException("배열 인덱스는 정수여야 합니다.", currentLine());
        long long actualIdx = idx->value;
        if (actualIdx < 0) actualIdx += arr->elements.size();
        if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
            throw RuntimeException("배열의 범위 밖 인덱스입니다.", currentLine());
        arr->elements[actualIdx] = value;
    } else if (auto* hm = dynamic_cast<HashMap*>(collection.get())) {
        auto* key = dynamic_cast<String*>(index.get());
        if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", currentLine());
        hm->pairs[key->value] = value;
    } else {
        throw RuntimeException("인덱스 대입이 지원되지 않는 형식입니다.", currentLine());
    }
    push(value);
    break;
}
```

#### Evaluator changes

Also add `IndexAssignmentStatement` handling to `evaluator.cpp` so both execution paths support it.

### Test Cases

```
배열 목록 = [1, 2, 3]
목록[0] = 10
목록[-1] = 30
출력(목록)  // [10, 2, 30]

사전 정보 = {"이름": "홍길동"}
정보["나이"] = "25"
출력(정보["나이"])  // "25"
```

- Out of bounds index: error
- Non-integer array index: error
- Non-string map key: error

## 4. Import (`가져오기`)

### Problem

`compiler.cpp` line 1050 throws `RuntimeException` for any import statement in VM mode.

### Solution

Add `OP_IMPORT` opcode. At runtime, read the file, compile it, and execute in the same VM globals scope.

### Semantics (matching evaluator behavior)

- **Name collisions**: last-write wins (same as evaluator's shared Environment)
- **Circular imports**: prevented via `importedFiles` set with canonicalized paths
- **Path resolution**: use `std::filesystem::weakly_canonical` for dedup, matching evaluator (`evaluator.cpp:1286-1290`)
- **Execution scope**: imported code executes using the same `globals` map, not a separate one

### Changes

**`vm/opcode.h`**: Add `OP_IMPORT`.

**`vm/compiler.cpp`** — `compileImport`:
```cpp
void Compiler::compileImport(ImportStatement* stmt) {
    uint16_t nameIdx = chunk().addConstant(make_shared<String>(stmt->filename));
    chunk().emitOpAndUint16(OpCode::OP_IMPORT, nameIdx, 0);
}
```

**`vm/vm.h`**:
```cpp
std::set<std::string> importedFiles;
IOContext* ioContext = nullptr; // for file reading in WASM
```

**`vm/vm.cpp`** — `OP_IMPORT` handler:
1. Read filename string from constants
2. Canonicalize path via `std::filesystem::weakly_canonical`
3. Check `importedFiles` — if already imported, skip (no-op)
4. Add to `importedFiles`
5. Read file content (direct file I/O for native, `ioContext->fileReadCallback` for WASM)
6. Lexer -> Parser -> Compiler pipeline to get `CompiledFunction`
7. Push as new `CallFrame` and execute — globals defined by the imported code go into `this->globals`

### Test Cases

```
// helper.hik
함수 제곱(정수 x) -> 정수:
    리턴 x * x

// main.hik
가져오기 "helper.hik"
제곱(5)  // 25
```

- Circular import: no infinite loop (second import is silently skipped)
- Non-existent file: `FILE_ERROR` with clear message
- Double import of same file: no-op
- `"./helper.hik"` and `"helper.hik"` resolve to the same file

## 5. Generator/Yield

### Problem

Tree-walking detects `yield` in function bodies (`containsYield`), runs the full body collecting yield values, and returns a `GeneratorObject` (iterable). VM has no yield mechanism.

### Solution

Eager evaluation approach (matching tree-walking behavior): when a function with `yield` is called, execute the entire body, collect yielded values into an array, return an iterator over that array.

### Changes

**`vm/opcode.h`**: Add `OP_YIELD`.

**`vm/chunk.h`** — Add to `CompiledFunction`:
```cpp
bool hasYield = false;
```

**`vm/compiler.cpp`**:
- `compileFunction`: scan body for `YieldStatement` nodes (recursive, like evaluator's `containsYield`), set `hasYield = true`
- Add `compileYield`: compile the expression, emit `OP_YIELD`
- Register `YieldStatement` in `compileStatement`

**`vm/vm.h`** — Add to `CallFrame` (per-frame, not global, to support nested generators):
```cpp
bool isGenerator = false;
std::vector<std::shared_ptr<Object>> yieldBuffer;
```

**`vm/vm.cpp`**:
- `OP_CALL`: if callee function has `hasYield`, set `frame.isGenerator = true`
- `OP_YIELD`: push value to `currentFrame().yieldBuffer`
- `OP_RETURN` (when `frame.isGenerator`):
  - Instead of returning the expression result, wrap `frame.yieldBuffer` into an `Array`
  - Create `IteratorState` from that array
  - Return the iterator to the caller

This per-frame approach correctly handles nested generators: if generator A calls generator B, each has its own `yieldBuffer` in its own `CallFrame`.

### Integration with iterator opcodes

The generator returns an `IteratorState` wrapping an `Array`, so existing `OP_ITER_INIT`/`OP_ITER_NEXT`/`OP_ITER_VALUE` work without changes — they already handle `Array` iterables.

### Test Cases

```
함수 세개() -> 정수:
    생산 1
    생산 2
    생산 3

각각 정수 값 세개() 에서:
    출력(값)
// 1, 2, 3
```

- Generator with loop:
```
함수 범위(정수 n) -> 정수:
    반복 정수 i = 0 부터 n 까지:
        생산 i

각각 정수 x 범위(5) 에서:
    출력(x)
// 0, 1, 2, 3, 4
```

- Generator with conditionals
- Empty generator (no yields): returns empty iterator
- Nested generators: outer generator calls inner generator, both collect independently

## Implementation Order

1. **String UTF-8 handling** — bug fix for iterator + index access, no new opcodes
2. **Default Parameters** — new fields in `CompiledFunction`, modify all call paths
3. **Index Assignment** — new opcode `OP_INDEX_SET`, parser changes, evaluator update
4. **Import** — new opcode `OP_IMPORT`, file I/O integration
5. **Generator/Yield** — new opcode `OP_YIELD`, per-frame state, modify `OP_CALL`/`OP_RETURN`

Each step is independently testable and deployable. Later steps do not depend on earlier ones (except that all modify `opcode.h`).

## Files Changed Summary

| File | Changes |
|------|---------|
| `vm/opcode.h` | Add `OP_INDEX_SET`, `OP_IMPORT`, `OP_YIELD`; update `opcodeName()` |
| `vm/chunk.h` | `CompiledFunction`: `defaultChunks`, `defaultCount`, `hasYield`; `CallFrame`: `isGenerator`, `yieldBuffer` |
| `vm/compiler.h` | New compile methods: `compileYield`, `compileIndexAssignment` |
| `vm/compiler.cpp` | Rewrite `compileImport`, add `compileYield`, `compileIndexAssignment`, default param compilation |
| `vm/vm.h` | `importedFiles`, IO context reference |
| `vm/vm.cpp` | New opcode handlers, modify `OP_CALL`/`OP_RETURN` for defaults + generators, fix UTF-8 in `OP_INDEX_GET` + iterators |
| `object/object.h` | `IteratorState.codePoints` field |
| `ast/statements.h` | New `IndexAssignmentStatement` node |
| `parser/parser.h` | `parseIndexAssignment` declaration |
| `parser/parser.cpp` | Parse `목록[0] = 값` syntax |
| `evaluator/evaluator.cpp` | Handle `IndexAssignmentStatement` |
| `tests/vm_test.cpp` | Tests for all 5 features |
