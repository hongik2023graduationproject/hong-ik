#ifndef OBJECT_H
#define OBJECT_H

#include <string>
#include <vector>

#include "object_type.h"
#include "../ast/expressions.h"
#include "../ast/statements.h"

// 전방 선언, 순환 참조가 발생해 전방 선언 사용
class Environment;


class Object {
public:
    virtual ~Object() = default;

    ObjectType type;

    virtual std::string ToString() = 0;
};

class Integer final : public Object {
public:
    Integer(long long value) : value(value) {
        type = ObjectType::INTEGER;
    }

    long long value;

    std::string ToString() override {
        return std::to_string(value);
    }
};

class Boolean final : public Object {
public:
    Boolean(bool value) : value(value) {
        type = ObjectType::BOOLEAN;
    }

    bool value;

    std::string ToString() override {
        return value ? "true" : "false";
    }
};

class String final : public Object {
public:
    String(std::string value) : value(value) {
        type = ObjectType::STRING;
    }

    std::string value;

    std::string ToString() override {
        return value;
    }
};

class ReturnValue final : public Object {
public:
    Object *value;

    std::string ToString() override {
        return value->ToString();
    }
};

class Function final : public Object {
public:
    std::vector<Expression *> parameters;
    BlockStatement *body;
    Environment *env;

    // TODO: 미구현 상태
    std::string ToString() override {
        std::string s = "함수: ";

        return s;
    }
};


class Builtin : public Object {
public:
    virtual Object* function(std::vector<Object*> args) = 0;

    std::string ToString() override {
        return "";
    }
};

#endif //OBJECT_H
