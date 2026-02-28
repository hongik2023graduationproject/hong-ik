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
