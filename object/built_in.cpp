#include "built_in.h"

#include <iostream>

using namespace std;


Object* Length::function(std::vector<Object*> parameters) {
    if (parameters.size() != 1) {
        // ERROR
    }

    if (auto string = dynamic_cast<String*>(parameters[0])) {
        return new Integer(string->value.size());
    }
    if (auto array = dynamic_cast<Array*>(parameters[0])) {
        return new Integer(array->elements.size());
    }

    // ERR
}

Object* Print::function(std::vector<Object*> parameters) {
    for (int i = 0; i < parameters.size(); i++) {
        if (auto string = dynamic_cast<String*>(parameters[i])) {
            cout << string->value;
        }
        if (auto integer = dynamic_cast<Integer*>(parameters[i])) {
            cout << integer->value;
        }
        if (auto boolean = dynamic_cast<Boolean*>(parameters[i])) {
            cout << boolean->value;
        }
    }
}

Object* Push::function(std::vector<Object*> parameters) {
    if (parameters.size() != 2) {
        // ERR
    }
    if (auto array = dynamic_cast<Array*>(parameters[0])) {
        // array type 검사 추가 예정
        array->elements.push_back(parameters[1]);
    }

    // ERR
}
