# Type System Strengthening Design

Date: 2026-04-01

## Goal

Add runtime type checking to enforce declared types at variable assignment, function calls, and operations. Type mismatches produce clear `TYPE_ERROR` exceptions immediately.

## Non-Goals

- Static type checking (compile-time analysis pass)
- Type inference (auto-deducing types without declarations)
- Generic types or parameterized collections (`정수배열`, `문자사전`)
- Array/map element type enforcement (deferred to future grammar extension)

## Approach

Runtime type checks inserted at key points in both the tree-walking evaluator and the bytecode VM. The language is dynamically typed with type annotations — annotations become enforced constraints.

## Type Mapping

| Korean Type | Token | ObjectType / ValueTag |
|---|---|---|
| `정수` | TokenType::정수 | INTEGER / INT |
| `실수` | TokenType::실수 | FLOAT / FLOAT |
| `문자` | TokenType::문자 | STRING / OBJECT(String) |
| `논리` | TokenType::논리 | BOOLEAN / BOOL |
| `배열` | TokenType::배열 | ARRAY / OBJECT(Array) |
| `사전` | TokenType::사전 | HASH_MAP / OBJECT(HashMap) |

Optional types (`정수?`) additionally accept `NULL_OBJ` / `NULL_V`.

---

## 1. Variable Type Checking

### Problem

`정수 x = "hello"` and `x = 3.14` execute without error despite type mismatch.

### Solution

#### Environment type tracking (tree-walking)

Add type map to `Environment`:

**`environment/environment.h`:**
```cpp
std::map<std::string, ObjectType> typeMap;
bool isOptional(const std::string& name) const;
void SetType(const std::string& name, ObjectType type, bool optional = false);
ObjectType GetType(const std::string& name) const;
```

**`environment/environment.cpp`:**
- `SetType`: stores declared type
- `GetType`: returns declared type (or NULL_OBJ if not found, meaning untyped)

#### Evaluator changes

**`evaluator/evaluator.cpp`** — `InitializationStatement` handling:
- After evaluating value, compare `value->type` with declared type from `stmt->type`
- If mismatch and not (optional + null): throw `RuntimeException("'타입' 타입 변수에 '실제타입' 값을 대입할 수 없습니다.")`
- Store type in environment: `environment->SetType(name, declaredType, isOptional)`

**`evaluator/evaluator.cpp`** — `AssignmentStatement` handling:
- Look up stored type via `environment->GetType(name)`
- If type exists (not untyped), compare with new value's type
- If mismatch: throw TYPE_ERROR

#### VM changes

**`vm/compiler.cpp`:**
- Emit type information alongside `OP_DEFINE_GLOBAL`: encode declared type as an extra byte after the opcode
- For locals: store type info in a parallel metadata structure

**`vm/vm.h`:**
- Add `std::map<std::string, ObjectType> globalTypes` for global variable types
- Add type metadata to local variable tracking

**`vm/vm.cpp`:**
- `OP_DEFINE_GLOBAL`: read type info, store in `globalTypes`, validate value
- `OP_SET_GLOBAL`: look up type in `globalTypes`, validate new value
- `OP_SET_LOCAL`: validate against local type metadata

### Type check helper

Shared helper function for type name conversion:

```cpp
std::string typeToKorean(ObjectType type) {
    switch (type) {
    case ObjectType::INTEGER: return "정수";
    case ObjectType::FLOAT: return "실수";
    case ObjectType::STRING: return "문자";
    case ObjectType::BOOLEAN: return "논리";
    case ObjectType::ARRAY: return "배열";
    case ObjectType::HASH_MAP: return "사전";
    default: return "알 수 없는 타입";
    }
}
```

For VMValue:
```cpp
std::string tagToKorean(ValueTag tag) {
    switch (tag) {
    case ValueTag::INT: return "정수";
    case ValueTag::FLOAT: return "실수";
    case ValueTag::BOOL: return "논리";
    case ValueTag::NULL_V: return "없음";
    case ValueTag::OBJECT: return "객체"; // further refined by Object->type
    }
}
```

### Error examples

```
정수 x = "hello"     // TYPE_ERROR: '정수' 타입 변수에 '문자' 값을 대입할 수 없습니다
정수 y = 10
y = 3.14             // TYPE_ERROR: '정수' 타입 변수에 '실수' 값을 대입할 수 없습니다
정수? z = 없음        // OK (optional allows null)
z = 42               // OK
z = "hello"          // TYPE_ERROR: '정수' 타입 변수에 '문자' 값을 대입할 수 없습니다
```

---

## 2. Function Call Type Checking

### Problem

Calling a function with wrong argument types or returning wrong type produces no error.

### Solution

#### Parameter type checking

At function call time, compare each argument's type against the declared parameter type.

**Evaluator:** In the function call handling (around line 1030), after building the argument list, iterate and check each argument against `fn->parameterTypes[i]`.

**VM:** Store parameter type info in `CompiledFunction`:
```cpp
std::vector<ObjectType> paramTypeChecks; // parallel to parameters
```
In `OP_CALL`, after filling defaults, check each arg on the stack against `paramTypeChecks`.

#### Return type checking

**Evaluator:** When a `ReturnValue` is captured, check against the function's declared `returnType`.

**VM:** Store return type in `CompiledFunction`:
```cpp
ObjectType returnTypeCheck = ObjectType::NULL_OBJ; // NULL_OBJ = no check
```
In `OP_RETURN`, if `returnTypeCheck != NULL_OBJ`, validate the return value.

#### Error examples

```
함수 더하기(정수 a, 정수 b) -> 정수:
    리턴 a + b

더하기("hello", 1)
// TYPE_ERROR: 매개변수 'a'는 '정수' 타입이지만 '문자' 값이 전달되었습니다

함수 잘못() -> 정수:
    리턴 "hello"
// TYPE_ERROR: '정수' 반환 타입에 '문자' 값을 반환할 수 없습니다
```

### Changes

- `vm/chunk.h` — `CompiledFunction`: add `paramTypeChecks`, `returnTypeCheck`
- `vm/compiler.cpp` — copy type info from `FunctionStatement` to `CompiledFunction`
- `vm/vm.cpp` — `OP_CALL`: param type check, `OP_RETURN`: return type check
- `evaluator/evaluator.cpp` — function call and return handling

---

## 3. Operation Type Error Messages

### Problem

`binaryOp` throws generic "좌항과 우항의 타입을 연산할 수 없습니다" without specifying which types or which operation.

### Solution

Improve error messages to include specific type names and operator symbol.

#### VM (`vm/vm.cpp` — `binaryOp`)

Replace the generic throw at the end with:
```cpp
std::string leftName = tagToKorean(left.tag);
std::string rightName = tagToKorean(right.tag);
if (left.tag == ValueTag::OBJECT) leftName = typeToKorean(left.objVal->type);
if (right.tag == ValueTag::OBJECT) rightName = typeToKorean(right.objVal->type);
std::string opName = opToSymbol(op);
throw RuntimeException(
    "'" + leftName + "'와 '" + rightName + "' 타입 간 '"
    + opName + "' 연산을 수행할 수 없습니다.", line);
```

#### Helper: opToSymbol

```cpp
std::string opToSymbol(OpCode op) {
    switch (op) {
    case OpCode::OP_ADD: return "+";
    case OpCode::OP_SUB: return "-";
    case OpCode::OP_MUL: return "*";
    case OpCode::OP_DIV: return "/";
    case OpCode::OP_MOD: return "%";
    case OpCode::OP_POW: return "**";
    case OpCode::OP_EQUAL: return "==";
    case OpCode::OP_NOT_EQUAL: return "!=";
    case OpCode::OP_LESS: return "<";
    case OpCode::OP_GREATER: return ">";
    default: return "?";
    }
}
```

#### Evaluator

Apply same improvement to evaluator's operation error messages.

---

## 4. Array/Map Element Types

**Deferred.** Current grammar has no syntax for element type annotation. Array/map variable-level type checking (e.g., `정수 x = [1,2,3]` fails because `x` is `정수` not `배열`) is already covered by variable type checking in section 1.

---

## TokenType to ObjectType Conversion

Needed in both evaluator and compiler. Shared utility:

```cpp
ObjectType tokenTypeToObjectType(TokenType tt) {
    switch (tt) {
    case TokenType::정수: return ObjectType::INTEGER;
    case TokenType::실수: return ObjectType::FLOAT;
    case TokenType::문자: return ObjectType::STRING;
    case TokenType::논리: return ObjectType::BOOLEAN;
    case TokenType::배열: return ObjectType::ARRAY;
    case TokenType::사전: return ObjectType::HASH_MAP;
    default: return ObjectType::NULL_OBJ; // unknown = no type check
    }
}
```

---

## Implementation Order

1. **Type helpers** — conversion functions, error message helpers
2. **Variable type check (evaluator)** — Environment type map + init/assign checks
3. **Variable type check (VM)** — global/local type metadata + define/set checks
4. **Function call type check (evaluator)** — param + return checks
5. **Function call type check (VM)** — CompiledFunction metadata + OP_CALL/RETURN checks
6. **Operation error messages** — binaryOp improvements in both evaluator and VM

Each step independently testable. Steps 2-3 and 4-5 can be done in parallel for evaluator/VM.

## Files Changed Summary

| File | Changes |
|------|---------|
| `environment/environment.h` | Add type map, SetType/GetType |
| `environment/environment.cpp` | Implement type tracking |
| `evaluator/evaluator.cpp` | Init/assign/call/return type checks |
| `vm/chunk.h` | CompiledFunction: paramTypeChecks, returnTypeCheck |
| `vm/compiler.cpp` | Copy type info to CompiledFunction, encode in bytecode |
| `vm/vm.h` | globalTypes map, local type metadata |
| `vm/vm.cpp` | Type checks at define/set/call/return, improved error messages |
| `vm/vm_value.h` | tagToKorean helper |
| `object/object.h` or `util/` | typeToKorean, tokenTypeToObjectType helpers |
| `tests/vm_test.cpp` | Type error tests |
| `tests/repl_test.cpp` | Tree-walking type error tests |
