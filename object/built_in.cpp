#include "built_in.h"

#include <iostream>
#include <stdexcept>

using namespace std;


std::shared_ptr<Object> Length::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("길이 함수는 인자를 1개만 받습니다.");
    }

    if (auto str = dynamic_cast<String*>(parameters[0].get())) {
        return make_shared<Integer>(str->value.size());
    }
    if (auto array = dynamic_cast<Array*>(parameters[0].get())) {
        return make_shared<Integer>(array->elements.size());
    }

    throw runtime_error("길이 함수는 문자열 또는 배열만 지원합니다.");
}

std::shared_ptr<Object> Print::function(std::vector<std::shared_ptr<Object>> parameters) {
    for (size_t i = 0; i < parameters.size(); i++) {
        if (auto str = dynamic_cast<String*>(parameters[i].get())) {
            cout << str->value << endl;
        } else if (auto integer = dynamic_cast<Integer*>(parameters[i].get())) {
            cout << integer->value << endl;
        } else if (auto boolean = dynamic_cast<Boolean*>(parameters[i].get())) {
            cout << (boolean->value ? "true" : "false") << endl;
        } else if (auto array = dynamic_cast<Array*>(parameters[i].get())) {
            cout << array->ToString() << endl;
        } else if (auto func = dynamic_cast<Function*>(parameters[i].get())) {
            cout << func->ToString() << endl;
        } else {
            throw runtime_error("출력 함수에서 지원하지 않는 타입입니다.");
        }
    }

    return nullptr;
}

std::shared_ptr<Object> Push::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("추가 함수는 인자를 2개 받습니다.");
    }
    if (auto array = dynamic_cast<Array*>(parameters[0].get())) {
        array->elements.push_back(parameters[1]);
        return nullptr;
    }

    throw runtime_error("추가 함수의 첫 번째 인자는 배열이어야 합니다.");
}
