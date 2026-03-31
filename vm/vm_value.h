#ifndef VM_VALUE_H
#define VM_VALUE_H

#include "../object/object.h"
#include <memory>
#include <string>

enum class ValueTag : uint8_t {
    INT,
    FLOAT,
    BOOL,
    NULL_V,
    OBJECT  // String, Array, HashMap, Function, Closure, ClassDef, Instance, etc.
};

struct VMValue {
    ValueTag tag;
    union {
        long long intVal;
        double floatVal;
        bool boolVal;
    };
    std::shared_ptr<Object> objVal; // only used when tag == OBJECT

    VMValue() : tag(ValueTag::NULL_V), intVal(0) {}

    static VMValue Int(long long v) {
        VMValue val;
        val.tag = ValueTag::INT;
        val.intVal = v;
        return val;
    }

    static VMValue Float(double v) {
        VMValue val;
        val.tag = ValueTag::FLOAT;
        val.floatVal = v;
        return val;
    }

    static VMValue Bool(bool v) {
        VMValue val;
        val.tag = ValueTag::BOOL;
        val.boolVal = v;
        return val;
    }

    static VMValue Null() {
        VMValue val;
        val.tag = ValueTag::NULL_V;
        return val;
    }

    static VMValue Obj(std::shared_ptr<Object> o) {
        VMValue val;
        val.tag = ValueTag::OBJECT;
        val.objVal = std::move(o);
        return val;
    }

    // Convert VMValue to shared_ptr<Object> (for external interfaces)
    std::shared_ptr<Object> toObject() const {
        switch (tag) {
        case ValueTag::INT: return std::make_shared<Integer>(intVal);
        case ValueTag::FLOAT: return std::make_shared<::Float>(floatVal);
        case ValueTag::BOOL: return std::make_shared<Boolean>(boolVal);
        case ValueTag::NULL_V: return std::make_shared<::Null>();
        case ValueTag::OBJECT: return objVal;
        }
        return std::make_shared<::Null>();
    }

    // Convert shared_ptr<Object> to VMValue
    static VMValue fromObject(const std::shared_ptr<Object>& o) {
        if (!o) return Null();
        switch (o->type) {
        case ObjectType::INTEGER:
            return Int(static_cast<Integer*>(o.get())->value);
        case ObjectType::FLOAT:
            return Float(static_cast<::Float*>(o.get())->value);
        case ObjectType::BOOLEAN:
            return Bool(static_cast<Boolean*>(o.get())->value);
        case ObjectType::NULL_OBJ:
            return Null();
        default:
            return Obj(o);
        }
    }

    // Helper: get as string for error messages
    std::string toString() const {
        return toObject()->ToString();
    }
};

#endif // VM_VALUE_H
