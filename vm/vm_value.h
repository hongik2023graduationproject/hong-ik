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

// VM 스택에 들어가는 값. 정수/실수/불리언은 union으로 인라인 저장(heap 할당 회피),
// 그 외 객체는 shared_ptr로 별도 보관. 모든 쓰기는 정적 팩토리(Int/Float/Bool/Null/Obj)를
// 통해서만 일어나도록 union 멤버를 private으로 둔다 — tag와 활성 멤버가 항상 일치한다는
// 불변식을 외부에서 깨뜨릴 수 없도록 만든다.
struct VMValue {
    VMValue() : tag_(ValueTag::NULL_V), intVal_(0) {}

    static VMValue Int(long long v) {
        VMValue val;
        val.tag_ = ValueTag::INT;
        val.intVal_ = v;
        return val;
    }

    static VMValue Float(double v) {
        VMValue val;
        val.tag_ = ValueTag::FLOAT;
        val.floatVal_ = v;
        return val;
    }

    static VMValue Bool(bool v) {
        VMValue val;
        val.tag_ = ValueTag::BOOL;
        val.boolVal_ = v;
        return val;
    }

    static VMValue Null() {
        VMValue val;
        val.tag_ = ValueTag::NULL_V;
        return val;
    }

    static VMValue Obj(std::shared_ptr<Object> o) {
        VMValue val;
        val.tag_ = ValueTag::OBJECT;
        val.objVal_ = std::move(o);
        return val;
    }

    // 태그 술어. 활성 union 멤버 식별 + 분기에서 사용.
    bool isInt() const { return tag_ == ValueTag::INT; }
    bool isFloat() const { return tag_ == ValueTag::FLOAT; }
    bool isBool() const { return tag_ == ValueTag::BOOL; }
    bool isNull() const { return tag_ == ValueTag::NULL_V; }
    bool isObject() const { return tag_ == ValueTag::OBJECT; }
    ValueTag kind() const { return tag_; }

    // 활성 멤버 읽기. 호출 측이 tag를 먼저 확인해야 한다 (잘못된 호출은 UB).
    // hot path 인라인 후 직접 union 접근과 동일한 코드로 컴파일된다.
    long long asInt() const { return intVal_; }
    double asFloat() const { return floatVal_; }
    bool asBool() const { return boolVal_; }
    const std::shared_ptr<Object>& asObject() const { return objVal_; }

    // VMValue -> shared_ptr<Object> (외부 인터페이스용).
    std::shared_ptr<Object> toObject() const {
        switch (tag_) {
        case ValueTag::INT: return std::make_shared<Integer>(intVal_);
        case ValueTag::FLOAT: return std::make_shared<::Float>(floatVal_);
        case ValueTag::BOOL: return std::make_shared<Boolean>(boolVal_);
        case ValueTag::NULL_V: return std::make_shared<::Null>();
        case ValueTag::OBJECT: return objVal_;
        }
        return std::make_shared<::Null>();
    }

    // shared_ptr<Object> -> VMValue. nullptr는 Null로 정규화한다.
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

    // 에러 메시지용.
    std::string toString() const {
        return toObject()->ToString();
    }

private:
    ValueTag tag_;
    union {
        long long intVal_;
        double floatVal_;
        bool boolVal_;
    };
    std::shared_ptr<Object> objVal_; // tag_ == OBJECT일 때만 유효
};

#endif // VM_VALUE_H
