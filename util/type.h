#ifndef TYPE_H
#define TYPE_H

#include "../object/object_type.h"
#include <memory>
#include <string>
#include <vector>

// Phase A 정적 타입 시스템의 1급 객체 모델.
// spec: docs/superpowers/specs/2026-05-19-static-type-system-design.md (D2)
//
// 런타임 객체(Object, ClassDef 등)와 독립적인 정적 분석 전용 표현.
// PrimType이 ObjectType enum을 재사용하는 것은 의도된 결합(spec D2.3).

class ClassDef;  // 전방 선언 (object/object.h)

class Type {
public:
    virtual ~Type() = default;

    // `other` 타입의 값이 `this` 타입 변수에 대입 가능한가?
    virtual bool isAssignableFrom(const Type& other) const = 0;

    virtual std::string toKorean() const = 0;

    virtual bool equals(const Type& other) const = 0;
};

class PrimType final : public Type {
public:
    ObjectType kind;

    explicit PrimType(ObjectType k) : kind(k) {}

    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};

class OptionalType final : public Type {
public:
    std::shared_ptr<Type> inner;

    explicit OptionalType(std::shared_ptr<Type> i) : inner(std::move(i)) {}

    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};

// 사용자 정의 함수 단일 시그니처.
// 매개변수가 Optional이면 params[i]가 OptionalType으로 래핑된다.
// paramHasDefault[i]는 기본값 보유 여부 (호출 인자 개수 검사용, Optional 여부와 무관).
class FunctionType final : public Type {
public:
    std::vector<std::shared_ptr<Type>> params;
    std::shared_ptr<Type> ret;
    std::vector<bool> paramHasDefault;
    std::vector<std::string> paramNames;  // 진단 메시지용 (spec D8 TC102) — 동등성 비교 불참여

    FunctionType(std::vector<std::shared_ptr<Type>> p,
                 std::shared_ptr<Type> r,
                 std::vector<bool> hasDef)
        : params(std::move(p)), ret(std::move(r)), paramHasDefault(std::move(hasDef)) {}

    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};

// 빌트인 함수. 다형 빌트인(`길이`, `포함` 등)을 위해 skipArgTypeCheck로 인자 타입 검사 우회 가능.
// 가변 인자: maxArity == INT_MAX 사용.
class BuiltinFunctionType final : public Type {
public:
    std::string name;
    int minArity;
    int maxArity;
    bool skipArgTypeCheck;
    std::shared_ptr<Type> ret;

    BuiltinFunctionType(std::string n, int minA, int maxA, bool skip, std::shared_ptr<Type> r)
        : name(std::move(n)), minArity(minA), maxArity(maxA),
          skipArgTypeCheck(skip), ret(std::move(r)) {}

    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};

// 클래스 타입. Phase A는 이름과 생성자 인자 개수만 사용.
class ClassType final : public Type {
public:
    std::string name;
    int constructorArity;
    std::shared_ptr<ClassDef> def;  // Phase A 미사용, Phase B에서 메서드/필드 lookup용

    ClassType(std::string n, int arity) : name(std::move(n)), constructorArity(arity) {}

    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};

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

// 모든 타입과 양방향 호환. 분석 불가/escape hatch.
class AnyType final : public Type {
public:
    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};

// 분석 실패 신호. 다른 타입과 양방향 호환(cascade 진단 차단).
class NeverType final : public Type {
public:
    bool isAssignableFrom(const Type& other) const override;
    std::string toKorean() const override;
    bool equals(const Type& other) const override;
};

// ---- 헬퍼 팩토리 ----

std::shared_ptr<Type> makePrim(ObjectType kind);
std::shared_ptr<Type> makeOptional(std::shared_ptr<Type> inner);
std::shared_ptr<Type> makeFunction(std::vector<std::shared_ptr<Type>> params,
                                   std::shared_ptr<Type> ret,
                                   std::vector<bool> paramHasDefault);
std::shared_ptr<Type> makeBuiltin(std::string name, int minArity, int maxArity,
                                  bool skipArgTypeCheck, std::shared_ptr<Type> ret);
std::shared_ptr<Type> makeClass(std::string name, int constructorArity);
std::shared_ptr<Type> makeInstance(std::string className, std::vector<std::string> ancestors);
std::shared_ptr<Type> makeAny();
std::shared_ptr<Type> makeNever();

#endif // TYPE_H
