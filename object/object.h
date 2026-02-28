#ifndef OBJECT_H
#define OBJECT_H

#include "../ast/expressions.h"
#include "../ast/statements.h"
#include "object_type.h"
#include <memory>
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
    String(std::string value) : value(std::move(value)) {
        type = ObjectType::STRING;
    }

    std::string value;

    std::string ToString() override {
        return value;
    }
};

class ReturnValue final : public Object {
public:
    std::shared_ptr<Object> value = nullptr;

    ReturnValue() {
        type = ObjectType::RETURN_VALUE;
    }

    std::string ToString() override {
        return value ? value->ToString() : "";
    }
};

class Function final : public Object {
public:
    std::vector<std::shared_ptr<Token>> parameterTypes;
    std::vector<std::shared_ptr<IdentifierExpression>> parameters;
    std::shared_ptr<BlockStatement> body;
    std::shared_ptr<Environment> env;
    std::shared_ptr<Token> returnType;

    Function() {
        type = ObjectType::FUNCTION;
    }

    std::string ToString() override {
        std::string s = "함수: ";
        return s;
    }
};

class Array final : public Object {
public:
    std::vector<std::shared_ptr<Object>> elements;

    Array() {
        type = ObjectType::ARRAY;
    }

    std::string ToString() override {
        std::string s = "[";
        for (size_t i = 0; i < elements.size(); i++) {
            if (i > 0) s += ", ";
            s += elements[i]->ToString();
        }
        s += "]";
        return s;
    }
};

class Builtin : public Object {
public:
    virtual std::shared_ptr<Object> function(std::vector<std::shared_ptr<Object>> args) = 0;
    std::vector<std::shared_ptr<Token>> parameterTypes;
    std::shared_ptr<Token> returnType;

    Builtin() {
        type = ObjectType::BUILTIN_FUNCTION;
    }

    std::string ToString() override {
        return "";
    }
};

#endif // OBJECT_H
