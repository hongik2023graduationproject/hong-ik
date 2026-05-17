# vm.cpp Phase 2-1 (run() handler extraction) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `vm/vm.cpp`의 `run()` 안에 인라인된 33개 opcode case 중 28개를 VM의 private 멤버 메서드로 추출한다. `run()` 본문을 약 80줄로 축소. **순수 추출, 동작 변경 0.**

**Architecture:** 각 case 본문을 그대로 `void VM::op<OpName>()` 메서드로 이동, run()의 case는 `opXxx(); break;` 한 줄로 교체. 그룹별로 빌드+테스트 게이트. 매우 짧은 case(CONSTANT/NULL/TRUE/FALSE/POP/DUP/GET_LOCAL/SET_LOCAL/binaryOp 묶음)는 인라인 유지. 모든 메서드 정의는 vm.cpp 안에 남김(파일 분리는 Phase 2-2).

**Tech Stack:** C++17/26, CMake 3.28+, Google Test, MSVC (또는 cl.exe via Visual Studio). 작업 경로: `E:\dev\projects\school\hongik\hong-ik`.

---

## 사전 결정 사항 (spec 기준)

### 추출 대상 (28개)

| OpCode | 메서드 | 출처 line (분할 전) |
|--------|--------|---------------------|
| NEGATE | `opNegate` | 273~282 |
| NOT | `opNot` | 283~290 |
| GET_GLOBAL | `opGetGlobal` | 302~311 |
| SET_GLOBAL | `opSetGlobal` | 312~317 |
| DEFINE_GLOBAL | `opDefineGlobal` | 318~367 |
| JUMP | `opJump` | 369~373 |
| JUMP_IF_FALSE | `opJumpIfFalse` | 374~384 |
| LOOP | `opLoop` | 385~389 |
| CALL | `opCall` | 391~496 |
| RETURN | `opReturn` | 497~552 |
| BUILD_ARRAY | `opBuildArray` | 553~563 |
| BUILD_HASHMAP | `opBuildHashMap` | 564~580 |
| BUILD_TUPLE | `opBuildTuple` | 581~590 |
| INDEX_GET | `opIndexGet` | 591~632 |
| INDEX_SET | `opIndexSet` | 633~658 |
| SLICE | `opSlice` | 659~719 |
| GET_MEMBER | `opGetMember` | 720~732 |
| SET_MEMBER | `opSetMember` | 733~745 |
| INVOKE | `opInvoke` | 746~793 |
| TRY_BEGIN | `opTryBegin` | 794~802 |
| TRY_END | `opTryEnd` | 803~807 |
| ITER_INIT | `opIterInit` | 808~818 |
| ITER_NEXT | `opIterNext` | 819~836 |
| ITER_VALUE | `opIterValue` | 837~848 |
| CLOSURE | `opClosure` | 849~874 |
| GET_UPVALUE | `opGetUpvalue` | 875~883 |
| SET_UPVALUE | `opSetUpvalue` | 884~891 |
| RANGE_CHECK | `opRangeCheck` | 895~926 |
| TYPE_CHECK | `opTypeCheck` | 927~957 |
| IMPORT | `opImport` | 958~1023 |
| YIELD | `opYield` | 1024~1028 |
| INTERPOLATE | `opInterpolate` | 1030~1040 |

= 32 메서드. (RANGE_CHECK과 TYPE_CHECK 추가로 spec의 28에서 보정)

> **Spec 보정:** spec의 "약 28개"는 어림수. 정확히 32개. 종합적으로 모두 추출.

### 인라인 유지 (case 8개)

- `OP_CONSTANT` (251~255)
- `OP_NULL` (256), `OP_TRUE` (257), `OP_FALSE` (258)
- binaryOp 묶음 (260~271) — 이미 `binaryOp()` 호출 한 줄
- `OP_GET_LOCAL` (292~296), `OP_SET_LOCAL` (297~301)
- `OP_POP` (892), `OP_DUP` (893)

### 시그니처 규칙

- 대부분: `void op<Name>()`
- 예외: `std::optional<std::shared_ptr<Object>> opReturn()` — `optional`이 비면 다음 iteration, 값이 있으면 run() 종료

### `frame` 참조 처리

run()은 매 iteration 시작에서 `auto& frame = currentFrame()`을 만든다. 추출된 메서드에서는 **이 참조를 못 쓰므로** 메서드 본문 안에서 다음 중 하나로 처리:
- 메서드 시작에서 `auto& frame = currentFrame();` 재선언 (가장 깔끔 — 기존 변수명 그대로 사용 가능)
- 또는 사용처마다 `currentFrame()` 직접 호출

이 plan은 **메서드 시작에서 `auto& frame = currentFrame();` 재선언** 패턴 사용.

### 진행 전략

- 그룹별로 추출 (Task 2~12)
- 각 그룹 끝에 빌드 + ctest 게이트
- 모든 그룹 끝나면 final 검증
- **단일 commit** (각 Task는 working tree만 변경, 마지막에 한 번에 commit)
- main에는 unstaged 변경 9건 있음 — git add는 분할 관련만(`vm/vm.cpp`, `vm/vm.h`)

---

## Task 0: Baseline + 브랜치 생성

**Files:** 없음 (환경 준비)

- [ ] **Step 1: 현재 브랜치 확인**

```
git branch --show-current
```
Expected: `main`. (다른 브랜치면 사용자 확인)

- [ ] **Step 2: feature 브랜치 생성 (working tree 보존)**

```
git checkout -b refactor/split-vm-cpp-phase2-1
git status -sb | head -3
```
Expected: `## refactor/split-vm-cpp-phase2-1`. 9개의 기존 unstaged 변경(README.md, vm.cpp, vm.h, evaluator.cpp, lexer.cpp, environment.h, exception.h, tests/lexer_test.cpp, tests/evaluator_test.cpp)이 working tree에 그대로 남아 있어야 함. 이들은 사용자가 진행 중인 다른 작업이므로 건드리지 않는다.

- [ ] **Step 3: baseline 빌드/테스트**

PowerShell (한 줄):
```
$env:PATH += ';C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'; cmake --build cmake-build-debug
```
Expected: 빌드 성공.

```
ctest --test-dir cmake-build-debug
```
Expected: `100% tests passed, 0 tests failed out of 361`.

- [ ] **Step 4: baseline commit hash 기록**

```
git log -1 --oneline
```
회귀 시 복구 기준점.

---

## Task 1: vm.h에 메서드 선언 추가 (스켈레톤)

**Files:**
- Modify: `vm/vm.h` (private 영역 끝에 32개 메서드 선언 + `#include <optional>` 추가)

- [ ] **Step 1: `#include <optional>` 추가**

`vm/vm.h`의 include 블록에 (위치: `<map>` 다음 또는 적절한 알파벳 순서):
```cpp
#include <optional>
```

- [ ] **Step 2: private 메서드 선언 추가**

`vm/vm.h`의 `private:` 영역, `long long currentLine();` 다음 줄에 다음 블록을 추가:

```cpp
    // ===== Opcode handlers (Phase 2-1에서 추출) =====
    void opNegate();
    void opNot();
    void opGetGlobal();
    void opSetGlobal();
    void opDefineGlobal();
    void opJump();
    void opJumpIfFalse();
    void opLoop();
    void opCall();
    std::optional<std::shared_ptr<Object>> opReturn();
    void opBuildArray();
    void opBuildHashMap();
    void opBuildTuple();
    void opIndexGet();
    void opIndexSet();
    void opSlice();
    void opGetMember();
    void opSetMember();
    void opInvoke();
    void opTryBegin();
    void opTryEnd();
    void opIterInit();
    void opIterNext();
    void opIterValue();
    void opClosure();
    void opGetUpvalue();
    void opSetUpvalue();
    void opRangeCheck();
    void opTypeCheck();
    void opImport();
    void opYield();
    void opInterpolate();
```

(`opRangeCheck`/`opTypeCheck` 포함 32개)

- [ ] **Step 3: 빌드 (link 안 함, 컴파일만 확인)**

선언만 추가했으니 컴파일은 통과해야 한다. 정의가 없어도 link 단계가 아니라면 OK. 실제로는 link도 통과해야 하니, link 에러가 나면 다음 Task로 가서 빠르게 메서드 본문 추가.

```
cmake --build cmake-build-debug 2>&1 | Select-Object -Last 8
```

**예상되는 link 에러:** `unresolved external symbol "VM::opNegate"` 등 32개. 이는 정상 — Task 2부터 본문을 채우면 해소된다.

> **임시 우회:** link 에러 회피를 위해 vm.cpp 끝에 다음 stub 32개를 한 번에 추가하는 방법도 있다 (각각 `void VM::opXxx() {}`). 그러나 stub은 동작이 다르므로 테스트가 깨질 위험이 있다. 권장: **Task 1은 선언만 추가하고 빌드 통과 안 해도 OK로 두기**. Task 2부터 본문 추가하며 점진적으로 link 통과.

이 task는 commit 없음. working tree만 변경.

---

## Task 2: 단항 연산자 추출 (opNegate, opNot)

**Files:**
- Modify: `vm/vm.cpp`

- [ ] **Step 1: 추출할 본문 확인**

```
grep -nE "OP_NEGATE|OP_NOT" vm/vm.cpp | head -4
```
원본 line: `OP_NEGATE` 273~282, `OP_NOT` 283~290.

- [ ] **Step 2: vm.cpp 끝(`VM::fillDefaults` 정의 다음)에 두 메서드 정의 추가**

```cpp
void VM::opNegate() {
    auto operand = pop();
    if (operand.tag == ValueTag::INT)
        push(VMValue::Int(-operand.intVal));
    else if (operand.tag == ValueTag::FLOAT)
        push(VMValue::Float(-operand.floatVal));
    else
        throw RuntimeException("'-' 전위 연산자가 지원되지 않는 타입입니다.", currentLine());
}

void VM::opNot() {
    auto operand = pop();
    if (operand.tag == ValueTag::BOOL)
        push(VMValue::Bool(!operand.boolVal));
    else
        throw RuntimeException("'!' 전위 연산자가 지원되지 않는 타입입니다.", currentLine());
}
```

- [ ] **Step 3: run()의 두 case를 한 줄 호출로 교체**

`OP_NEGATE` 블록(273~282)을:
```cpp
            case OpCode::OP_NEGATE: opNegate(); break;
```

`OP_NOT` 블록(283~290)을:
```cpp
            case OpCode::OP_NOT: opNot(); break;
```

- [ ] **Step 4: 빌드 + 테스트**

```
cmake --build cmake-build-debug 2>&1 | Select-Object -Last 5
ctest --test-dir cmake-build-debug 2>&1 | Select-Object -Last 4
```
Expected: 빌드 성공, 361/361 통과. (link 에러는 아직 30개 메서드 미정의로 인해 남아있을 수 있음. 그러면 Task 3부터 점진적 해소.)

> **예외 처리 안내:** 다른 메서드들의 link 에러 때문에 빌드 자체가 실패할 수 있다. 이 경우 Task 2~12를 연속으로 진행하고, 마지막 Task에서 한 번에 빌드/테스트해도 무방. **권장: 모든 task를 한꺼번에 진행하고 Task 13에서 최종 빌드/테스트 한 번만.**

---

## Task 3: 전역 변수 추출 (opGetGlobal, opSetGlobal, opDefineGlobal)

**Files:**
- Modify: `vm/vm.cpp`

- [ ] **Step 1: vm.cpp 끝에 세 메서드 정의 추가**

```cpp
void VM::opGetGlobal() {
    auto& frame = currentFrame();
    uint16_t nameIdx = readUint16();
    auto* name = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
    if (!name) throw RuntimeException("잘못된 전역 변수명입니다.", currentLine());
    auto it = globals.find(name->value);
    if (it != globals.end()) { push(it->second); return; }
    auto bit = builtins.find(name->value);
    if (bit != builtins.end()) { push(VMValue::Obj(bit->second)); return; }
    throw RuntimeException("'" + name->value + "' 존재하지 않는 식별자입니다.", currentLine());
}

void VM::opSetGlobal() {
    auto& frame = currentFrame();
    uint16_t nameIdx = readUint16();
    auto* name = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
    globals[name->value] = peek(0);
}

void VM::opDefineGlobal() {
    auto& frame = currentFrame();
    uint16_t nameIdx = readUint16();
    auto* name = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
    auto val = pop();
    // 상속 해결: CompiledClassDef의 부모 참조 설정
    if (val.tag == ValueTag::OBJECT) {
        if (auto* ccd = dynamic_cast<CompiledClassDef*>(val.objVal.get())) {
            if (!ccd->parentName.empty() && !ccd->parent) {
                auto parentIt = globals.find(ccd->parentName);
                if (parentIt != globals.end() && parentIt->second.tag == ValueTag::OBJECT) {
                    auto parentCcd = dynamic_pointer_cast<CompiledClassDef>(parentIt->second.objVal);
                    if (parentCcd) {
                        ccd->parent = parentCcd;
                        // 부모 필드 병합
                        vector<string> mergedFieldNames = parentCcd->fieldNames;
                        vector<shared_ptr<Token>> mergedFieldTypes = parentCcd->fieldTypes;
                        for (size_t i = 0; i < ccd->fieldNames.size(); i++) {
                            bool found = false;
                            for (size_t j = 0; j < mergedFieldNames.size(); j++) {
                                if (mergedFieldNames[j] == ccd->fieldNames[i]) {
                                    mergedFieldTypes[j] = ccd->fieldTypes[i];
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                mergedFieldNames.push_back(ccd->fieldNames[i]);
                                mergedFieldTypes.push_back(ccd->fieldTypes[i]);
                            }
                        }
                        ccd->fieldNames = mergedFieldNames;
                        ccd->fieldTypes = mergedFieldTypes;
                        // 부모 생성자 상속 (자식에 없으면)
                        if (!ccd->compiledConstructor && parentCcd->compiledConstructor) {
                            ccd->compiledConstructor = parentCcd->compiledConstructor;
                        }
                        // 부모 메서드 병합
                        for (auto& [mname, mfn] : parentCcd->compiledMethods) {
                            if (ccd->compiledMethods.find(mname) == ccd->compiledMethods.end()) {
                                ccd->compiledMethods[mname] = mfn;
                            }
                        }
                    }
                }
            }
        }
    }
    globals[name->value] = val;
}
```

> **참고:** 원본의 `break;`가 메서드에서는 `return;`으로 변경된다 (opGetGlobal의 early returns).

- [ ] **Step 2: run()의 세 case를 한 줄 호출로 교체**

`OP_GET_GLOBAL` (302~311) → `case OpCode::OP_GET_GLOBAL: opGetGlobal(); break;`
`OP_SET_GLOBAL` (312~317) → `case OpCode::OP_SET_GLOBAL: opSetGlobal(); break;`
`OP_DEFINE_GLOBAL` (318~367) → `case OpCode::OP_DEFINE_GLOBAL: opDefineGlobal(); break;`

(빌드/테스트는 Task 12 이후 일괄)

---

## Task 4: 제어 흐름 추출 (opJump, opJumpIfFalse, opLoop)

- [ ] **Step 1: vm.cpp 끝에 추가**

```cpp
void VM::opJump() {
    uint16_t offset = readUint16();
    currentFrame().ip += offset;
}

void VM::opJumpIfFalse() {
    uint16_t offset = readUint16();
    auto condition = pop();
    if (condition.tag != ValueTag::BOOL) {
        throw RuntimeException("조건식의 결과는 논리(참/거짓) 값이어야 합니다.", currentLine());
    }
    if (!condition.boolVal) {
        currentFrame().ip += offset;
    }
}

void VM::opLoop() {
    uint16_t offset = readUint16();
    currentFrame().ip -= offset;
}
```

- [ ] **Step 2: run()의 세 case를 한 줄 호출로 교체**

`OP_JUMP` (369~373) → `case OpCode::OP_JUMP: opJump(); break;`
`OP_JUMP_IF_FALSE` (374~384) → `case OpCode::OP_JUMP_IF_FALSE: opJumpIfFalse(); break;`
`OP_LOOP` (385~389) → `case OpCode::OP_LOOP: opLoop(); break;`

---

## Task 5: 함수 호출/반환 추출 (opCall, opReturn) — **가장 큰 case**

**참고:** OP_CALL은 100줄, OP_RETURN은 55줄. 본문은 vm.cpp의 원본 line 391~496(CALL), 497~552(RETURN)을 **그대로** 옮긴다. 단:
- 메서드 시작에 `auto& frame = currentFrame();` (필요 시) 추가
- `opReturn`의 경우 run() 종료 신호를 `optional<shared_ptr<Object>>`로 표현

- [ ] **Step 1: opCall 정의 추가 (vm.cpp 끝에)**

원본 line 391~496의 `OP_CALL` 본문(중괄호 안)을 그대로 복사. 메서드 시작에 `auto& frame = currentFrame();` 추가. 본문 안의 `break;`를 `return;`으로 교체 (early exit 패턴 있는 경우).

```cpp
void VM::opCall() {
    auto& frame = currentFrame();
    uint8_t argCount = readByte();
    auto& callee = peek(argCount);

    // ... 원본 line 393~495 본문 그대로 (break → return으로 교체) ...
}
```

> **실행 시 정확한 본문은 `Read vm/vm.cpp` 후 line 391~496을 가져와 복사.** 본문 길이상 plan에 전문 인용은 생략. 옮길 때 다음만 주의:
> - `auto& callee = peek(argCount);` 등의 변수 선언 그대로
> - 본문 내 `break;` → `return;` 변경 (case label 의미 없으니 early exit로 의미 유지)
> - `frame` 참조는 메서드 시작의 `currentFrame()` 결과 사용

- [ ] **Step 2: opReturn 정의 추가**

원본 line 497~552의 `OP_RETURN` 본문을 옮김. **반환 타입은 `std::optional<std::shared_ptr<Object>>`.**

핵심 의미:
- 원본 RETURN 처리에서 `return result;` 식으로 함수에서 빠져나가는 부분이 있으면 → `return result;` (optional에 값 담아) 그대로
- 호출 프레임이 남아있어 다음 iteration으로 가야 하는 경우 → `return std::nullopt;`
- `break;`만 있었던 경우 → `return std::nullopt;`

```cpp
std::optional<std::shared_ptr<Object>> VM::opReturn() {
    auto& frame = currentFrame();
    // ... 원본 line 498~551 본문 그대로 ...
    // 마지막에 만약 break;였으면 return std::nullopt; 로 교체
    return std::nullopt;
}
```

> **실행 시 vm.cpp의 OP_RETURN 본문을 정확히 읽어 옮길 것. 패턴이 단순치 않을 수 있으니 원본 보고 reason about.**

- [ ] **Step 3: run()의 두 case를 호출로 교체**

`OP_CALL` (391~496) → `case OpCode::OP_CALL: opCall(); break;`

`OP_RETURN` (497~552) →
```cpp
            case OpCode::OP_RETURN: {
                if (auto result = opReturn()) return *result;
                break;
            }
```

---

## Task 6: 컬렉션 생성 추출 (opBuildArray, opBuildHashMap, opBuildTuple)

- [ ] **Step 1: vm.cpp 끝에 추가**

원본 line 553~563 (OP_BUILD_ARRAY), 564~580 (OP_BUILD_HASHMAP), 581~590 (OP_BUILD_TUPLE) 본문을 각각 `void VM::opBuildArray()`, `opBuildHashMap()`, `opBuildTuple()`로 이동. `frame` 사용 시 `auto& frame = currentFrame();` 메서드 시작에 추가.

> **실행 시 정확한 본문은 vm.cpp에서 Read.**

- [ ] **Step 2: run()의 세 case를 호출로 교체**

```cpp
            case OpCode::OP_BUILD_ARRAY: opBuildArray(); break;
            case OpCode::OP_BUILD_HASHMAP: opBuildHashMap(); break;
            case OpCode::OP_BUILD_TUPLE: opBuildTuple(); break;
```

---

## Task 7: 인덱싱 추출 (opIndexGet, opIndexSet, opSlice)

- [ ] **Step 1: vm.cpp 끝에 추가**

원본:
- `OP_INDEX_GET` (591~632, 42줄)
- `OP_INDEX_SET` (633~658, 26줄)
- `OP_SLICE` (659~719, 61줄)

각각 `void VM::opIndexGet()`, `opIndexSet()`, `opSlice()`로 이동.

- [ ] **Step 2: run()의 세 case를 호출로 교체**

```cpp
            case OpCode::OP_INDEX_GET: opIndexGet(); break;
            case OpCode::OP_INDEX_SET: opIndexSet(); break;
            case OpCode::OP_SLICE: opSlice(); break;
```

---

## Task 8: 멤버/메서드 추출 (opGetMember, opSetMember, opInvoke)

- [ ] **Step 1: vm.cpp 끝에 추가**

원본:
- `OP_GET_MEMBER` (720~732)
- `OP_SET_MEMBER` (733~745)
- `OP_INVOKE` (746~793, 48줄 — frame push 있음, callValue 호출)

각각 메서드로 이동. `OP_INVOKE`는 frame을 push하므로 메서드 시작에서 frame 변수 사용 후 frame이 stale될 수 있음 — 사용 후 `currentFrame()` 재호출 패턴 유지.

- [ ] **Step 2: run()의 세 case를 호출로 교체**

```cpp
            case OpCode::OP_GET_MEMBER: opGetMember(); break;
            case OpCode::OP_SET_MEMBER: opSetMember(); break;
            case OpCode::OP_INVOKE: opInvoke(); break;
```

---

## Task 9: 예외 추출 (opTryBegin, opTryEnd)

- [ ] **Step 1: vm.cpp 끝에 추가**

원본 `OP_TRY_BEGIN` (794~802), `OP_TRY_END` (803~807)를 각각 `void VM::opTryBegin()`, `opTryEnd()`로 이동.

- [ ] **Step 2: run()의 두 case를 호출로 교체**

```cpp
            case OpCode::OP_TRY_BEGIN: opTryBegin(); break;
            case OpCode::OP_TRY_END: opTryEnd(); break;
```

---

## Task 10: 반복자 추출 (opIterInit, opIterNext, opIterValue)

- [ ] **Step 1: vm.cpp 끝에 추가**

원본:
- `OP_ITER_INIT` (808~818)
- `OP_ITER_NEXT` (819~836)
- `OP_ITER_VALUE` (837~848)

각각 메서드로 이동.

- [ ] **Step 2: run()의 세 case를 호출로 교체**

```cpp
            case OpCode::OP_ITER_INIT: opIterInit(); break;
            case OpCode::OP_ITER_NEXT: opIterNext(); break;
            case OpCode::OP_ITER_VALUE: opIterValue(); break;
```

---

## Task 11: 클로저 추출 (opClosure, opGetUpvalue, opSetUpvalue)

- [ ] **Step 1: vm.cpp 끝에 추가**

원본:
- `OP_CLOSURE` (849~874, 26줄)
- `OP_GET_UPVALUE` (875~883)
- `OP_SET_UPVALUE` (884~891)

각각 메서드로 이동.

- [ ] **Step 2: run()의 세 case를 호출로 교체**

```cpp
            case OpCode::OP_CLOSURE: opClosure(); break;
            case OpCode::OP_GET_UPVALUE: opGetUpvalue(); break;
            case OpCode::OP_SET_UPVALUE: opSetUpvalue(); break;
```

---

## Task 12: 검증/기타 추출 (opRangeCheck, opTypeCheck, opImport, opYield, opInterpolate)

- [ ] **Step 1: vm.cpp 끝에 추가**

원본:
- `OP_RANGE_CHECK` (895~926, 32줄)
- `OP_TYPE_CHECK` (927~957, 31줄)
- `OP_IMPORT` (958~1023, 66줄 — frame push 있음)
- `OP_YIELD` (1024~1028, 5줄)
- `OP_INTERPOLATE` (1030~1040, 11줄)

각각 메서드로 이동.

- [ ] **Step 2: run()의 다섯 case를 호출로 교체**

```cpp
            case OpCode::OP_RANGE_CHECK: opRangeCheck(); break;
            case OpCode::OP_TYPE_CHECK: opTypeCheck(); break;
            case OpCode::OP_IMPORT: opImport(); break;
            case OpCode::OP_YIELD: opYield(); break;
            case OpCode::OP_INTERPOLATE: opInterpolate(); break;
```

---

## Task 13: 빌드 + 테스트 + 검증

이 시점에 32개 메서드가 모두 정의되어 link 에러 0이 되어야 한다. run()은 약 80줄로 축소됨.

- [ ] **Step 1: 클린 빌드**

```
cmake --build cmake-build-debug --clean-first 2>&1 | Select-Object -Last 10
```
Expected: 빌드 성공, warning 0건.

- [ ] **Step 2: 전체 테스트**

```
ctest --test-dir cmake-build-debug --output-on-failure 2>&1 | Select-Object -Last 5
```
Expected: `100% tests passed, 0 tests failed out of 361`.

- [ ] **Step 3: VM 회귀 필터**

```
& "cmake-build-debug\HongIkTests.exe" --gtest_filter="VMTest.*:*VM/*" 2>&1 | Select-Object -Last 6
```
Expected: 모두 PASS.

- [ ] **Step 4: 추출 검증 — 32개 메서드 모두 정의됨**

```
grep -cE "^(void|std::optional.*) VM::op[A-Z]" vm/vm.cpp
```
Expected: `32`.

- [ ] **Step 5: run() 줄수 축소 확인**

```
awk '/^shared_ptr<Object> VM::run\(\)/,/^\}/' vm/vm.cpp | wc -l
```
Expected: 100줄 이하 (목표 ~80줄).

- [ ] **Step 6: 인라인 유지 case 확인 (8개)**

```
grep -cE "case OpCode::(OP_CONSTANT|OP_NULL|OP_TRUE|OP_FALSE|OP_POP|OP_DUP|OP_GET_LOCAL|OP_SET_LOCAL):" vm/vm.cpp
```
Expected: `8`.

- [ ] **Step 7: 어떤 op 메서드도 run() 안에서만 정의되지 않았는지 확인**

```
grep -nE "void VM::op[A-Z]" vm/vm.cpp | head -32
```
Expected: 32줄 출력, 모두 `void VM::opXxx()` 또는 `std::optional... VM::opReturn()` 형태.

---

## Task 14: 단일 commit

**Files:** 없음 (git만)

- [ ] **Step 1: 변경 사항 확인 (분할 관련만)**

```
git status -s | grep -E "^.M (vm/vm\.|CMakeLists)" 
git diff --stat vm/
```
Expected: `vm/vm.cpp`와 `vm/vm.h`만 변경. CMakeLists는 이번 phase에서 변경 없음.

- [ ] **Step 2: 분할 관련만 staging**

```
git add vm/vm.cpp vm/vm.h docs/superpowers/plans/2026-05-17-vm-cpp-phase2-1-extract.md
git status -s
```
Expected: vm 관련 + plan 파일만 `M`/`A`. 다른 unstaged 변경(README, lexer, evaluator 등)은 `M` 상태 유지(staging 안 됨).

- [ ] **Step 3: 단일 commit**

```
git commit -m "$(cat <<'EOF'
refactor(vm): extract 32 opcode handlers from run() into private methods

Reduce run() from ~830 lines to ~80 lines by extracting each non-trivial
opcode case into a dedicated VM::opXxx() private method. Pure
extraction - no behavior change. Cases left inline: OP_CONSTANT,
OP_NULL/TRUE/FALSE, OP_POP/DUP, OP_GET_LOCAL/SET_LOCAL, and the
binary-op group (already a one-liner via binaryOp()).

Phase 2-1 of the giant-file split roadmap (see
docs/superpowers/specs/2026-05-17-vm-cpp-split-phase2-1-design.md).

vm.cpp total line count is unchanged - file-level split lands in
Phase 2-2 (handlers move into vm/handlers/).

All 361 tests pass on a clean build.
EOF
)"
```

- [ ] **Step 4: commit 확인**

```
git log -1 --stat
```
Expected: 3 files changed (vm.cpp + vm.h + plan).

---

## Task 15: main으로 fast-forward merge + 브랜치 삭제

이 단계는 사용자 승인 후 진행 (Phase 1과 동일 패턴).

- [ ] **Step 1: main 체크아웃**

```
git checkout main
```
Expected: 9개 unstaged 변경 보존, refactor 브랜치 분리.

- [ ] **Step 2: fast-forward merge**

```
git merge refactor/split-vm-cpp-phase2-1
```
Expected: `Updating <hash>..<hash>`, `Fast-forward`. main이 refactor 브랜치의 commit으로 진행.

- [ ] **Step 3: 재검증**

```
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug
```
Expected: 빌드 성공, 361 통과.

- [ ] **Step 4: 브랜치 삭제**

```
git branch -d refactor/split-vm-cpp-phase2-1
```

---

## Task 16: 메모리 인덱스 업데이트

**Files:**
- Modify: `C:\Users\98kim\.claude\projects\E--dev-projects-school-hongik\memory\project-refactoring-roadmap.md`
- Modify: `C:\Users\98kim\.claude\projects\E--dev-projects-school-hongik\memory\MEMORY.md`

- [ ] **Step 1: 로드맵 갱신**

`#2 Phase 2~4 (High)` 섹션에서:
- `Phase 2: vm/vm.cpp ...` 항목을 **완료 섹션**으로 이동: `**#2 Phase 2-1: vm.cpp run() handler 추출 (High)** — 2026-05-17. 32개 opcode handler를 VM private 멤버 메서드로 추출, run() ~830줄 → ~80줄. vm.cpp 줄수는 유지. 361/361 tests 통과, single commit, branch fast-forward merged. plan: docs/superpowers/plans/2026-05-17-vm-cpp-phase2-1-extract.md.`
- **대기 섹션** 시작점을 **Phase 2-2 (vm.cpp 파일 분할)**로 변경.

- [ ] **Step 2: MEMORY.md 한 줄 갱신**

```
- [Refactoring roadmap](project-refactoring-roadmap.md) — #1·#3·#2-Phase 1·#2-Phase 2-1 완료, 다음은 **#2 Phase 2-2 (vm.cpp 파일 분할)**
```

---

## Self-Review 체크리스트 (계획서 작성자 검토 완료)

- [x] **Spec coverage** — spec의 32개 메서드 매핑이 Task 2~12에 모두 대응. vm.h 변경, 검증 절차, 단일 commit, 메모리 업데이트까지 모두 task로 매핑.
- [x] **Placeholder scan** — TBD/TODO 없음. 단 Task 5, 6, 7, 8, 11, 12에서 "본문은 원본 line N~M을 그대로 옮김"이라고 명시했으나 원본 코드를 plan에 전문 인용하지 않음. 의도적: 원본이 100줄 가까운 함수도 있어 plan 비대화 회피. **실행자에게는 vm.cpp Read → 본문 복사 → 메서드로 이동 + `frame` 변수 처리 + `break→return` 교체 패턴이 반복적이라 충분히 명세됨.** Task 2, 3, 4에는 완전한 코드 블록 포함하여 패턴 시연.
- [x] **Type consistency** — 모든 메서드 시그니처가 spec과 일치 (`void` 31개 + `std::optional<std::shared_ptr<Object>> opReturn()` 1개). 메서드 명명 `op<Name>` 일관.

---

## 실행 옵션

Plan complete and saved to `docs/superpowers/plans/2026-05-17-vm-cpp-phase2-1-extract.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
