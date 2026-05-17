# hong-ik 거대 C++ 파일 분할 설계 (Phase 2-1: vm.cpp run() 메서드 추출)

- 작성일: 2026-05-17
- 대상: `hong-ik/vm/vm.cpp` (C++17/26 한글 인터프리터의 바이트코드 VM)
- 목적: `run()`의 거대 switch에서 opcode handler를 private 메서드로 추출 — **순수 함수 추출, 동작 보존**
- 로드맵 위치: 리팩토링 #2 Phase 2-1 (Phase 1 완료 후 후속)

## 1. 목표와 범위

### 목표
1. `vm.cpp`의 `run()` 메서드(약 830줄)를 ~50줄로 축소한다.
2. 각 opcode handler를 VM의 private 멤버 메서드로 추출해 인지 단위를 잘게 쪼갠다.
3. 추출 가치가 작은 매우 짧은 case(POP/DUP/NULL/TRUE/FALSE/CONSTANT/GET_LOCAL/SET_LOCAL)는 인라인 유지.

### 비목표 (이번 단계에서 하지 않음)
- 핸들러 본문 리팩토링 / 버그 수정 / 시그니처 변경 / 명령 의미 변경.
- vm.cpp 파일 분할 (Phase 2-2에서 진행).
- vm.h 인터페이스의 public/protected 변경 — private 영역만 늘림.
- `using namespace std;` 정리 (로드맵 #6).
- binaryOp/callValue/fillDefaults 등 이미 분리된 helper 변경.

## 2. 현재 상태 진단

### vm.cpp 구조

| 항목 | 현황 |
|------|------|
| 총 줄수 | 1,087 |
| `run()` 메서드 | line 240~1073, 약 830줄 |
| switch case 개수 | 33개 (case label 83개, binaryOp 묶음에 14개 통합) |
| 평균 case 본문 | 1줄(POP/DUP) ~ 100줄(OP_CALL) |

### 외부 helper (변경 없음)

- `callValue(VMValue callee, int argCount)` — 함수/메서드 호출
- `fillDefaults(CompiledFunction* fn, int argCount)` — 기본 매개변수 채우기
- `binaryOp(OpCode, left, right, line)` — 이항 산술/비교
- `push/pop/peek`, `readByte/readUint16`, `currentFrame`, `currentLine` — 인프라

### 회귀 안전망

- `tests/vm_test.cpp` (~25KB) — VM 단위 테스트
- `tests/repl_test.cpp` (~64KB) — end-to-end 회귀
- `tests/golden_test.cpp` + `tests/fixtures/golden/` — VM 포함 golden 출력
- Phase 1 baseline = **361 tests**, 전부 통과 필수.

## 3. 추출 대상 분류

### 추출 (~28개 private 메서드)

본문 5줄 이상으로 추출 가치가 명확한 case들:

| 그룹 | 메서드 | OpCode |
|------|--------|--------|
| 단항 | `opNegate`, `opNot` | NEGATE, NOT |
| 전역 변수 | `opGetGlobal`, `opSetGlobal`, `opDefineGlobal` | GET_GLOBAL, SET_GLOBAL, DEFINE_GLOBAL |
| 제어 흐름 | `opJump`, `opJumpIfFalse`, `opLoop` | JUMP, JUMP_IF_FALSE, LOOP |
| 호출/반환 | `opCall`, `opReturn` | CALL, RETURN |
| 컬렉션 생성 | `opBuildArray`, `opBuildHashMap`, `opBuildTuple` | BUILD_ARRAY, BUILD_HASHMAP, BUILD_TUPLE |
| 인덱싱 | `opIndexGet`, `opIndexSet`, `opSlice` | INDEX_GET, INDEX_SET, SLICE |
| 멤버/메서드 | `opGetMember`, `opSetMember`, `opInvoke` | GET_MEMBER, SET_MEMBER, INVOKE |
| 예외 | `opTryBegin`, `opTryEnd` | TRY_BEGIN, TRY_END |
| 반복자 | `opIterInit`, `opIterNext`, `opIterValue` | ITER_INIT, ITER_NEXT, ITER_VALUE |
| 클로저 | `opClosure`, `opGetUpvalue`, `opSetUpvalue` | CLOSURE, GET_UPVALUE, SET_UPVALUE |
| 검증 | `opRangeCheck`, `opTypeCheck` | RANGE_CHECK, TYPE_CHECK |
| 기타 | `opImport`, `opYield`, `opInterpolate` | IMPORT, YIELD, INTERPOLATE |

### 인라인 유지 (8 case)

본문이 4줄 이하라 추출 시 오히려 인지부담 증가:

- `OP_CONSTANT` (4줄)
- `OP_NULL` / `OP_TRUE` / `OP_FALSE` (각 1줄)
- `OP_POP` / `OP_DUP` (각 1줄)
- `OP_GET_LOCAL` / `OP_SET_LOCAL` (각 3줄)
- 이항 묶음(`OP_ADD`/`OP_SUB`/...) — 이미 `binaryOp()` 한 줄 호출

## 4. 메서드 시그니처 결정

대부분: `void opXxx()` — VM 멤버라 push/pop/peek/readByte/readUint16/currentFrame/currentLine 자유 사용.

### 제어 흐름이 run()에 영향 주는 케이스

| 메서드 | 반환 타입 | 의미 |
|--------|-----------|------|
| `opReturn` | `std::optional<std::shared_ptr<Object>>` | 값이 있으면 run() 종료, 빈 값이면 다음 iteration |
| `opCall` / `opInvoke` / `opImport` | `void` | frame push로 자연 처리, run()의 다음 iteration에서 `currentFrame()`이 새 프레임 반환 |
| `opYield`, 기타 모든 핸들러 | `void` | break로 다음 iteration |

run() 본문에서 사용 예:
```cpp
case OpCode::OP_RETURN: {
    if (auto result = opReturn()) return *result;
    break;
}
case OpCode::OP_YIELD: opYield(); break;
case OpCode::OP_CALL: opCall(); break;
```

### `frame` 참조 무효화 주의

`opCall`, `opReturn`, `opInvoke`, `opImport`는 `frames` 벡터를 변경(push/pop)하므로 직전 `auto& frame = currentFrame()` 참조가 무효화될 수 있다. **현재 run()도 이미 매 iteration 시작에서 `currentFrame()`을 다시 호출하는 패턴**이라 영향 없음. 핸들러 내부에서 frame이 필요하면 메서드 안에서 `currentFrame()`을 호출.

## 5. run() 본문 (After)

```cpp
shared_ptr<Object> VM::run() {
    while (true) {
        auto& frame = currentFrame();
        if (frame.ip >= frame.function->code.size()) break;
        OpCode instruction = static_cast<OpCode>(readByte());

        try {
            switch (instruction) {
            case OpCode::OP_CONSTANT: {
                uint16_t idx = readUint16();
                push(VMValue::fromObject(frame.function->constants[idx]));
                break;
            }
            case OpCode::OP_NULL: push(VMValue::Null()); break;
            case OpCode::OP_TRUE: push(VMValue::Bool(true)); break;
            case OpCode::OP_FALSE: push(VMValue::Bool(false)); break;

            case OpCode::OP_ADD: case OpCode::OP_SUB: ...
            case OpCode::OP_AND: case OpCode::OP_OR: {
                auto right = pop();
                auto left = pop();
                push(binaryOp(instruction, left, right, currentLine()));
                break;
            }

            case OpCode::OP_NEGATE: opNegate(); break;
            case OpCode::OP_NOT: opNot(); break;

            case OpCode::OP_GET_LOCAL: {
                uint16_t slot = readUint16();
                push(stack[frame.slotOffset + slot]);
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint16_t slot = readUint16();
                stack[frame.slotOffset + slot] = peek(0);
                break;
            }
            case OpCode::OP_GET_GLOBAL: opGetGlobal(); break;
            case OpCode::OP_SET_GLOBAL: opSetGlobal(); break;
            case OpCode::OP_DEFINE_GLOBAL: opDefineGlobal(); break;

            case OpCode::OP_JUMP: opJump(); break;
            case OpCode::OP_JUMP_IF_FALSE: opJumpIfFalse(); break;
            case OpCode::OP_LOOP: opLoop(); break;

            case OpCode::OP_CALL: opCall(); break;
            case OpCode::OP_RETURN: {
                if (auto result = opReturn()) return *result;
                break;
            }

            case OpCode::OP_BUILD_ARRAY: opBuildArray(); break;
            case OpCode::OP_BUILD_HASHMAP: opBuildHashMap(); break;
            case OpCode::OP_BUILD_TUPLE: opBuildTuple(); break;

            case OpCode::OP_INDEX_GET: opIndexGet(); break;
            case OpCode::OP_INDEX_SET: opIndexSet(); break;
            case OpCode::OP_SLICE: opSlice(); break;

            case OpCode::OP_GET_MEMBER: opGetMember(); break;
            case OpCode::OP_SET_MEMBER: opSetMember(); break;
            case OpCode::OP_INVOKE: opInvoke(); break;

            case OpCode::OP_TRY_BEGIN: opTryBegin(); break;
            case OpCode::OP_TRY_END: opTryEnd(); break;

            case OpCode::OP_ITER_INIT: opIterInit(); break;
            case OpCode::OP_ITER_NEXT: opIterNext(); break;
            case OpCode::OP_ITER_VALUE: opIterValue(); break;

            case OpCode::OP_CLOSURE: opClosure(); break;
            case OpCode::OP_GET_UPVALUE: opGetUpvalue(); break;
            case OpCode::OP_SET_UPVALUE: opSetUpvalue(); break;

            case OpCode::OP_POP: pop(); break;
            case OpCode::OP_DUP: push(peek(0)); break;

            case OpCode::OP_RANGE_CHECK: opRangeCheck(); break;
            case OpCode::OP_TYPE_CHECK: opTypeCheck(); break;
            case OpCode::OP_IMPORT: opImport(); break;
            case OpCode::OP_YIELD: opYield(); break;
            case OpCode::OP_INTERPOLATE: opInterpolate(); break;
            }
        } catch (const RuntimeException& e) {
            // 기존 예외 처리 로직 그대로
        }
    }
    return nullptr;
}
```

목표 줄수: **~80줄** (현재 830줄의 10% 수준).

## 6. vm.h 변경

`private:` 영역 끝에 ~28개 메서드 선언 추가. 기존 helper(`callValue`, `binaryOp`, `fillDefaults` 등) 아래에 배치.

```cpp
private:
    // ... 기존 필드, helper ...

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

`#include <optional>` 추가.

vm.h: 102줄 → ~140줄.

## 7. 검증 절차

각 그룹 추출 후 빌드/테스트 게이트 (Phase 1과 동일 패턴):

```bash
cmake --build cmake-build-debug 2>&1 | tee build.log
ctest --test-dir cmake-build-debug --output-on-failure
```

**합격 기준:**
- 361 tests 100% 통과
- 새 warning 0건
- vm.cpp의 줄수는 거의 그대로(±10줄). run() 자체만 ~80줄로 축소.

추가 회귀 필터:
```bash
cmake-build-debug/HongIkTests --gtest_filter="VMTest.*:*VM/*"
```

## 8. 위험 분석과 롤백

| 위험 | 가능성 | 대응 |
|------|--------|------|
| 메서드 추출 중 변수 스코프 오류 (예: `frame` 참조가 stale) | 중 | 빌드 에러 즉시 발생. 핸들러 내부에서 `currentFrame()` 재호출 |
| `opReturn`의 optional 처리 누락 | 낮 | 회귀 테스트로 즉시 발견 (return 못 하면 무한 루프) |
| `opYield`의 yield buffer 종료 신호 변경 | 낮 | 제너레이터 테스트(`tests/vm_test.cpp`의 GeneratorTest)가 회귀 망 |
| try/catch 블록 의미 변경 | 매우 낮 | run()의 try/catch는 유지. 핸들러는 그냥 throw만 |
| frame iterator invalidation | 낮 | 현재도 매 iteration `currentFrame()` 재호출. 변경 없음 |
| 그룹 추출 순서 실수로 한 case 누락 | 낮 | grep으로 `^\s*case OpCode::` 33개 모두 처리되었는지 검증 |

**롤백 계획:** 그룹별로 single commit. 회귀 시 `git revert <commit>` 한 줄.

## 9. 단계 완료 정의 (Definition of Done)

- [ ] vm.h에 28개 op 핸들러 선언 추가, `#include <optional>` 추가
- [ ] vm.cpp run() 메서드가 ~80줄로 축소
- [ ] 추출된 28개 메서드가 `VM::opXxx` 형태로 vm.cpp에 정의
- [ ] 인라인 유지 case(CONSTANT/NULL/TRUE/FALSE/POP/DUP/GET_LOCAL/SET_LOCAL/binaryOp 묶음) 그대로
- [ ] `cmake --build cmake-build-debug` 성공
- [ ] `ctest --test-dir cmake-build-debug` 361/361 통과
- [ ] 새 warning 0건
- [ ] 단일 commit (메시지: `refactor(vm): extract opcode handlers from run()`)
- [ ] 메모리 인덱스에 Phase 2-1 완료 + Phase 2-2 시작점 기록

## 10. 후속 단계 미리보기 (이번 설계서 범위 밖)

**Phase 2-2:** 추출된 28개 메서드 정의를 `vm/handlers/{arith,var,control,call,collection,index,member,exception,iter,closure,misc}.cpp` 그룹별 파일로 이동. vm.cpp ~350줄로 축소. vm.h 변경 없음.

**Phase 3 (vm/compiler.cpp):** Phase 1과 동일 패턴 — 메서드 이미 분리됨, 파일만 `compiler_stmt.cpp` + `compiler_expr.cpp`로 분할.

**Phase 4 (evaluator/evaluator.cpp):** 가장 위험. `eval()`의 dynamic_cast chain을 helper로 1차 추출 후 `evaluator_stmt.cpp` + `evaluator_expr.cpp` 분리.
