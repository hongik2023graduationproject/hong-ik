#include "type.h"

#include "type_utils.h"

#include <algorithm>
#include <climits>
#include <sstream>

// ---- PrimType ----

bool PrimType::isAssignableFrom(const Type& other) const {
    if (dynamic_cast<const NeverType*>(&other)) return true;
    if (dynamic_cast<const AnyType*>(&other)) return true;
    if (auto* p = dynamic_cast<const PrimType*>(&other)) {
        return kind == p->kind;
    }
    // NULL_OBJ는 OptionalType에서만 허용. Non-null PrimType은 거부.
    // 정수 ← 실수, 실수 ← 정수 자동 승격: Phase A에서 거부 (spec 1.1, Task 0 결과 박제).
    return false;
}

std::string PrimType::toKorean() const {
    return typeToKorean(kind);
}

bool PrimType::equals(const Type& other) const {
    auto* p = dynamic_cast<const PrimType*>(&other);
    return p && kind == p->kind;
}

// ---- OptionalType ----

bool OptionalType::isAssignableFrom(const Type& other) const {
    if (dynamic_cast<const NeverType*>(&other)) return true;
    if (dynamic_cast<const AnyType*>(&other)) return true;
    // T? ← 없음
    if (auto* p = dynamic_cast<const PrimType*>(&other); p && p->kind == ObjectType::NULL_OBJ) {
        return true;
    }
    // T? ← U? iff T ← U
    if (auto* o = dynamic_cast<const OptionalType*>(&other)) {
        return inner->isAssignableFrom(*o->inner);
    }
    // T? ← T (Non-null → Optional 좁히기)
    return inner->isAssignableFrom(other);
}

std::string OptionalType::toKorean() const {
    return inner->toKorean() + "?";
}

bool OptionalType::equals(const Type& other) const {
    auto* o = dynamic_cast<const OptionalType*>(&other);
    return o && inner->equals(*o->inner);
}

// ---- FunctionType ----

bool FunctionType::isAssignableFrom(const Type& other) const {
    if (dynamic_cast<const NeverType*>(&other)) return true;
    if (dynamic_cast<const AnyType*>(&other)) return true;
    auto* f = dynamic_cast<const FunctionType*>(&other);
    if (!f) return false;
    // Phase A: invariant — 매개변수/반환 타입 모두 동일해야 함 (spec 1.2)
    if (params.size() != f->params.size()) return false;
    for (size_t i = 0; i < params.size(); i++) {
        if (!params[i]->equals(*f->params[i])) return false;
    }
    return ret->equals(*f->ret);
}

std::string FunctionType::toKorean() const {
    std::ostringstream oss;
    oss << "함수(";
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) oss << ", ";
        oss << params[i]->toKorean();
    }
    oss << ") -> " << ret->toKorean();
    return oss.str();
}

bool FunctionType::equals(const Type& other) const {
    return isAssignableFrom(other) && other.isAssignableFrom(*this);
}

// ---- BuiltinFunctionType ----

bool BuiltinFunctionType::isAssignableFrom(const Type& other) const {
    if (dynamic_cast<const NeverType*>(&other)) return true;
    if (dynamic_cast<const AnyType*>(&other)) return true;
    auto* b = dynamic_cast<const BuiltinFunctionType*>(&other);
    return b && name == b->name;
}

std::string BuiltinFunctionType::toKorean() const {
    return "내장함수 " + name;
}

bool BuiltinFunctionType::equals(const Type& other) const {
    auto* b = dynamic_cast<const BuiltinFunctionType*>(&other);
    return b && name == b->name;
}

// ---- ClassType ----

bool ClassType::isAssignableFrom(const Type& other) const {
    if (dynamic_cast<const NeverType*>(&other)) return true;
    if (dynamic_cast<const AnyType*>(&other)) return true;
    // Phase A: 단순 이름 일치. 정확한 서브타이핑은 Phase B.
    auto* c = dynamic_cast<const ClassType*>(&other);
    return c && name == c->name;
}

std::string ClassType::toKorean() const {
    return name;
}

bool ClassType::equals(const Type& other) const {
    auto* c = dynamic_cast<const ClassType*>(&other);
    return c && name == c->name;
}

// ---- InstanceType ----

bool InstanceType::isAssignableFrom(const Type& other) const {
    if (dynamic_cast<const NeverType*>(&other)) return true;
    if (dynamic_cast<const AnyType*>(&other)) return true;
    auto* inst = dynamic_cast<const InstanceType*>(&other);
    if (!inst) return false;
    if (className == inst->className) return true;
    // 부모 <- 자식: other의 조상 체인에 자신이 있으면 허용 (spec D5, VM 시맨틱 — 부록 B #6)
    return std::find(inst->ancestors.begin(), inst->ancestors.end(), className)
           != inst->ancestors.end();
}

std::string InstanceType::toKorean() const {
    return className;
}

bool InstanceType::equals(const Type& other) const {
    auto* inst = dynamic_cast<const InstanceType*>(&other);
    return inst && className == inst->className;
}

// ---- AnyType ----

bool AnyType::isAssignableFrom(const Type&) const { return true; }
std::string AnyType::toKorean() const { return "임의"; }
bool AnyType::equals(const Type& other) const {
    return dynamic_cast<const AnyType*>(&other) != nullptr;
}

// ---- NeverType ----

bool NeverType::isAssignableFrom(const Type&) const { return true; }
std::string NeverType::toKorean() const { return "분석실패"; }
bool NeverType::equals(const Type& other) const {
    return dynamic_cast<const NeverType*>(&other) != nullptr;
}

// ---- 헬퍼 팩토리 ----

std::shared_ptr<Type> makePrim(ObjectType kind) {
    return std::make_shared<PrimType>(kind);
}

std::shared_ptr<Type> makeOptional(std::shared_ptr<Type> inner) {
    return std::make_shared<OptionalType>(std::move(inner));
}

std::shared_ptr<Type> makeFunction(std::vector<std::shared_ptr<Type>> params,
                                   std::shared_ptr<Type> ret,
                                   std::vector<bool> paramHasDefault) {
    return std::make_shared<FunctionType>(std::move(params), std::move(ret), std::move(paramHasDefault));
}

std::shared_ptr<Type> makeBuiltin(std::string name, int minArity, int maxArity,
                                  bool skipArgTypeCheck, std::shared_ptr<Type> ret) {
    return std::make_shared<BuiltinFunctionType>(std::move(name), minArity, maxArity,
                                                 skipArgTypeCheck, std::move(ret));
}

std::shared_ptr<Type> makeClass(std::string name, int constructorArity) {
    return std::make_shared<ClassType>(std::move(name), constructorArity);
}

std::shared_ptr<Type> makeInstance(std::string className, std::vector<std::string> ancestors) {
    return std::make_shared<InstanceType>(std::move(className), std::move(ancestors));
}

std::shared_ptr<Type> makeAny() {
    return std::make_shared<AnyType>();
}

std::shared_ptr<Type> makeNever() {
    return std::make_shared<NeverType>();
}
