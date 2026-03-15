#ifndef OBJECT_H
#define OBJECT_H

#include "../ast/expressions.h"
#include "../ast/statements.h"
#include "object_type.h"
#include <map>
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

class Float final : public Object {
public:
    Float(double value) : value(value) {
        type = ObjectType::FLOAT;
    }

    double value;

    std::string ToString() override {
        std::string s = std::to_string(value);
        // 불필요한 후행 0 제거
        size_t dot = s.find('.');
        if (dot != std::string::npos) {
            size_t last = s.find_last_not_of('0');
            if (last == dot) last++;
            s = s.substr(0, last + 1);
        }
        return s;
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

class HashMap final : public Object {
public:
    std::map<std::string, std::shared_ptr<Object>> pairs;

    HashMap() {
        type = ObjectType::HASH_MAP;
    }

    std::string ToString() override {
        std::string s = "{";
        size_t i = 0;
        for (auto& [key, value] : pairs) {
            if (i > 0) s += ", ";
            s += "\"" + key + "\": " + value->ToString();
            i++;
        }
        s += "}";
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

class BreakSignal final : public Object {
public:
    BreakSignal() {
        type = ObjectType::BREAK_SIGNAL;
    }

    std::string ToString() override {
        return "";
    }
};

class Null final : public Object {
public:
    Null() {
        type = ObjectType::NULL_OBJ;
    }

    std::string ToString() override {
        return "없음";
    }
};

class Tuple final : public Object {
public:
    std::vector<std::shared_ptr<Object>> elements;

    Tuple() {
        type = ObjectType::TUPLE;
    }

    std::string ToString() override {
        std::string s = "(";
        for (size_t i = 0; i < elements.size(); i++) {
            if (i > 0) s += ", ";
            s += elements[i]->ToString();
        }
        s += ")";
        return s;
    }
};

class ClassDef : public Object {
public:
    std::string name;
    std::vector<std::shared_ptr<Token>> fieldTypes;
    std::vector<std::string> fieldNames;
    std::vector<std::shared_ptr<Token>> constructorParamTypes;
    std::vector<std::shared_ptr<IdentifierExpression>> constructorParams;
    std::shared_ptr<BlockStatement> constructorBody;
    std::vector<std::shared_ptr<Function>> methods;
    std::vector<std::string> methodNames;
    std::shared_ptr<Environment> env;

    ClassDef() {
        type = ObjectType::CLASS_DEF;
    }

    std::string ToString() override {
        return "클래스 " + name;
    }
};

class Instance final : public Object {
public:
    std::shared_ptr<ClassDef> classDef;
    std::shared_ptr<Environment> fields;

    Instance() {
        type = ObjectType::INSTANCE;
    }

    std::string ToString() override {
        return classDef->name + " 인스턴스";
    }
};

#endif // OBJECT_H
