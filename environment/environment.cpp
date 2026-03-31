#include "environment.h"

std::shared_ptr<Object> Environment::Get(const std::string& name) {
    auto it = store.find(name);
    if (it != store.end()) {
        return it->second;
    }
    if (outer != nullptr) {
        return outer->Get(name);
    }
    return nullptr;
}

std::shared_ptr<Object> Environment::Set(const std::string& name, std::shared_ptr<Object> object) {
    store[name] = object;
    return object;
}

void Environment::SetOptional(const std::string& name) {
    optionalVars.insert(name);
}

bool Environment::IsOptional(const std::string& name) {
    if (optionalVars.count(name)) return true;
    if (outer != nullptr) return outer->IsOptional(name);
    return false;
}

std::shared_ptr<Object> Environment::Update(const std::string& name, std::shared_ptr<Object> object) {
    auto it = store.find(name);
    if (it != store.end()) {
        store[name] = object;
        return object;
    }
    if (outer != nullptr) {
        return outer->Update(name, object);
    }
    // 찾지 못한 경우: 선언되지 않은 변수의 업데이트이므로 아무것도 하지 않고 nullptr 반환
    return nullptr;
}

void Environment::SetType(const std::string& name, ObjectType type) {
    typeMap[name] = type;
}

ObjectType Environment::GetType(const std::string& name) {
    auto it = typeMap.find(name);
    if (it != typeMap.end()) {
        return it->second;
    }
    if (outer != nullptr) {
        return outer->GetType(name);
    }
    return ObjectType::NULL_OBJ;
}

bool Environment::HasType(const std::string& name) {
    auto it = typeMap.find(name);
    if (it != typeMap.end()) {
        return true;
    }
    if (outer != nullptr) {
        return outer->HasType(name);
    }
    return false;
}
