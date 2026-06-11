// TypeChecker — 클래스 등록·본문 검사·멤버 lookup (spec D5, 부록 C 실측).
#include "type_checker.h"
#include "type_checker_util.h"

#include <algorithm>

// spec D5: 이름·생성자 arity 등록 + ClassInfo(필드/메서드 시그니처) 구성 후 본문 검사.
// 생성자 미정의(constructorBody 없음) 시 부모 생성자 arity 상속 (런타임 실측 2026-06-11)
void TypeChecker::checkClassStatement(ClassStatement& cls) {
    int constructorArity = static_cast<int>(cls.constructorParams.size());
    auto parent = cls.parentName.empty() ? classTypes_.end() : classTypes_.find(cls.parentName);
    if (!cls.constructorBody && parent != classTypes_.end()) {
        constructorArity = parent->second->constructorArity;
    }
    auto classType = std::make_shared<ClassType>(cls.name, constructorArity);
    classTypes_[cls.name] = classType;
    declare(cls.name, classType);
    if (!cls.parentName.empty() && parent == classTypes_.end()) {
        warn(currentLine_, "TC006", "식별자 '" + cls.parentName + "'를 찾을 수 없습니다.");
    }

    ClassInfo info;
    info.name = cls.name;
    info.parentName = cls.parentName;
    info.constructorArity = constructorArity;
    for (size_t i = 0; i < cls.fieldNames.size() && i < cls.fieldTypes.size(); i++) {
        info.fields[cls.fieldNames[i]] = typeFromToken(cls.fieldTypes[i], false);
    }
    for (const auto& method : cls.methods) {
        if (!method) {
            continue;
        }
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
    classInfos_[cls.name] = std::move(info);

    checkClassBody(cls);
}

// 클래스 본문 검사 (spec D5). 시그니처는 checkClassStatement에서 이미 classInfos_에 등록됨 —
// 메서드 상호 참조가 자기. 경유로 가능 (부록 C 실측).
void TypeChecker::checkClassBody(ClassStatement& cls) {
    auto self = instanceTypeOf(cls.name);
    auto prevClass = currentClassName_;
    currentClassName_ = cls.name;

    // `부모` 키워드는 부모 InstanceType으로 — 본문에서의 TC006 오탐 방지
    std::shared_ptr<Type> parentType =
        cls.parentName.empty() ? nullptr : instanceTypeOf(cls.parentName);

    if (cls.constructorBody) {
        pushScope();
        declare("자기", self);
        if (parentType) {
            declare("부모", parentType);
        }
        for (size_t i = 0; i < cls.constructorParams.size(); i++) {
            if (!cls.constructorParams[i]) {
                continue;
            }
            declare(cls.constructorParams[i]->name,
                    typeFromToken(i < cls.constructorParamTypes.size() ? cls.constructorParamTypes[i]
                                                                       : nullptr,
                                  false));
        }
        checkStatement(cls.constructorBody);
        popScope();
    }

    for (const auto& method : cls.methods) {
        if (!method) {
            continue;
        }
        pushScope();
        declare("자기", self);
        if (parentType) {
            declare("부모", parentType);
        }
        checkFunctionLike(*method, false);
        popScope();
    }

    currentClassName_ = prevClass;
}

// ---- 클래스 헬퍼 (spec D5) ----

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
        if (found != info->fields.end()) {
            return found->second;
        }
    }
    return nullptr;
}

std::shared_ptr<FunctionType> TypeChecker::lookupMethod(const std::string& className,
                                                        const std::string& method) const {
    for (const ClassInfo* info = findClassInfo(className); info;
         info = info->parentName.empty() ? nullptr : findClassInfo(info->parentName)) {
        auto found = info->methods.find(method);
        if (found != info->methods.end()) {
            return found->second;
        }
    }
    return nullptr;
}

void TypeChecker::warnUnknownMember(const std::string& className, const std::string& member) {
    warn(currentLine_, "TC201", "클래스 '" + className + "'에 '" + member + "' 멤버가 없습니다.");
}
