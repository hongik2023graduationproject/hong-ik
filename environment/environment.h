#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "../object/object.h"
#include "../object/object_type.h"
#include <map>
#include <memory>
#include <set>
#include <string>

// Environment
// scope 내의 변수들을 저장하는 환경
class Environment : public std::enable_shared_from_this<Environment> {
public:
    std::map<std::string, std::shared_ptr<Object>> store;
    std::set<std::string> optionalVars;
    std::map<std::string, ObjectType> typeMap;
    std::shared_ptr<Environment> outer;

    Environment() = default;
    explicit Environment(std::shared_ptr<Environment> outer) : outer(std::move(outer)) {}

    std::shared_ptr<Object> Get(const std::string& name);

    std::shared_ptr<Object> Set(const std::string& name, std::shared_ptr<Object> object);

    void SetOptional(const std::string& name);

    bool IsOptional(const std::string& name);

    // 외부 스코프까지 탐색하여 변수를 업데이트
    std::shared_ptr<Object> Update(const std::string& name, std::shared_ptr<Object> object);

    // 타입 추적
    void SetType(const std::string& name, ObjectType type);
    ObjectType GetType(const std::string& name);
    bool HasType(const std::string& name);
};

#endif // ENVIRONMENT_H
