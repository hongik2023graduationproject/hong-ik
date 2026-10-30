# 정적 타입 시스템 Phase B-2 (클래스 본문 분석) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** InstanceType을 도입하고 클래스 본문(필드/생성자/메서드)을 검사한다 — 미지 멤버 TC201, 필드 대입 TC002, 메서드 리턴 TC103, 서브타이핑.

**Architecture:** spec `2026-06-11-static-type-system-phase-b-design.md` D5 + 부록 C(클래스 실측). InstanceType은 조상 체인 스냅샷을 보유해 Type 계층을 TypeChecker 상태와 분리 유지. 클래스 정보(ClassInfo)는 TypeChecker가 보유하고 부모 체인을 lookup 시 탐색. 마지막에 type_checker.cpp를 3분할.

**Tech Stack:** C++17/26, CMake(풀패스 사용), Google Test, MSVC. 작업 경로: `E:\dev\projects\school\hongik\hong-ik`. 베이스라인 테스트 461개.

---

## Task 0: 사전 실측 — ✅ 완료 (2026-06-11, spec 부록 C)

- [x] 미지 필드 읽기/메서드 호출: both reject → TC201 근거
- [x] `자기.새필드 = ...` 동적 필드: both pass → 쓰기에는 TC201 금지, 동적 등록
- [x] 메서드 상호 참조: both pass → 시그니처 선등록 후 본문 검사
- [x] `자기` 없는 메서드 직접 호출(`둘째()`): both reject → TC006 발화 정당
- [x] 생성자 상속: Phase A TC101 오탐 핫픽스 완료 (bc723fb)
- [x] 서브타입 대입: VM 통과/eval 거부 (이슈 #6) → D5는 VM 기준
- [x] super-call 문법 없음 (docs/inheritance.md)

### Task 1: InstanceType (`util/type.{h,cpp}`)

**Files:**
- Modify: `util/type.h`, `util/type.cpp`
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성** — `TEST(TypeTest, AssignabilityRules)` 뒤에 추가:

```cpp
TEST(TypeTest, InstanceSubtyping) {
    auto animal = makeInstance("동물", {});
    auto dog = makeInstance("강아지", {"동물"});
    auto cat = makeInstance("고양이", {"동물"});

    EXPECT_TRUE(animal->isAssignableFrom(*dog));   // 부모 <- 자식
    EXPECT_FALSE(dog->isAssignableFrom(*animal));  // 자식 <- 부모 거부
    EXPECT_FALSE(dog->isAssignableFrom(*cat));     // 형제 거부
    EXPECT_TRUE(dog->isAssignableFrom(*dog));      // 동일
    EXPECT_TRUE(makeAny()->isAssignableFrom(*dog));
    EXPECT_TRUE(dog->isAssignableFrom(*makeNever()));
    EXPECT_FALSE(dog->isAssignableFrom(*makePrim(ObjectType::INTEGER)));
    // Optional 래핑: 동물? <- 강아지
    EXPECT_TRUE(makeOptional(makeInstance("동물", {}))->isAssignableFrom(*dog));
}
```

- [ ] **Step 2: 실패 확인** (컴파일 에러 — makeInstance 미정의)
- [ ] **Step 3: 구현** — `util/type.h`의 ClassType 뒤에:

```cpp
// 클래스 인스턴스 값. ClassType(클래스 자체·호출 대상)과 구분된다 (Phase B spec D5).
// ancestors는 생성 시점의 부모 체인 스냅샷 — Type 계층이 TypeChecker 상태를 참조하지 않게 한다.
class InstanceType final : public Type {
public:
    std::string className;
    std::vector<std::string> ancestors;

    InstanceType(std::string name, std::vector<std::string> ancestors)
        : className(std::move(name)), ancestors(std::move(ancestors)) {}

    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};
```

팩토리 선언 + `util/type.cpp` 구현:

```cpp
std::shared_ptr<Type> makeInstance(std::string className, std::vector<std::string> ancestors);

// type.cpp (ClassType 구현 뒤)
bool InstanceType::isAssignableFrom(const Type& other) const {
    if (dynamic_cast<const NeverType*>(&other)) return true;
    if (dynamic_cast<const AnyType*>(&other)) return true;
    auto* inst = dynamic_cast<const InstanceType*>(&other);
    if (!inst) return false;
    if (className == inst->className) return true;
    // 부모 <- 자식: other의 조상 체인에 자신이 있으면 허용 (spec D5, VM 시맨틱 — 이슈 #6)
    return std::find(inst->ancestors.begin(), inst->ancestors.end(), className)
           != inst->ancestors.end();
}
std::string InstanceType::toKorean() const { return className; }
bool InstanceType::equals(const Type& other) const {
    auto* inst = dynamic_cast<const InstanceType*>(&other);
    return inst && className == inst->className;
}
std::shared_ptr<Type> makeInstance(std::string className, std::vector<std::string> ancestors) {
    return std::make_shared<InstanceType>(std::move(className), std::move(ancestors));
}
```

(type.cpp에 `#include <algorithm>` 추가 필요 — std::find.)

- [ ] **Step 4: 빌드 + `-R TypeTest` PASS + 전체 회귀**
- [ ] **Step 5: Commit** — `feat(types): InstanceType with ancestor-chain subtyping (Phase B-2 Task 1)`

### Task 2: ClassInfo 등록 + InstanceType 적용

**Files:**
- Modify: `analyzer/type_checker.h` (ClassInfo, classInfos_, 헬퍼 선언)
- Modify: `analyzer/type_checker.cpp` (ClassStatement 등록 확장, typeFromToken, inferCallExpression)
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성**

```cpp
// ---- plan B-2 Task 2: InstanceType 적용 + 서브타이핑 ----

TEST(TypeCheckerTest, SubtypeAssignmentOk) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 동물:\n"
        "    문자 이름\n"
        "    생성(문자 이름):\n"
        "        자기.이름 = 이름\n"
        "클래스 강아지 < 동물:\n"
        "    함수 소리() -> 문자:\n"
        "        리턴 \"멍멍\"\n"
        "동물 a = 강아지(\"뽀삐\")\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, SiblingAssignmentRejected) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 동물:\n"
        "    문자 이름\n"
        "클래스 강아지 < 동물:\n"
        "    정수 나이\n"
        "클래스 고양이 < 동물:\n"
        "    정수 나이\n"
        "강아지 d = 고양이()\n");
    expectSingleDiagnostic(result, "TC001");
}
```

- [ ] **Step 2: 실패 확인** — 현재 생성자 결과가 Any라 SiblingAssignmentRejected가 0건으로 FAIL.
- [ ] **Step 3: 구현**

`type_checker.h` private에 (classTypes_ 선언 아래):

```cpp
// Phase B-2: 클래스 본문 정보 (spec D5). 필드/메서드는 자신 것만 — 부모는 lookup 시 체인 탐색.
struct ClassInfo {
    std::string name;
    std::string parentName;
    int constructorArity = 0;
    std::map<std::string, std::shared_ptr<Type>> fields;
    std::map<std::string, std::shared_ptr<FunctionType>> methods;
};
std::map<std::string, ClassInfo> classInfos_;
std::string currentClassName_;  // 클래스 본문 검사 중인 클래스 (자기.필드 대입용)

const ClassInfo* findClassInfo(const std::string& className) const;
std::vector<std::string> ancestorChainOf(const std::string& className) const;
std::shared_ptr<Type> instanceTypeOf(const std::string& className) const;
std::shared_ptr<Type> lookupField(const std::string& className, const std::string& field) const;
std::shared_ptr<FunctionType> lookupMethod(const std::string& className,
                                           const std::string& method) const;
```

`type_checker.cpp` 구현 (ClassStatement 처리 함수들 근처):

```cpp
const TypeChecker::ClassInfo* TypeChecker::findClassInfo(const std::string& className) const {
    auto it = classInfos_.find(className);
    return it == classInfos_.end() ? nullptr : &it->second;
}

std::vector<std::string> TypeChecker::ancestorChainOf(const std::string& className) const {
    std::vector<std::string> chain;
    const ClassInfo* info = findClassInfo(className);
    while (info && !info->parentName.empty()) {
        if (std::find(chain.begin(), chain.end(), info->parentName) != chain.end()) {
            break;  // 순환 상속 가드
        }
        chain.push_back(info->parentName);
        info = findClassInfo(info->parentName);
    }
    return chain;
}

std::shared_ptr<Type> TypeChecker::instanceTypeOf(const std::string& className) const {
    return makeInstance(className, ancestorChainOf(className));
}

std::shared_ptr<Type> TypeChecker::lookupField(const std::string& className,
                                               const std::string& field) const {
    for (const ClassInfo* info = findClassInfo(className); info;
         info = info->parentName.empty() ? nullptr : findClassInfo(info->parentName)) {
        auto found = info->fields.find(field);
        if (found != info->fields.end()) return found->second;
    }
    return nullptr;
}

std::shared_ptr<FunctionType> TypeChecker::lookupMethod(const std::string& className,
                                                        const std::string& method) const {
    for (const ClassInfo* info = findClassInfo(className); info;
         info = info->parentName.empty() ? nullptr : findClassInfo(info->parentName)) {
        auto found = info->methods.find(method);
        if (found != info->methods.end()) return found->second;
    }
    return nullptr;
}
```

ClassStatement 분기 확장 — 기존 등록 코드 뒤에 ClassInfo 구성 추가 (본문 검사는 Task 4):

```cpp
ClassInfo info;
info.name = cls->name;
info.parentName = cls->parentName;
info.constructorArity = constructorArity;  // 기존 계산값 (상속 포함)
for (size_t i = 0; i < cls->fieldNames.size() && i < cls->fieldTypes.size(); i++) {
    info.fields[cls->fieldNames[i]] = typeFromToken(cls->fieldTypes[i], false);
}
for (const auto& method : cls->methods) {
    if (!method) continue;
    std::vector<std::shared_ptr<Type>> params;
    std::vector<bool> paramHasDefault;
    std::vector<std::string> paramNames;
    for (size_t i = 0; i < method->parameters.size(); i++) {
        bool optional = i < method->parameterOptionals.size() && method->parameterOptionals[i];
        params.push_back(typeFromToken(
            i < method->parameterTypes.size() ? method->parameterTypes[i] : nullptr, optional));
        paramHasDefault.push_back(i < method->defaultValues.size()
                                  && method->defaultValues[i] != nullptr);
        paramNames.push_back(method->parameters[i] ? method->parameters[i]->name : "");
    }
    auto funcType = std::make_shared<FunctionType>(
        params, typeFromToken(method->returnType, method->returnTypeOptional), paramHasDefault);
    funcType->paramNames = paramNames;
    info.methods[method->name] = funcType;
}
classInfos_[cls->name] = std::move(info);
```

`typeFromToken`의 IDENTIFIER 분기 변경 — 선언 위치의 클래스 이름은 인스턴스 타입:

```cpp
case TokenType::IDENTIFIER: {
    auto found = classTypes_.find(tok->text);
    base = (found != classTypes_.end()) ? instanceTypeOf(tok->text) : makeAny();
    break;
}
```

`inferCallExpression`의 ClassType 분기 — 결과를 Any 대신 인스턴스로:

```cpp
return instanceTypeOf(cls->name);
```

- [ ] **Step 4: 빌드 + 전체 회귀** — 기존 클래스 테스트(ClassConstructorOk, TC501_MemberAccessOnOptionalClass 등) 통과 확인 (멤버 접근은 아직 Any 경로)
- [ ] **Step 5: Commit** — `feat(types): ClassInfo registry + InstanceType in declarations/constructor results (Phase B-2 Task 2)`

### Task 3: 멤버 접근/메서드 호출 검사 (TC201)

**Files:**
- Modify: `analyzer/type_checker.h` (`void warnUnknownMember(const std::string& className, const std::string& member);` 선언)
- Modify: `analyzer/type_checker.cpp` (MemberAccess/MethodCall 분기)
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성**

```cpp
// ---- plan B-2 Task 3: TC201 미지 멤버 + 멤버 타입 ----

TEST(TypeCheckerTest, TC201_UnknownField) {
    TypeChecker tc;
    auto result = checkSource(tc, std::string(kPointClass) + "점 p = 점(1, 2)\n출력(p.없는것)\n");
    expectSingleDiagnostic(result, "TC201");
}

TEST(TypeCheckerTest, TC201_UnknownMethod) {
    TypeChecker tc;
    auto result = checkSource(tc, std::string(kPointClass) + "점 p = 점(1, 2)\np.없는것()\n");
    expectSingleDiagnostic(result, "TC201");
}

TEST(TypeCheckerTest, FieldTypeInferred) {
    TypeChecker tc;
    // p.x는 정수 필드 — 문자 변수에 대입하면 TC001
    auto result = checkSource(tc, std::string(kPointClass) + "점 p = 점(1, 2)\n문자 s = p.x\n");
    expectSingleDiagnostic(result, "TC001");
}

TEST(TypeCheckerTest, MethodReturnTypeInferred) {
    TypeChecker tc;
    auto result = checkSource(tc, std::string(kPointClass) + "점 p = 점(1, 2)\n문자 s = p.합()\n");
    expectSingleDiagnostic(result, "TC001");  // 합() -> 정수
}

TEST(TypeCheckerTest, TC101_MethodArity) {
    TypeChecker tc;
    auto result = checkSource(tc, std::string(kPointClass) + "점 p = 점(1, 2)\np.합(1)\n");
    expectSingleDiagnostic(result, "TC101");
}

TEST(TypeCheckerTest, InheritedMemberOk) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 동물:\n"
        "    문자 이름\n"
        "    생성(문자 이름):\n"
        "        자기.이름 = 이름\n"
        "    함수 소개() -> 문자:\n"
        "        리턴 자기.이름\n"
        "클래스 강아지 < 동물:\n"
        "    함수 소리() -> 문자:\n"
        "        리턴 \"멍멍\"\n"
        "강아지 d = 강아지(\"뽀삐\")\n"
        "문자 n = d.이름\n"
        "문자 s = d.소개()\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, BuiltinMethodChainingSilent) {
    TypeChecker tc;
    // 내장 타입 메서드 체이닝 (evaluator methodMap) — 좌항이 InstanceType이 아니므로 침묵
    auto result = checkSource(tc, "문자 s = \"가나\"\n출력(s.길이())\n");
    EXPECT_TRUE(result.diagnostics.empty());
}
```

(주의: kPointClass는 Task 10에서 정의된 기존 익명 namespace 상수 재사용.)

- [ ] **Step 2: 실패 확인** — TC201/FieldType/MethodReturn/TC101_MethodArity FAIL.
- [ ] **Step 3: 구현** — `warnUnknownMember`:

```cpp
void TypeChecker::warnUnknownMember(const std::string& className, const std::string& member) {
    warn(currentLine_, "TC201",
         "클래스 '" + className + "'에 '" + member + "' 멤버가 없습니다.");
}
```

MemberAccessExpression 분기 교체:

```cpp
if (auto* member = dynamic_cast<MemberAccessExpression*>(expr.get())) {
    auto objectType = inferExpression(member->object);
    if (auto* opt = dynamic_cast<OptionalType*>(objectType.get())) {
        warnUnresolvedOptional(*opt);  // TC501 우선 (기존)
        return makeAny();
    }
    if (auto* inst = dynamic_cast<InstanceType*>(objectType.get())) {
        if (auto fieldType = lookupField(inst->className, member->member)) {
            return fieldType;
        }
        if (lookupMethod(inst->className, member->member)) {
            return makeAny();  // 메서드 참조 (호출 없이) — Phase B-2는 Any
        }
        if (findClassInfo(inst->className)) {  // 미지 클래스(정보 없음)는 침묵
            warnUnknownMember(inst->className, member->member);
            return makeNever();
        }
    }
    return makeAny();
}
```

MethodCallExpression 분기 교체:

```cpp
if (auto* methodCall = dynamic_cast<MethodCallExpression*>(expr.get())) {
    auto objectType = inferExpression(methodCall->object);
    std::vector<std::shared_ptr<Type>> argTypes;
    for (const auto& arg : methodCall->arguments) {
        argTypes.push_back(inferExpression(arg));
    }
    if (auto* opt = dynamic_cast<OptionalType*>(objectType.get())) {
        warnUnresolvedOptional(*opt);
        return makeAny();
    }
    auto* inst = dynamic_cast<InstanceType*>(objectType.get());
    if (!inst || !findClassInfo(inst->className)) {
        return makeAny();  // Any/내장 타입 메서드(evaluator methodMap) 등 — Phase B-2 침묵
    }
    auto method = lookupMethod(inst->className, methodCall->method);
    if (!method) {
        warnUnknownMember(inst->className, methodCall->method);
        return makeNever();
    }
    checkCallArguments(*method, methodCall->method, argTypes);  // TC101/TC102 (아래 추출)
    return method->ret;
}
```

기존 `inferCallExpression`의 FunctionType arity/타입 검사 블록을 `void checkCallArguments(const FunctionType& func, const std::string& calleeName, const std::vector<std::shared_ptr<Type>>& argTypes)`로 추출해 양쪽에서 재사용 (헤더 선언 추가).

**주의:** 문자/배열/사전의 내장 메서드 체이닝(`"a".길이()` — evaluator methodMap)은 InstanceType이 아니므로 자동 침묵 ✓.

- [ ] **Step 4: 빌드 + 전체 회귀 + 골든 스윕** (classes_basic의 `p.x`/`p.합()`이 이제 실타입 경로 — 0건 유지 확인)
- [ ] **Step 5: Commit** — `feat(types): TC201 unknown member + member type inference (Phase B-2 Task 3)`

### Task 4: 클래스 본문 검사 (생성자/메서드/자기)

**Files:**
- Modify: `analyzer/type_checker.h` (`void checkClassBody(ClassStatement& cls);`, `void checkFunctionLike(FunctionStatement& fn, bool declareName);` 선언)
- Modify: `analyzer/type_checker.cpp`
- Test: `tests/type_checker_test.cpp`

- [ ] **Step 1: 실패하는 테스트 작성**

```cpp
// ---- plan B-2 Task 4: 클래스 본문 검사 ----

TEST(TypeCheckerTest, TC002_SelfFieldTypeMismatch) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 점:\n"
        "    정수 x\n"
        "    생성(정수 a):\n"
        "        자기.x = \"문자\"\n");
    expectSingleDiagnostic(result, "TC002");
}

TEST(TypeCheckerTest, TC103_MethodReturnMismatch) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 점:\n"
        "    정수 x\n"
        "    생성(정수 a):\n"
        "        자기.x = a\n"
        "    함수 이름() -> 문자:\n"
        "        리턴 자기.x\n");
    expectSingleDiagnostic(result, "TC103");
}

TEST(TypeCheckerTest, DynamicFieldRegistered) {
    TypeChecker tc;
    // 부록 C: 동적 필드는 양 런타임 허용 — 진단 없이 등록되고 메서드에서 보임
    auto result = checkSource(tc,
        "클래스 동적:\n"
        "    정수 x\n"
        "    생성(정수 a):\n"
        "        자기.x = a\n"
        "        자기.y = a + 1\n"
        "    함수 와이() -> 정수:\n"
        "        리턴 자기.y\n"
        "동적 d = 동적(5)\n"
        "출력(d.와이())\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, TC006_DirectMethodCallInBody) {
    TypeChecker tc;
    // 자기 없는 메서드 직접 호출은 양 런타임 거부 (부록 C) — TC006 정당
    auto result = checkSource(tc,
        "클래스 직접:\n"
        "    정수 x\n"
        "    생성(정수 a):\n"
        "        자기.x = a\n"
        "    함수 첫째() -> 정수:\n"
        "        리턴 둘째()\n"
        "    함수 둘째() -> 정수:\n"
        "        리턴 9\n");
    expectSingleDiagnostic(result, "TC006");
}

TEST(TypeCheckerTest, MethodMutualReferenceViaSelf) {
    TypeChecker tc;
    auto result = checkSource(tc,
        "클래스 상호:\n"
        "    정수 x\n"
        "    생성(정수 a):\n"
        "        자기.x = a\n"
        "    함수 첫째() -> 정수:\n"
        "        리턴 자기.둘째()\n"
        "    함수 둘째() -> 정수:\n"
        "        리턴 7\n");
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TypeCheckerTest, MethodBodyNowTraversed) {
    TypeChecker tc;
    // Phase A의 MethodBodyNotTraversed 의도 반전: `자기.이름 + 1` (문자+정수)은 TC601
    auto result = checkSource(tc,
        "클래스 동물:\n"
        "    문자 이름\n"
        "    생성(문자 이름):\n"
        "        자기.이름 = 이름\n"
        "    함수 인사() -> 정수:\n"
        "        리턴 자기.이름 + 1\n");
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0].code, "TC601");
}
```

기존 `MethodBodyNotTraversed` 테스트는 삭제 (위 MethodBodyNowTraversed로 대체 — Phase A 한계 문서화 테스트였음).

- [ ] **Step 2: 실패 확인**
- [ ] **Step 3: 구현**

`checkFunctionStatement`를 분리 — 기존 본문을 `checkFunctionLike(fn, /*declareName=*/...)`로 바꾸고 declare 부분만 조건화:

```cpp
void TypeChecker::checkFunctionStatement(FunctionStatement& fn) {
    checkFunctionLike(fn, true);
}

// declareName=false: 클래스 메서드 — 이름을 스코프에 올리지 않는다
// (자기 없는 직접 호출은 양 런타임 거부 — 부록 C)
void TypeChecker::checkFunctionLike(FunctionStatement& fn, bool declareName) {
    // (기존 checkFunctionStatement 본문 그대로, 단 declare(fn.name, funcType)를
    //  `if (declareName) declare(fn.name, funcType);`로 감싼다)
}
```

ClassStatement 분기 끝(classInfos_ 등록 후)에 본문 검사 추가:

```cpp
checkClassBody(*cls);
```

```cpp
void TypeChecker::checkClassBody(ClassStatement& cls) {
    auto self = instanceTypeOf(cls.name);
    auto prevClass = currentClassName_;
    currentClassName_ = cls.name;

    // `부모` 키워드는 부모 InstanceType으로 — 메서드/생성자 본문의 TC006 오탐 방지 (spec D5)
    std::shared_ptr<Type> parentType =
        cls.parentName.empty() ? nullptr : instanceTypeOf(cls.parentName);

    if (cls.constructorBody) {
        pushScope();
        declare("자기", self);
        if (parentType) declare("부모", parentType);
        for (size_t i = 0; i < cls.constructorParams.size(); i++) {
            if (!cls.constructorParams[i]) continue;
            declare(cls.constructorParams[i]->name,
                    typeFromToken(i < cls.constructorParamTypes.size()
                                      ? cls.constructorParamTypes[i] : nullptr, false));
        }
        checkStatement(cls.constructorBody);
        popScope();
    }
    for (const auto& method : cls.methods) {
        if (!method) continue;
        pushScope();
        declare("자기", self);
        if (parentType) declare("부모", parentType);
        checkFunctionLike(*method, false);
        popScope();
    }

    currentClassName_ = prevClass;
}
```

AssignmentStatement 분기 선두에 `자기.필드` 대입 처리 추가 (parser가 `자기.x = v`를 name="자기.x"인 AssignmentStatement로 만듦):

```cpp
static const std::string kSelfPrefix = "자기.";
if (assign->name.rfind(kSelfPrefix, 0) == 0 && !currentClassName_.empty()) {
    std::string field = assign->name.substr(kSelfPrefix.size());
    auto valueType = inferExpression(assign->value);
    if (auto fieldType = lookupField(currentClassName_, field)) {
        if (!fieldType->isAssignableFrom(*valueType)) {
            warn(currentLine_, "TC002",
                 "'" + fieldType->toKorean() + "' 타입 변수 '" + assign->name + "'에 '"
                     + valueType->toKorean() + "' 값을 대입할 수 없습니다.");
        }
    } else {
        // 동적 필드 등록 (부록 C — 양 런타임 허용). 첫 대입 값으로 타입 추론.
        classInfos_[currentClassName_].fields[field] = valueType;
    }
    return;
}
```

`자기.x` 읽기는 MemberAccessExpression(object=Identifier("자기")) — 메서드 스코프의 자기(InstanceType) declare로 기존 Task 3 경로가 처리 ✓.

- [ ] **Step 4: 빌드 + 전체 회귀 + 골든 스윕** (classes_basic 본문 진입으로 0건 유지 확인 — 핵심 게이트)
- [ ] **Step 5: Commit** — `feat(types): class body checking — constructor/methods/자기 (Phase B-2 Task 4)`

### Task 5: type_checker.cpp 3분할 (행동 불변 리팩토링)

**Files:**
- Create: `analyzer/type_checker_util.h` (nodeLine/isPrimKind/isNumeric — inline)
- Create: `analyzer/type_checker_expr.cpp` (inferExpression/inferInfixExpression/inferCallExpression/checkCallArguments/collectNarrowings)
- Create: `analyzer/type_checker_class.cpp` (ClassStatement 등록·checkClassBody·findClassInfo/ancestorChainOf/instanceTypeOf/lookupField/lookupMethod·warnUnknownMember)
- Modify: `analyzer/type_checker.cpp` (나머지: check/checkStatement/checkFunctionLike/스코프/진단/typeFromToken/registerBuiltins)
- Modify: `CMakeLists.txt` (HONGIK_CORE_SOURCES에 2개 .cpp 추가)

- [ ] **Step 1: 순수 이동만** — 코드 수정 없이 함수 단위로 이동. ClassStatement 분기는 `checkStatement`에서 `checkClassStatement(*cls)` 호출로 추출해 class 파일로.
- [ ] **Step 2: 빌드 + 전체 ctest 통과 (개수 동일 — 행동 불변 게이트)**
- [ ] **Step 3: Commit** — `refactor(analyzer): split type_checker.cpp by concern (Phase B-2 Task 5)`

### Task 6: 종합 게이트 + 문서

- [ ] **Step 1: 전체 ctest + 골든 스윕** (error_undefined 외 clean)
- [ ] **Step 2: REPL 확인** — 클래스 선언 후 미지 멤버 접근이 TC201 1줄 + 프롬프트 복귀
- [ ] **Step 3: CHANGELOG** — Phase B-2 항목 (TC201, InstanceType 서브타이핑, 클래스 본문 검사, 생성자 상속 오탐 수정 포함)
- [ ] **Step 4: DoD 확인** — evaluator/vm/environment/object(시그니처 제외)/lexer/parser/ast 무변경 (B-2는 ast/parser도 무변경), spec 부록 C와 구현 일치
- [ ] **Step 5: Commit**

---

## 완료 기준 (Definition of Done)

1. 전체 ctest 통과 (기존 461 + 신규)
2. classes_basic 등 골든 스윕 clean (본문 진입 후에도)
3. TC201 positive/negative, 서브타이핑 양방향, 동적 필드, 직접 호출 TC006, 메서드 본문 TC103/TC601 테스트 존재
4. 런타임 파일 무변경 (`git diff --stat`), Type 계층은 TypeChecker 상태 비참조 (ancestors 스냅샷)
5. type_checker 3분할 후에도 테스트 개수·결과 동일
6. CHANGELOG 갱신

## Task 의존 그래프

```
Task 0 (실측, 완료) → Task 1 (InstanceType) → Task 2 (ClassInfo 적용) → Task 3 (TC201)
                                                                       → Task 4 (본문 검사)  [Task 3의 멤버 경로 의존]
Task 5 (분할) ← Task 4
Task 6 (종합) ← Task 5
```
