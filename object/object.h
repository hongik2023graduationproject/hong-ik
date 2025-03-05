#ifndef OBJECT_H
#define OBJECT_H

#include <string>

#include "object_type.h"

class Object {
public:
    ObjectType type;

    virtual std::string String() = 0;
};

class Integer : public Object {
public:
    Integer(long long value) : value(value) {
        type = ObjectType::INTEGER;
    }

    long long value;

    std::string String() override {
        return std::to_string(value);
    }
};

class Boolean : public Object {
public:
    Boolean(bool value) : value(value) {
        type = ObjectType::BOOLEAN;
    }

    bool value;

    std::string String() override {
        return value ? "true" : "false";
    }
};


#endif //OBJECT_H
