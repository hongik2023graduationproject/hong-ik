# VM 최적화 사이클 1 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 빌트인·컬렉션 경계의 불필요한 힙 할당/복사 제거 + peephole 융합 패스로 VM 실행 성능을 개선하고, `compare.py`로 베이스라인 대비 효과를 수치 증명한다.

**Architecture:** (1) `Builtin::function` 인자를 const 참조로 바꿔 호출 복사 제거, VM 빌트인 경로에 재사용 버퍼·`길이` fast path 도입. (2) `OP_INDEX_GET/SET`에서 스칼라 인덱스의 `toObject()` 힙 할당 제거. (3) `vm/peephole.{h,cpp}` 신설 — decode → 점프 타깃 수집 → `SET_* + OP_POP` 융합(타깃 가드) → 오프셋·lines 재조립.

**Tech Stack:** C++ (core는 Emscripten C++20 호환 필수), CMake, Google Test, Google Benchmark.

**Spec:** `docs/superpowers/specs/2026-07-10-vm-optimization-cycle1-design.md`

## Global Constraints

- 브랜치: `feature/vm-opt-cycle1` (이미 생성됨). **푸시 금지** — 사용자 확인 후에만.
- 커밋 푸터(빈 줄 후): `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- `vm/peephole.cpp`는 `HONGIK_CORE_SOURCES`에 들어가 **Emscripten C++20으로도 컴파일**된다 — C++26 전용 기능 금지.
- CMakeLists의 MSVC `/utf-8` `add_compile_options`는 모든 타깃 정의보다 앞에 있어야 한다 — 위치를 옮기지 말 것.
- 새 파일 이름은 ASCII. 파일 내용의 한국어(주석·에러 메시지)는 그대로 사용.
- .hik 문법을 추측하지 말 것 — 기존 테스트(`tests/*.cpp`)와 `benchmarks/scenarios/*.hik`에 등장하는 구문만 사용.
- 빌드/테스트 도구 (Git Bash 경로):
  - cmake: `"/c/Program Files/JetBrains/CLion 2025.2.2/bin/cmake/win/x64/bin/cmake"`
  - ctest: `"/c/Program Files/JetBrains/CLion 2025.2.2/bin/cmake/win/x64/bin/ctest"`
  - clang-format: `"/c/Program Files/JetBrains/CLion 2025.2.2/bin/clang/win/x64/bin/clang-format.exe" -i <files>` (커밋 전 새/수정 파일에 실행)
- 전체 테스트: `cd hong-ik && cmake --build cmake-build-debug --target HongIkTests && "…/ctest" --test-dir cmake-build-debug --output-on-failure`
- 에러 메시지 문자열은 **바이트 단위로 기존과 동일**해야 한다 (백엔드 parity 테스트가 잠금).

---

### Task 1: Builtin::function 시그니처 by-value → const& (spec D1)

**Files:**
- Modify: `object/object.h:174` (순수 가상 선언)
- Modify: `object/built_in.h` (오버라이드 선언 45곳)
- Modify: `object/builtins/{math,string,array,collection,io,conversion,json}.cpp` (정의부)

**Interfaces:**
- Produces: `virtual std::shared_ptr<Object> function(const std::vector<std::shared_ptr<Object>>& args) = 0;` — Task 2가 이 시그니처로 재사용 버퍼를 넘긴다.
- 호출부(evaluator/vm)는 lvalue를 넘기므로 수정 불필요 — 컴파일로 검증.

- [ ] **Step 1: 시그니처 일괄 변경**

`object/object.h:174`:
```cpp
    virtual std::shared_ptr<Object> function(const std::vector<std::shared_ptr<Object>>& args) = 0;
```

나머지는 완전 균일 패턴이므로 sed로 (레포 루트 `hong-ik/`에서):
```bash
sed -i 's/function(std::vector<std::shared_ptr<Object>> parameters)/function(const std::vector<std::shared_ptr<Object>>\& parameters)/g' \
  object/built_in.h object/builtins/math.cpp object/builtins/string.cpp object/builtins/array.cpp \
  object/builtins/collection.cpp object/builtins/io.cpp object/builtins/conversion.cpp object/builtins/json.cpp
```

- [ ] **Step 2: 잔여 by-value 시그니처가 없는지 확인**

Run: `grep -rn "function(std::vector<std::shared_ptr<Object>>" object/ evaluator/ vm/ web/ repl/`
Expected: 출력 없음. (있으면 해당 파일 수동 수정 — 순수 가상과 오버라이드 불일치는 어차피 컴파일 에러로 터진다.)

주의: 일부 정의부가 `parameters` 내용을 **수정**(예: `parameters[0] = ...`)하면 const&에서 컴파일 에러가 난다. 그 경우 함수 안에서 지역 복사를 만들지 말고, 수정 없이 동작하도록 지역 변수로 대체한다 (예상 지점 없음 — 에러가 나는 곳만 대응).

- [ ] **Step 3: 빌드 + 전체 테스트**

```bash
"/c/Program Files/JetBrains/CLion 2025.2.2/bin/cmake/win/x64/bin/cmake" --build cmake-build-debug --target HongIkTests HongIk
"/c/Program Files/JetBrains/CLion 2025.2.2/bin/cmake/win/x64/bin/ctest" --test-dir cmake-build-debug --output-on-failure
```
Expected: 전부 PASS (549개).

- [ ] **Step 4: clang-format + 커밋**

```bash
git add object/object.h object/built_in.h object/builtins/
git commit -m "perf(object): Builtin::function 인자를 const 참조로 — 호출당 vector 복사 제거

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: VM 빌트인 호출 경로 정비 + 길이 fast path (spec D2·D3)

**Files:**
- Modify: `vm/vm.h` (멤버 2개 + 헬퍼 선언)
- Modify: `vm/vm_handlers.cpp` (`opCall` 빌트인 분기, `opInvoke` 빌트인 분기)
- Modify: `vm/vm.cpp` (생성자에서 길이 빌트인 캐시)
- Test: `tests/vm_test.cpp` (등가성·에러 시맨틱 잠금 테스트 추가)

**Interfaces:**
- Consumes: Task 1의 `function(const std::vector<...>&)`.
- Produces: `bool VM::tryLengthOf(const VMValue& arg, long long& out)` (private), 멤버 `std::shared_ptr<Builtin> lengthBuiltin_`, `std::vector<std::shared_ptr<Object>> builtinArgs_`.

- [ ] **Step 1: 시맨틱 잠금 테스트 먼저 작성** (최적화 전에도 통과해야 하는 회귀 방어망 — fast path가 시맨틱을 바꾸면 실패한다)

`tests/vm_test.cpp` 끝에 추가:
```cpp
// ---- VM 최적화 사이클 1: 빌트인 경계 시맨틱 잠금 (spec D2·D3) ----
// 길이 fast path(VM 네이티브 경로)가 registry 구현과 동일하게 동작하는지 고정.
TEST(VMBuiltinBoundary, LengthSemanticsLocked) {
    // (source, expected ToString) — VM 실행 결과
    EXPECT_EQ(runVMSource("길이(\"가나다\")")->ToString(), "3");
    EXPECT_EQ(runVMSource("길이(\"\")")->ToString(), "0");
    EXPECT_EQ(runVMSource("길이([1, 2, 3, 4])")->ToString(), "4");
    EXPECT_EQ(runVMSource("길이({\"a\": 1, \"b\": 2})")->ToString(), "2");
    // ASCII+한글 혼합: 코드포인트 기준
    EXPECT_EQ(runVMSource("길이(\"a가b나\")")->ToString(), "4");
}

TEST(VMBuiltinBoundary, LengthErrorsFallThroughToGenericPath) {
    // 지원 안 되는 타입 → registry의 에러 메시지 그대로 (fast path 폴스루 검증)
    EXPECT_THROW_WITH_MESSAGE(runVMSource("길이(5)"), "길이 함수는 문자열, 배열 또는 사전만 지원합니다.");
    // 인자 수 오류 → registry 메시지 그대로
    EXPECT_THROW_WITH_MESSAGE(runVMSource("길이(\"a\", \"b\")"), "길이 함수는 인자를 1개만 받습니다.");
}

TEST(VMBuiltinBoundary, GenericBuiltinPathStillWorks) {
    // 재사용 버퍼 경로: 다인자 빌트인 + 스칼라/컬렉션 인자 혼합
    EXPECT_EQ(runVMSource("최대(3, 7)")->ToString(), "7");
    EXPECT_EQ(runVMSource("배열 a = []\n추가(a, 1)\n추가(a, 2)\n길이(a)")->ToString(), "2");
    // 연속 호출에서 버퍼 재사용이 인자를 오염시키지 않는지
    EXPECT_EQ(runVMSource("최소(최대(1, 2), 최대(3, 4))")->ToString(), "2");
}

TEST(VMBuiltinBoundary, MethodStyleBuiltinInvoke) {
    // OP_INVOKE 빌트인 경로 (내장 타입 메서드 호출)
    EXPECT_EQ(runVMSource("배열 a = [3, 1, 2]\na.길이()")->ToString(), "3");
}
```

주의(구현자에게): `runVMSource`/`EXPECT_THROW_WITH_MESSAGE` 헬퍼가 `tests/vm_test.cpp`에 이미 있는지 먼저 확인하고, **있으면 기존 것을 그대로 사용**한다 (이름이 다르면 기존 이름으로 맞춘다). 없으면 파일 상단 anonymous namespace에 추가:
```cpp
// bench::prepare 재사용 — 표준 파이프라인 (trailing \n 포함)
#include "benchmarks/runner.h"
static std::shared_ptr<Object> runVMSource(const std::string& src) {
    auto sp = bench::prepare(src + "\n");
    return bench::runVm(sp);
}
#define EXPECT_THROW_WITH_MESSAGE(stmt, msg)                    \
    try {                                                       \
        stmt;                                                   \
        FAIL() << "예외가 발생하지 않음";                        \
    } catch (const std::exception& e) {                         \
        EXPECT_TRUE(std::string(e.what()).find(msg) != std::string::npos) << e.what(); \
    }
```
`a.길이()` 메서드 문법이 파서에서 지원되는지 불확실하면 기존 테스트에서 `MethodCall` 사용례를 먼저 검색(`grep -rn "\.길이()\|MethodCall" tests/`)하고, 없으면 이 테스트 케이스는 `길이(a)` 형태로 대체한다 (문법 추측 금지 규칙).

- [ ] **Step 2: 테스트 실행 — 전부 통과 확인** (잠금 성격이므로 최적화 전에 green이어야 함)

```bash
"…/cmake" --build cmake-build-debug --target HongIkTests && cmake-build-debug/HongIkTests --gtest_filter="VMBuiltinBoundary.*"
```
Expected: PASS. (실패하면 테스트가 현재 시맨틱과 다른 것 — 실제 동작에 맞춰 기대값 수정.)

- [ ] **Step 3: vm.h에 멤버·헬퍼 추가**

`vm/vm.h`의 private 섹션에 (builtins 멤버 근처):
```cpp
    // 빌트인 경계 최적화 (spec D2·D3)
    std::shared_ptr<Builtin> lengthBuiltin_;            // '길이' fast path 식별용 (identity 비교)
    std::vector<std::shared_ptr<Object>> builtinArgs_;  // 빌트인 인자 버퍼 재사용 — 빌트인은 VM 재진입 없는 leaf 함수
    bool tryLengthOf(const VMValue& arg, long long& out);
```

- [ ] **Step 4: 생성자에서 길이 빌트인 캐시 + tryLengthOf 구현**

`vm/vm.cpp` 생성자:
```cpp
VM::VM(IOContext* ioCtxPtr, ExecutionLimiter* limiterPtr)
    : ioCtx(ioCtxPtr), limiter(limiterPtr) {
    stack.reserve(STACK_MAX);
    builtins = BuiltinRegistry::build(ioCtx);
    auto lenIt = builtins.find("길이");
    if (lenIt != builtins.end()) lengthBuiltin_ = lenIt->second;
}
```

`vm/vm.cpp` (fillDefaults 근처)에 추가 — **collection.cpp의 Length::function과 시맨틱 동일 유지가 불변식** (등가성은 Step 1 테스트가 잠금):
```cpp
// '길이' fast path (spec D3). 지원 타입이 아니면 false — 호출측이 제네릭 경로로 폴스루해
// 에러 메시지·엣지 시맨틱이 registry 구현과 자동으로 일치한다.
bool VM::tryLengthOf(const VMValue& arg, long long& out) {
    if (!arg.isObject()) return false;
    Object* o = arg.asObject().get();
    if (auto* str = dynamic_cast<String*>(o)) {
        out = static_cast<long long>(utf8::codePointCount(str->value));
        return true;
    }
    if (auto* arr = dynamic_cast<Array*>(o)) {
        out = static_cast<long long>(arr->elements.size());
        return true;
    }
    if (auto* hm = dynamic_cast<HashMap*>(o)) {
        out = static_cast<long long>(hm->pairs.size());
        return true;
    }
    return false;
}
```

- [ ] **Step 5: opCall 빌트인 분기 재작성**

`vm/vm_handlers.cpp`의 `opCall()` 첫 분기(현재 142~152행)를 다음으로 교체:
```cpp
    if (callee.isObject()) {
        if (auto* builtin = dynamic_cast<Builtin*>(callee.asObject().get())) {
            // '길이' fast path: 인자 vector·결과 Integer 할당·std::function 디스패치 생략 (spec D3)
            if (argCount == 1 && builtin == lengthBuiltin_.get()) {
                long long len;
                if (tryLengthOf(peek(0), len)) {
                    stack.resize(stack.size() - 2); // 인자 + callee 제거
                    push(VMValue::Int(len));
                    return;
                }
            }
            // 제네릭 경로: 버퍼 재사용 (const& 시그니처라 호출 복사 없음, spec D2)
            builtinArgs_.clear();
            for (int i = 0; i < argCount; i++) {
                builtinArgs_.push_back(peek(argCount - 1 - i).toObject());
            }
            stack.resize(stack.size() - argCount - 1); // 인자들 + callee 제거
            auto result = builtin->function(builtinArgs_);
            // builtin은 void 의미를 Null 객체로 반환한다 (nullptr 반환 안 함). fromObject가 NULL_OBJ를 VMValue::Null()로 정규화.
            push(VMValue::fromObject(result));
            return;
        }
    }
    if (callee.isObject() && dynamic_cast<Closure*>(callee.asObject().get())) {
```
(뒤따르는 `else if` 체인의 첫 항목을 위처럼 `if`로 바꾸고 빌트인 분기는 `return`으로 빠진다 — 기존 `else if (callee.isObject() && dynamic_cast<Closure*>...)`부터는 구조 유지.)

주의: 기존 코드의 `for (...) pop(); pop();`은 `stack.resize(stack.size() - argCount - 1)`과 동일 — `peek(argCount)`이 이미 깊이를 검증했다.

- [ ] **Step 6: opInvoke 빌트인 분기도 버퍼 재사용으로**

`opInvoke()`의 내장 타입 메서드 분기(현재 509~518행)를 교체:
```cpp
        auto bit = builtins.find(methodName->value);
        if (bit != builtins.end()) {
            builtinArgs_.clear();
            builtinArgs_.push_back(peek(argCount).toObject()); // target이 args[0]
            for (int i = 0; i < argCount; i++) {
                builtinArgs_.push_back(peek(argCount - 1 - i).toObject());
            }
            stack.resize(stack.size() - argCount - 1); // 인자들 + target 제거
            auto result = bit->second->function(builtinArgs_);
            push(VMValue::fromObject(result));
            return;
        }
```

- [ ] **Step 7: 빌드 + 전체 테스트**

Run: 빌드 후 `ctest --test-dir cmake-build-debug --output-on-failure`
Expected: 전부 PASS (기존 549 + Step 1 추가분).

- [ ] **Step 8: 효과 스팟체크 (참고용, 게이트 아님)**

```bash
"…/cmake" --build cmake-build-release --target HongIkBenchmarks --config Release
./cmake-build-release/Release/HongIkBenchmarks.exe --benchmark_filter="builtin_calls|array_ops" --benchmark_repetitions=3
```
결과 중앙값을 진행 원장에 메모 (베이스라인: vm/builtin_calls 46.0ms, vm/array_ops 8.7ms).

- [ ] **Step 9: clang-format + 커밋**

```bash
git add vm/vm.h vm/vm.cpp vm/vm_handlers.cpp tests/vm_test.cpp
git commit -m "perf(vm): 빌트인 호출 경로 정비 — 인자 버퍼 재사용 + '길이' fast path

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: OP_INDEX_GET / OP_INDEX_SET의 스칼라 toObject 제거 (spec D4)

**Files:**
- Modify: `vm/vm_handlers.cpp` (`opIndexGet`, `opIndexSet`)
- Test: `tests/vm_test.cpp` (인덱스 시맨틱 잠금 테스트 추가)

**Interfaces:**
- Consumes: 없음 (독립).
- Produces: 동작 불변 — 인터페이스 변화 없음.

- [ ] **Step 1: 시맨틱 잠금 테스트 작성** (`tests/vm_test.cpp`, Task 2와 같은 헬퍼 사용)

```cpp
TEST(VMIndexBoundary, IndexSemanticsLocked) {
    EXPECT_EQ(runVMSource("배열 a = [10, 20, 30]\na[1]")->ToString(), "20");
    EXPECT_EQ(runVMSource("배열 a = [10, 20, 30]\na[-1]")->ToString(), "30"); // 음수 인덱스
    EXPECT_EQ(runVMSource("문자 s = \"가나다\"\ns[1]")->ToString(), "나");
    EXPECT_EQ(runVMSource("문자 s = \"가나다\"\ns[-1]")->ToString(), "다");
    EXPECT_EQ(runVMSource("사전 d = {\"k\": 42}\nd[\"k\"]")->ToString(), "42");
    EXPECT_EQ(runVMSource("배열 a = [1, 2]\na[0] = 9\na[0]")->ToString(), "9");
    EXPECT_EQ(runVMSource("배열 a = [1, 2]\na[-1] = 9\na[1]")->ToString(), "9"); // set 음수 인덱스
    EXPECT_EQ(runVMSource("사전 d = {\"k\": 1}\nd[\"k\"] = 7\nd[\"k\"]")->ToString(), "7");
}

TEST(VMIndexBoundary, IndexErrorsLocked) {
    // 에러 메시지 바이트 단위 잠금 — D4 재작성이 메시지를 바꾸면 실패
    EXPECT_THROW_WITH_MESSAGE(runVMSource("배열 a = [1]\na[5]"),
        "배열의 범위 밖 인덱스입니다: 인덱스 5, 배열 크기 1");
    EXPECT_THROW_WITH_MESSAGE(runVMSource("배열 a = [1]\na[\"x\"]"), "배열 인덱스는 정수여야 합니다.");
    EXPECT_THROW_WITH_MESSAGE(runVMSource("문자 s = \"가\"\ns[3]"), "문자열의 범위 밖 인덱스입니다.");
    EXPECT_THROW_WITH_MESSAGE(runVMSource("사전 d = {\"a\": 1}\nd[\"z\"]"), "사전에 키가 존재하지 않습니다.");
    EXPECT_THROW_WITH_MESSAGE(runVMSource("사전 d = {\"a\": 1}\nd[0]"), "사전 키는 문자열이어야 합니다.");
    EXPECT_THROW_WITH_MESSAGE(runVMSource("정수 x = 1\nx[0]"), "인덱스 연산이 지원되지 않는 형식입니다.");
    EXPECT_THROW_WITH_MESSAGE(runVMSource("배열 a = [1]\na[5] = 0"),
        "배열의 범위 밖 인덱스입니다: 인덱스 5, 배열 크기 1");
    EXPECT_THROW_WITH_MESSAGE(runVMSource("정수 x = 1\nx[0] = 1"), "인덱스 대입이 지원되지 않는 형식입니다.");
}
```

- [ ] **Step 2: 테스트 실행 — 통과 확인** (잠금 성격)

Run: `cmake-build-debug/HongIkTests --gtest_filter="VMIndexBoundary.*"`
Expected: PASS. (기대값이 실제와 다르면 실제 동작에 맞춰 수정 — 특히 튜플/음수 인덱스는 기존 코드 확인.)

- [ ] **Step 3: opIndexGet 재작성**

`vm/vm_handlers.cpp`의 `opIndexGet()` 전체 교체:
```cpp
namespace {
    // VMValue 인덱스에서 정수 추출 — INT 태그 직접, OBJECT 태그의 Integer도 수용(방어적; 정상 경로에선 안 나타남)
    bool vmIndexAsInt(const VMValue& v, long long& out) {
        if (v.isInt()) { out = v.asInt(); return true; }
        if (v.isObject()) {
            if (auto* i = dynamic_cast<Integer*>(v.asObject().get())) { out = i->value; return true; }
        }
        return false;
    }
} // namespace

void VM::opIndexGet() {
    auto index = pop();
    auto collection = pop();
    // 스칼라 인덱스의 toObject() 힙 할당 제거 (spec D4) — 태그 직접 검사, 시맨틱·메시지 불변
    Object* colObj = collection.isObject() ? collection.asObject().get() : nullptr;
    if (auto* arr = colObj ? dynamic_cast<Array*>(colObj) : nullptr) {
        long long idxVal;
        if (!vmIndexAsInt(index, idxVal)) throw RuntimeException("배열 인덱스는 정수여야 합니다.", currentLine());
        long long actualIdx = idxVal;
        if (actualIdx < 0) actualIdx = static_cast<long long>(arr->elements.size()) + actualIdx;
        if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
            throw RuntimeException("배열의 범위 밖 인덱스입니다: 인덱스 " + to_string(idxVal) + ", 배열 크기 " + to_string(arr->elements.size()), currentLine());
        push(VMValue::fromObject(arr->elements[actualIdx]));
    } else if (auto* str = colObj ? dynamic_cast<String*>(colObj) : nullptr) {
        long long idxVal;
        if (!vmIndexAsInt(index, idxVal)) throw RuntimeException("문자열 인덱스는 정수여야 합니다.", currentLine());
        const auto& cps = str->codePoints();
        long long len = static_cast<long long>(cps.size());
        long long actualIdx = idxVal;
        if (actualIdx < 0) actualIdx = len + actualIdx;
        if (actualIdx < 0 || actualIdx >= len)
            throw RuntimeException("문자열의 범위 밖 인덱스입니다.", currentLine());
        push(VMValue::Obj(make_shared<String>(cps[actualIdx])));
    } else if (auto* hm = colObj ? dynamic_cast<HashMap*>(colObj) : nullptr) {
        String* key = index.isObject() ? dynamic_cast<String*>(index.asObject().get()) : nullptr;
        if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", currentLine());
        auto it = hm->pairs.find(key->value);
        if (it == hm->pairs.end())
            throw RuntimeException("사전에 키가 존재하지 않습니다.", currentLine());
        push(VMValue::fromObject(it->second));
    } else if (auto* tup = colObj ? dynamic_cast<Tuple*>(colObj) : nullptr) {
        long long idxVal;
        if (!vmIndexAsInt(index, idxVal) || idxVal < 0 || idxVal >= static_cast<long long>(tup->elements.size()))
            throw RuntimeException("튜플의 범위 밖 인덱스입니다.", currentLine());
        push(VMValue::fromObject(tup->elements[idxVal]));
    } else {
        throw RuntimeException("인덱스 연산이 지원되지 않는 형식입니다.", currentLine());
    }
}
```
주의: 기존 코드의 사전 키 에러는 `"사전의 키는..."`가 아니라 `"사전 키는 문자열이어야 합니다."`(365~367행) — 그대로 유지. 스칼라 컬렉션(정수 등)은 `colObj == nullptr` → 마지막 else로 흘러 기존과 동일한 에러.

- [ ] **Step 4: opIndexSet 재작성**

```cpp
void VM::opIndexSet() {
    auto value = pop();
    auto index = pop();
    auto collection = pop();
    Object* colObj = collection.isObject() ? collection.asObject().get() : nullptr;
    if (auto* arr = colObj ? dynamic_cast<Array*>(colObj) : nullptr) {
        long long idxVal;
        if (!vmIndexAsInt(index, idxVal)) throw RuntimeException("배열 인덱스는 정수여야 합니다.", currentLine());
        long long actualIdx = idxVal;
        if (actualIdx < 0) actualIdx += static_cast<long long>(arr->elements.size());
        if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
            throw RuntimeException("배열의 범위 밖 인덱스입니다: 인덱스 " + to_string(idxVal) + ", 배열 크기 " + to_string(arr->elements.size()), currentLine());
        arr->elements[actualIdx] = value.toObject(); // 원소 저장은 Object — 후보 ③(별도 사이클) 전까지 유지
    } else if (auto* hm = colObj ? dynamic_cast<HashMap*>(colObj) : nullptr) {
        String* key = index.isObject() ? dynamic_cast<String*>(index.asObject().get()) : nullptr;
        if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", currentLine());
        hm->pairs[key->value] = value.toObject();
    } else {
        throw RuntimeException("인덱스 대입이 지원되지 않는 형식입니다.", currentLine());
    }
    push(value);
}
```

- [ ] **Step 5: 빌드 + 전체 테스트**

Expected: 전부 PASS. 특히 `VMIndexBoundary.*`와 골든 테스트.

- [ ] **Step 6: 스팟체크 (참고용)**

```bash
./cmake-build-release/Release/HongIkBenchmarks.exe --benchmark_filter="array_ops|hashmap_ops|string_index" --benchmark_repetitions=3
```
(Release 재빌드 후. 베이스라인: vm/array 8.7 / vm/hashmap 24.5 / vm/string_index 51.5)

- [ ] **Step 7: clang-format + 커밋**

```bash
git add vm/vm_handlers.cpp tests/vm_test.cpp
git commit -m "perf(vm): OP_INDEX_GET/SET에서 스칼라 인덱스 toObject 힙 할당 제거

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: Peephole 패스 — decode/융합/재조립 + 신설 opcode 4종 (spec D5)

**Files:**
- Create: `vm/peephole.h`, `vm/peephole.cpp`
- Modify: `vm/opcode.h` (OP_GET_BUILTIN 제거, `*_POP` 4종 추가, opcodeName 갱신)
- Modify: `vm/vm.cpp` (`run()` switch에 4종 핸들링)
- Modify: `vm/compiler.cpp` (`Compiler::optimize` — peephole 호출 + 중첩 함수/클래스 재귀)
- Modify: `CMakeLists.txt` (`HONGIK_CORE_SOURCES`에 `vm/peephole.cpp`, HongIkTests에 `tests/peephole_test.cpp`)
- Test: `tests/peephole_test.cpp` (신규)

**Interfaces:**
- Produces: `namespace peephole { struct DecodedInstr { OpCode op; size_t pos; size_t size; }; std::vector<DecodedInstr> decode(const std::vector<uint8_t>& code); void optimizeChunk(CompiledFunction& fn); }`
- Consumes: `CompiledFunction` (chunk.h), `bench::prepare` (테스트에서).

- [ ] **Step 1: opcode.h 갱신**

`OP_GET_BUILTIN` 항목 삭제 (emit·핸들러 모두 없는 죽은 opcode — `grep -rn "OP_GET_BUILTIN" --include="*.cpp" --include="*.h" .`으로 opcode.h 외 사용처 없음을 먼저 확인). enum 끝(OP_YIELD 뒤)에 추가:
```cpp
    // 융합 명령 (vm/peephole.cpp 산출 — 컴파일러가 직접 emit하지 않는다)
    OP_SET_LOCAL_POP,   // [uint16 slot] SET_LOCAL 후 즉시 POP
    OP_SET_GLOBAL_POP,  // [uint16 nameIdx]
    OP_SET_UPVALUE_POP, // [uint16 slot]
    OP_SET_MEMBER_POP,  // [uint16 nameIdx]
```
`opcodeName`에도 4개 case 추가, `OP_GET_BUILTIN` case 삭제.

- [ ] **Step 2: 실패하는 테스트 작성** (`tests/peephole_test.cpp` 신규 — 진짜 TDD 구간)

```cpp
#include "vm/peephole.h"
#include "benchmarks/runner.h"
#include "object/object.h"
#include <gtest/gtest.h>

namespace {

    long long asInt(const std::shared_ptr<Object>& obj) {
        auto* i = dynamic_cast<Integer*>(obj.get());
        EXPECT_NE(i, nullptr) << (obj ? obj->ToString() : "null");
        return i ? i->value : -1;
    }

    bool containsOp(const CompiledFunction& fn, OpCode op) {
        for (auto& in : peephole::decode(fn.code))
            if (in.op == op) return true;
        return false;
    }

    // ---- decode ----
    TEST(PeepholeDecode, CoversAllConstructs) {
        // 함수(클로저)·클래스·try·foreach·match·보간 등 가변 길이 포함 프로그램이 끝까지 디코드되는지
        auto sp = bench::prepare(
            "함수 f(정수 a) -> 정수:\n    리턴 a + 1\n"
            "클래스 점:\n    정수 x\n    생성(정수 a):\n        자기.x = a\n"
            "배열 arr = [1, 2]\n"
            "각각 arr 의 원소 마다:\n    출력(원소)\n"
            "f(1)\n");
        auto instrs = peephole::decode(sp.bytecode->code);
        ASSERT_FALSE(instrs.empty());
        // 명령 크기 합 == 코드 길이 (경계 정합)
        size_t total = 0;
        for (auto& in : instrs) total += in.size;
        EXPECT_EQ(total, sp.bytecode->code.size());
    }

    // ---- 융합 존재 ----
    TEST(PeepholeFusion, GlobalAssignmentFused) {
        auto sp = bench::prepare("정수 x = 0\nx = 1\nx\n");
        EXPECT_TRUE(containsOp(*sp.bytecode, OpCode::OP_SET_GLOBAL_POP));
        EXPECT_EQ(asInt(bench::runVm(sp)), 1);
    }

    TEST(PeepholeFusion, LocalAssignmentInFunctionFused) {
        auto sp = bench::prepare(
            "함수 f() -> 정수:\n    정수 y = 0\n    y = 5\n    리턴 y\nf()\n");
        // 중첩 함수 상수에 재귀 적용됐는지
        bool found = false;
        for (auto& c : sp.bytecode->constants) {
            if (auto* nested = dynamic_cast<CompiledFunction*>(c.get()))
                if (containsOp(*nested, OpCode::OP_SET_LOCAL_POP)) found = true;
        }
        EXPECT_TRUE(found);
        EXPECT_EQ(asInt(bench::runVm(sp)), 5);
    }

    TEST(PeepholeFusion, MemberAssignmentInMethodFused) {
        auto sp = bench::prepare(
            "클래스 점:\n    정수 x\n    생성(정수 a):\n        자기.x = a\n"
            "    함수 셋(정수 v) -> 정수:\n        자기.x = v\n        리턴 자기.x\n"
            "점 p = 점(1)\np.셋(9)\n");
        bool found = false;
        for (auto& c : sp.bytecode->constants) {
            if (auto* ccd = dynamic_cast<CompiledClassDef*>(c.get())) {
                if (ccd->compiledConstructor && containsOp(*ccd->compiledConstructor, OpCode::OP_SET_MEMBER_POP)) found = true;
                for (auto& [name, m] : ccd->compiledMethods)
                    if (containsOp(*m, OpCode::OP_SET_MEMBER_POP)) found = true;
            }
        }
        EXPECT_TRUE(found);
        EXPECT_EQ(asInt(bench::runVm(sp)), 9);
    }

    // ---- 점프 타깃 가드 (수제 청크 — 소스로는 강제하기 어려움) ----
    TEST(PeepholeGuard, PopAtJumpTargetNotFused) {
        CompiledFunction fn;
        // JUMP +4 → SET_LOCAL 0 바로 뒤의 POP을 타깃으로 하는 인위적 청크
        fn.emitOpAndUint16(OpCode::OP_JUMP, 4, 1);        // pos 0..2, target = 3+4 = 7 (POP 위치)
        fn.emitOpAndUint16(OpCode::OP_SET_LOCAL, 0, 1);   // pos 3..6
        fn.emitOp(OpCode::OP_POP, 1);                     // pos 7
        fn.emitOp(OpCode::OP_RETURN, 1);                  // pos 8
        auto before = fn.code;
        peephole::optimizeChunk(fn);
        EXPECT_EQ(fn.code, before); // 가드로 융합 불가 → 무변경
    }

    TEST(PeepholeGuard, PopNotTargetIsFused) {
        CompiledFunction fn;
        fn.emitOpAndUint16(OpCode::OP_JUMP, 5, 1);        // target = 3+5 = 8 (RETURN 위치) — POP 아님
        fn.emitOpAndUint16(OpCode::OP_SET_LOCAL, 0, 1);
        fn.emitOp(OpCode::OP_POP, 1);
        fn.emitOp(OpCode::OP_RETURN, 1);                  // pos 8
        peephole::optimizeChunk(fn);
        // 융합 후: JUMP(3) + SET_LOCAL_POP(3) + RETURN(1) = 7바이트, JUMP 타깃은 새 RETURN 위치(6)로 재매핑
        ASSERT_EQ(fn.code.size(), 7u);
        EXPECT_EQ(static_cast<OpCode>(fn.code[3]), OpCode::OP_SET_LOCAL_POP);
        uint16_t off = static_cast<uint16_t>((fn.code[1] << 8) | fn.code[2]);
        EXPECT_EQ(3 + off, 6u); // 새 타깃 = RETURN 새 위치
        EXPECT_EQ(fn.code.size(), fn.lines.size()); // lines 재매핑 정합
    }

    // ---- 실행 동등성: 융합·재매핑이 제어 흐름을 깨지 않는지 (VM vs Evaluator) ----
    TEST(PeepholeParity, ControlFlowWithAssignments) {
        const char* sources[] = {
            // while + break/continue + 대입
            "정수 s = 0\n정수 i = 0\n반복 i < 10 동안:\n    i = i + 1\n    만약 i == 3 라면:\n        계속\n    만약 i == 8 라면:\n        멈춤\n    s = s + i\ns\n",
            // if/else 마지막 문이 대입
            "정수 x = 0\n만약 1 < 2 라면:\n    x = 10\n아니면:\n    x = 20\nx\n",
            // try/catch 안 대입
            "정수 r = 0\n시도:\n    r = 1\n    정수 z = 1 / 0\n예외 e:\n    r = 2\nr\n",
            // foreach 안 대입
            "정수 s = 0\n배열 a = [1, 2, 3]\n각각 a 의 v 마다:\n    s = s + v\ns\n",
        };
        for (const char* src : sources) {
            auto sp = bench::prepare(src);
            EXPECT_EQ(asInt(bench::runVm(sp)), asInt(bench::runEval(sp))) << src;
        }
    }

} // namespace
```
주의(구현자에게): 위 .hik 구문(`계속`/`멈춤`/`시도`/`예외`/`각각 … 의 … 마다`)은 **기존 테스트에서 실제 사용례를 검색해 확인** 후 사용한다 (`grep -rn "멈춤\|계속\|시도\|각각" tests/*.cpp` — 다르면 실제 문법으로 교체). 키워드가 지원 안 되면 해당 케이스는 지원되는 구문으로 대체하되 "점프 위에 대입" 구조는 유지한다.

- [ ] **Step 3: CMakeLists 수정 + 테스트 실행 → 컴파일 실패 확인**

`CMakeLists.txt`: `HONGIK_CORE_SOURCES`의 `vm/vm_handlers.cpp` 다음 줄에 `vm/peephole.cpp` 추가. `HongIkTests` 소스 목록의 `tests/benchmark_scenarios_test.cpp` 다음에 `tests/peephole_test.cpp` 추가.
Run: 빌드 → Expected: FAIL — `vm/peephole.h` 없음.

- [ ] **Step 4: peephole.h 작성**

```cpp
#ifndef PEEPHOLE_H
#define PEEPHOLE_H

#include "chunk.h"
#include <cstddef>
#include <vector>

namespace peephole {

    // 디코드된 명령 하나. size = opcode 1바이트 + 피연산자 바이트.
    struct DecodedInstr {
        OpCode op;
        size_t pos;  // 원본 code에서의 시작 오프셋
        size_t size; // 총 바이트 수
    };

    // 바이트코드를 명령 단위로 디코드한다.
    // 알 수 없는 opcode·경계 초과를 만나면 빈 벡터를 반환한다 (호출측은 최적화를 포기 — 안전 우선).
    std::vector<DecodedInstr> decode(const std::vector<uint8_t>& code);

    // SET_{LOCAL,GLOBAL,UPVALUE,MEMBER} + OP_POP → *_POP 융합 후 점프 오프셋·lines를 재조립한다.
    // 이 함수는 fn.code만 다룬다 — 중첩 함수/클래스 재귀는 호출측(Compiler::optimize) 책임.
    void optimizeChunk(CompiledFunction& fn);

} // namespace peephole

#endif // PEEPHOLE_H
```

- [ ] **Step 5: peephole.cpp 작성**

```cpp
#include "peephole.h"
#include <unordered_map>
#include <unordered_set>

namespace {

    // 피연산자 바이트 수. -1 = 가변(OP_CLOSURE), -2 = 알 수 없음.
    int operandBytes(OpCode op) {
        switch (op) {
        case OpCode::OP_CONSTANT:
        case OpCode::OP_GET_LOCAL:
        case OpCode::OP_SET_LOCAL:
        case OpCode::OP_GET_GLOBAL:
        case OpCode::OP_SET_GLOBAL:
        case OpCode::OP_DEFINE_GLOBAL:
        case OpCode::OP_JUMP:
        case OpCode::OP_JUMP_IF_FALSE:
        case OpCode::OP_LOOP:
        case OpCode::OP_GET_UPVALUE:
        case OpCode::OP_SET_UPVALUE:
        case OpCode::OP_BUILD_ARRAY:
        case OpCode::OP_BUILD_HASHMAP:
        case OpCode::OP_BUILD_TUPLE:
        case OpCode::OP_GET_MEMBER:
        case OpCode::OP_SET_MEMBER:
        case OpCode::OP_TRY_BEGIN:
        case OpCode::OP_ITER_NEXT:
        case OpCode::OP_INTERPOLATE:
        case OpCode::OP_TYPE_CHECK:
        case OpCode::OP_DECL_CHECK:
        case OpCode::OP_IMPORT:
        case OpCode::OP_SET_LOCAL_POP:
        case OpCode::OP_SET_GLOBAL_POP:
        case OpCode::OP_SET_UPVALUE_POP:
        case OpCode::OP_SET_MEMBER_POP:
            return 2;
        case OpCode::OP_CALL:
        case OpCode::OP_SLICE:
            return 1;
        case OpCode::OP_INVOKE:
            return 3; // uint16 nameIdx + uint8 argCount
        case OpCode::OP_CLOSURE:
            return -1; // uint16 constIdx + uint8 count + count * (uint8 + uint16)
        case OpCode::OP_NULL:
        case OpCode::OP_TRUE:
        case OpCode::OP_FALSE:
        case OpCode::OP_ADD:
        case OpCode::OP_SUB:
        case OpCode::OP_MUL:
        case OpCode::OP_DIV:
        case OpCode::OP_MOD:
        case OpCode::OP_POW:
        case OpCode::OP_BITWISE_AND:
        case OpCode::OP_BITWISE_OR:
        case OpCode::OP_EQUAL:
        case OpCode::OP_NOT_EQUAL:
        case OpCode::OP_LESS:
        case OpCode::OP_GREATER:
        case OpCode::OP_LESS_EQUAL:
        case OpCode::OP_GREATER_EQUAL:
        case OpCode::OP_AND:
        case OpCode::OP_OR:
        case OpCode::OP_NEGATE:
        case OpCode::OP_NOT:
        case OpCode::OP_RETURN:
        case OpCode::OP_INDEX_GET:
        case OpCode::OP_INDEX_SET:
        case OpCode::OP_TRY_END:
        case OpCode::OP_ITER_INIT:
        case OpCode::OP_ITER_VALUE:
        case OpCode::OP_POP:
        case OpCode::OP_DUP:
        case OpCode::OP_RANGE_CHECK: // opRangeCheck는 read* 호출 없음 — 피연산자 0 (vm_handlers.cpp:669)
        case OpCode::OP_ASSERT_BOOL:
        case OpCode::OP_YIELD:
            return 0;
        }
        return -2;
    }

    bool isJump(OpCode op) {
        return op == OpCode::OP_JUMP || op == OpCode::OP_JUMP_IF_FALSE || op == OpCode::OP_ITER_NEXT
            || op == OpCode::OP_TRY_BEGIN || op == OpCode::OP_LOOP;
    }

    uint16_t readU16(const std::vector<uint8_t>& code, size_t pos) {
        return static_cast<uint16_t>((static_cast<uint16_t>(code[pos]) << 8) | code[pos + 1]);
    }

    // 점프 명령의 절대 타깃. 모든 점프는 피연산자 뒤 ip 기준 상대 (OP_LOOP만 후방).
    size_t jumpTarget(const std::vector<uint8_t>& code, const peephole::DecodedInstr& in) {
        size_t after = in.pos + 3;
        uint16_t off = readU16(code, in.pos + 1);
        return in.op == OpCode::OP_LOOP ? after - off : after + off;
    }

    bool fusedOpFor(OpCode op, OpCode& fused) {
        switch (op) {
        case OpCode::OP_SET_LOCAL: fused = OpCode::OP_SET_LOCAL_POP; return true;
        case OpCode::OP_SET_GLOBAL: fused = OpCode::OP_SET_GLOBAL_POP; return true;
        case OpCode::OP_SET_UPVALUE: fused = OpCode::OP_SET_UPVALUE_POP; return true;
        case OpCode::OP_SET_MEMBER: fused = OpCode::OP_SET_MEMBER_POP; return true;
        default: return false;
        }
    }

} // namespace

namespace peephole {

    std::vector<DecodedInstr> decode(const std::vector<uint8_t>& code) {
        std::vector<DecodedInstr> out;
        size_t pos = 0;
        while (pos < code.size()) {
            OpCode op = static_cast<OpCode>(code[pos]);
            int ob = operandBytes(op);
            size_t size;
            if (ob == -1) { // OP_CLOSURE 가변 길이
                if (pos + 4 > code.size()) return {};
                uint8_t count = code[pos + 3];
                size = 4 + static_cast<size_t>(count) * 3;
            } else if (ob >= 0) {
                size = 1 + static_cast<size_t>(ob);
            } else {
                return {};
            }
            if (pos + size > code.size()) return {};
            out.push_back({op, pos, size});
            pos += size;
        }
        return out;
    }

    void optimizeChunk(CompiledFunction& fn) {
        auto instrs = decode(fn.code);
        if (instrs.empty()) return;

        // 1) 점프 타깃 절대주소 수집
        std::unordered_set<size_t> targets;
        for (const auto& in : instrs) {
            if (isJump(in.op)) targets.insert(jumpTarget(fn.code, in));
        }

        // 2) 융합 계획: SET_* 직후 OP_POP, 단 POP이 점프 타깃이면 제외
        std::vector<bool> drop(instrs.size(), false);
        std::vector<OpCode> newOp(instrs.size());
        bool changed = false;
        for (size_t i = 0; i < instrs.size(); i++) newOp[i] = instrs[i].op;
        for (size_t i = 0; i + 1 < instrs.size(); i++) {
            OpCode fused;
            if (fusedOpFor(instrs[i].op, fused) && instrs[i + 1].op == OpCode::OP_POP
                && targets.count(instrs[i + 1].pos) == 0) {
                newOp[i] = fused;
                drop[i + 1] = true;
                changed = true;
            }
        }
        if (!changed) return;

        // 3) 새 위치 계산 (융합은 길이 불변, POP 1바이트만 탈락)
        std::unordered_map<size_t, size_t> posMap; // 명령 시작 oldPos → newPos
        size_t newLen = 0;
        for (size_t i = 0; i < instrs.size(); i++) {
            if (drop[i]) continue;
            posMap[instrs[i].pos] = newLen;
            newLen += instrs[i].size;
        }
        posMap[fn.code.size()] = newLen; // 코드 끝을 가리키는 전방 점프 대응

        // 4) 재조립: 바이트 복사 + 점프 오프셋·lines 재매핑
        std::vector<uint8_t> newCode;
        std::vector<long long> newLines;
        newCode.reserve(newLen);
        newLines.reserve(newLen);
        for (size_t i = 0; i < instrs.size(); i++) {
            if (drop[i]) continue;
            const auto& in = instrs[i];
            size_t start = newCode.size();
            newCode.push_back(static_cast<uint8_t>(newOp[i]));
            newLines.push_back(fn.lines[in.pos]);
            for (size_t b = 1; b < in.size; b++) {
                newCode.push_back(fn.code[in.pos + b]);
                newLines.push_back(fn.lines[in.pos + b]);
            }
            if (isJump(in.op)) {
                auto it = posMap.find(jumpTarget(fn.code, in));
                if (it == posMap.end()) return; // 타깃이 명령 경계가 아님 — fn 미변경 상태로 포기
                size_t after = start + 3;
                size_t off = in.op == OpCode::OP_LOOP ? after - it->second : it->second - after;
                newCode[start + 1] = static_cast<uint8_t>((off >> 8) & 0xff);
                newCode[start + 2] = static_cast<uint8_t>(off & 0xff);
            }
        }
        fn.code = std::move(newCode);
        fn.lines = std::move(newLines);
    }

} // namespace peephole
```
**주의 — 구현자가 반드시 검증할 것:** 위 피연산자 표를 `vm.cpp`/`vm_handlers.cpp`의 각 핸들러 read* 호출과 1:1 대조 검증하라 (표는 계획 작성 시점에 핸들러 기준으로 검증됨). decode 테스트(크기 합 == 코드 길이)가 어긋나면 여기가 원인이다.

- [ ] **Step 6: VM run() switch에 4종 추가** (`vm/vm.cpp`, `OP_POP` case 근처)

기존 핸들러 재사용 + pop() — 시맨틱 분기 없음:
```cpp
            // 융합 명령 (peephole 산출): 기존 핸들러 + pop — 디스패치 1회 절약
            case OpCode::OP_SET_LOCAL_POP: {
                uint16_t slot = readUint16();
                size_t idx = frame.slotOffset + slot;
                if (idx >= stack.size()) {
                    throw RuntimeException("VM 로컬 슬롯이 범위 밖입니다.", currentLine());
                }
                stack[idx] = peek(0);
                pop();
                break;
            }
            case OpCode::OP_SET_GLOBAL_POP: opSetGlobal(); pop(); break;
            case OpCode::OP_SET_UPVALUE_POP: opSetUpvalue(); pop(); break;
            case OpCode::OP_SET_MEMBER_POP: opSetMember(); pop(); break;
```

- [ ] **Step 7: Compiler::optimize 구현** (`vm/compiler.cpp:120`)

```cpp
#include "peephole.h"
```
(파일 상단 include 추가 후)
```cpp
void Compiler::optimize(CompiledFunction& fn) {
    peephole::optimizeChunk(fn);
    // 중첩 함수(OP_CLOSURE 상수)와 클래스 생성자/메서드에 재귀 적용
    for (auto& c : fn.constants) {
        if (auto* nested = dynamic_cast<CompiledFunction*>(c.get())) {
            optimize(*nested);
        } else if (auto* ccd = dynamic_cast<CompiledClassDef*>(c.get())) {
            if (ccd->compiledConstructor) optimize(*ccd->compiledConstructor);
            for (auto& [name, method] : ccd->compiledMethods) optimize(*method);
        }
    }
}
```

- [ ] **Step 8: 테스트 실행 — 신규 전부 PASS 확인**

Run: 빌드 후 `cmake-build-debug/HongIkTests --gtest_filter="Peephole*"`
Expected: PASS. 이어서 **전체** ctest: 기존 549 + 신규 전부 PASS (골든·parity·벤치 스모크 포함 — 융합 회귀의 최전선).

- [ ] **Step 9: 스팟체크 (참고용)**

```bash
./cmake-build-release/Release/HongIkBenchmarks.exe --benchmark_filter="arith_loop|fib|frontend" --benchmark_repetitions=3
```
frontend/compile이 크게 늘지 않았는지도 확인 (베이스라인 0.9ms — 2ms 넘으면 원장에 플래그).

- [ ] **Step 10: clang-format + 커밋**

```bash
git add vm/peephole.h vm/peephole.cpp vm/opcode.h vm/vm.cpp vm/compiler.cpp CMakeLists.txt tests/peephole_test.cpp
git commit -m "perf(vm): peephole 융합 패스 — SET_*+POP 4종, 점프 타깃 가드·오프셋 재조립

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: 재측정 + compare.py + 결과 문서화 (spec D7)

**Files:**
- Create: `docs/benchmarks/2026-07-10-cycle1.md`, `docs/benchmarks/2026-07-10-cycle1.json`
- Modify: `.superpowers/sdd/progress.md` (원장 갱신)

**Interfaces:**
- Consumes: Task 1~4 완료 상태의 Release 빌드.

- [ ] **Step 1: Release 재빌드 + 측정** (측정 중 다른 부하 금지)

```bash
"…/cmake" --build cmake-build-release --target HongIkBenchmarks --config Release
./cmake-build-release/Release/HongIkBenchmarks.exe --benchmark_repetitions=5 --benchmark_format=json > docs/benchmarks/2026-07-10-cycle1.json
```

- [ ] **Step 2: compare.py로 베이스라인 대비**

```bash
python cmake-build-release/_deps/googlebenchmark-src/tools/compare.py benchmarks docs/benchmarks/2026-07-08-baseline.json docs/benchmarks/2026-07-10-cycle1.json
```
출력 전체를 보관 (문서에 첨부).

- [ ] **Step 3: DoD 판정**

- builtin_calls·array_ops·hashmap_ops 중앙값 개선 확인.
- **어느 시나리오도 중앙값 +5% 초과 회귀 없음** — 위반 시 원인 태스크를 bisect(태스크 커밋 단위)하고 머지 보류, 사용자 보고.

- [ ] **Step 4: 결과 문서 작성** (`docs/benchmarks/2026-07-10-cycle1.md`)

베이스라인 문서와 같은 형식: 측정 환경(커밋 해시 포함), before/after 표(중앙값, 개선율), compare.py 출력, 시나리오별 어느 최적화가 기여했는지 해석(태스크별 스팟체크 메모 활용), 남은 후보(전역 인라인 캐시·디스패치 타이트닝·컬렉션 표현) 갱신.

- [ ] **Step 5: CPython 대비 갱신** (선택 — 시간 되면)

`benchmarks/reference/*.py` 각 5회 재실행해 VM/CPython 비율 갱신. 환경이 베이스라인과 동일하므로 기존 CPython 수치 재사용도 가능 — 문서에 어느 쪽인지 명시.

- [ ] **Step 6: 커밋**

```bash
git add docs/benchmarks/2026-07-10-cycle1.md docs/benchmarks/2026-07-10-cycle1.json .superpowers/sdd/progress.md
git commit -m "docs(bench): 사이클 1 재측정 — 베이스라인 대비 compare.py 결과

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```
