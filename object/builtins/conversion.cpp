#include "../built_in.h"

#include <stdexcept>
#include <string>

using namespace std;

// ===== 타입 변환 내장함수 =====

std::shared_ptr<Object> TypeOf::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("타입 함수는 인자를 1개만 받습니다.");
    }

    switch (parameters[0]->type) {
    case ObjectType::INTEGER: return make_shared<String>("정수");
    case ObjectType::FLOAT: return make_shared<String>("실수");
    case ObjectType::BOOLEAN: return make_shared<String>("논리");
    case ObjectType::STRING: return make_shared<String>("문자");
    case ObjectType::ARRAY: return make_shared<String>("배열");
    case ObjectType::HASH_MAP: return make_shared<String>("사전");
    case ObjectType::FUNCTION: return make_shared<String>("함수");
    case ObjectType::BUILTIN_FUNCTION: return make_shared<String>("내장함수");
    case ObjectType::NULL_OBJ: return make_shared<String>("없음");
    case ObjectType::TUPLE: return make_shared<String>("튜플");
    case ObjectType::CLASS_DEF: return make_shared<String>("클래스");
    case ObjectType::INSTANCE: return make_shared<String>("인스턴스");
    default: return make_shared<String>("알수없음");
    }
}

std::shared_ptr<Object> ToInteger::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("정수변환 함수는 인자를 1개만 받습니다.");
    }

    if (auto integer = dynamic_cast<Integer*>(parameters[0].get())) {
        return parameters[0];
    }
    if (auto float_obj = dynamic_cast<Float*>(parameters[0].get())) {
        return make_shared<Integer>(static_cast<long long>(float_obj->value));
    }
    if (auto str = dynamic_cast<String*>(parameters[0].get())) {
        try {
            return make_shared<Integer>(stoll(str->value));
        } catch (...) {
            throw runtime_error("문자열 '" + str->value + "'을(를) 정수로 변환할 수 없습니다.");
        }
    }
    if (auto boolean = dynamic_cast<Boolean*>(parameters[0].get())) {
        return make_shared<Integer>(boolean->value ? 1 : 0);
    }

    throw runtime_error("정수변환이 지원되지 않는 타입입니다.");
}

std::shared_ptr<Object> ToFloat::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("실수변환 함수는 인자를 1개만 받습니다.");
    }

    if (auto float_obj = dynamic_cast<Float*>(parameters[0].get())) {
        return parameters[0];
    }
    if (auto integer = dynamic_cast<Integer*>(parameters[0].get())) {
        return make_shared<Float>(static_cast<double>(integer->value));
    }
    if (auto str = dynamic_cast<String*>(parameters[0].get())) {
        try {
            return make_shared<Float>(stod(str->value));
        } catch (...) {
            throw runtime_error("문자열 '" + str->value + "'을(를) 실수로 변환할 수 없습니다.");
        }
    }

    throw runtime_error("실수변환이 지원되지 않는 타입입니다.");
}

std::shared_ptr<Object> ToString_::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("문자변환 함수는 인자를 1개만 받습니다.");
    }

    return make_shared<String>(parameters[0]->ToString());
}
