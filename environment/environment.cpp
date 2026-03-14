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

std::shared_ptr<Object> Environment::Update(const std::string& name, std::shared_ptr<Object> object) {
    auto it = store.find(name);
    if (it != store.end()) {
        store[name] = object;
        return object;
    }
    if (outer != nullptr) {
        return outer->Update(name, object);
    }
    // 찾지 못한 경우 현재 스코프에 설정
    store[name] = object;
    return object;
}
