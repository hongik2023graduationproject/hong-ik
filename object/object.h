#ifndef OBJECT_H
#define OBJECT_H

#include "../ast/expressions.h"
#include "../ast/statements.h"
#include "object_type.h"
#include <string>
#include <vector>

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
    Object* value;

    std::string ToString() override {
        return value->ToString();
    }
};

class Function final : public Object {
public:
    std::vector<Token*> parameterTypes;
    std::vector<IdentifierExpression*> parameters;
    BlockStatement* body;
    Environment* env;

    // TODO: 미구현 상태
    std::string ToString() override {
        std::string s = "함수: ";

        return s;
    }
};

class Array final : public Object {
public:
    std::vector<Object*> elements;

    std::string ToString() override {
        std::string s = "[";
        for (auto& element : elements) {
            s += element->ToString();
            s += ", ";
        }
        s += "]";
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

#endif // OBJECT_H
