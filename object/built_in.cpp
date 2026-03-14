#include "built_in.h"

#include <fstream>
#include <iostream>
#include <sstream>
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
    if (auto hashmap = dynamic_cast<HashMap*>(parameters[0].get())) {
        return make_shared<Integer>(hashmap->pairs.size());
    }

    throw runtime_error("길이 함수는 문자열, 배열 또는 사전만 지원합니다.");
}

std::shared_ptr<Object> Print::function(std::vector<std::shared_ptr<Object>> parameters) {
    for (size_t i = 0; i < parameters.size(); i++) {
        if (auto str = dynamic_cast<String*>(parameters[i].get())) {
            cout << str->value;
        } else if (auto integer = dynamic_cast<Integer*>(parameters[i].get())) {
            cout << integer->value;
        } else if (auto float_obj = dynamic_cast<Float*>(parameters[i].get())) {
            cout << float_obj->ToString();
        } else if (auto boolean = dynamic_cast<Boolean*>(parameters[i].get())) {
            cout << (boolean->value ? "true" : "false");
        } else if (auto array = dynamic_cast<Array*>(parameters[i].get())) {
            cout << array->ToString();
        } else if (auto hashmap = dynamic_cast<HashMap*>(parameters[i].get())) {
            cout << hashmap->ToString();
        } else if (auto func = dynamic_cast<Function*>(parameters[i].get())) {
            cout << func->ToString();
        } else {
            throw runtime_error("출력 함수에서 지원하지 않는 타입입니다.");
        }

        if (i < parameters.size() - 1) {
            cout << " ";
        }
    }
    cout << endl;

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

std::shared_ptr<Object> Input::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() > 1) {
        throw runtime_error("입력 함수는 인자를 0개 또는 1개 받습니다.");
    }

    // 프롬프트 출력 (선택)
    if (parameters.size() == 1) {
        if (auto str = dynamic_cast<String*>(parameters[0].get())) {
            cout << str->value;
        }
    }

    string input;
    getline(cin, input);
    return make_shared<String>(input);
}

std::shared_ptr<Object> Keys::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("키목록 함수는 인자를 1개만 받습니다.");
    }

    auto* hashmap = dynamic_cast<HashMap*>(parameters[0].get());
    if (!hashmap) {
        throw runtime_error("키목록 함수의 인자는 사전이어야 합니다.");
    }

    auto array = make_shared<Array>();
    for (auto& [key, _] : hashmap->pairs) {
        array->elements.push_back(make_shared<String>(key));
    }
    return array;
}

std::shared_ptr<Object> Contains::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("포함 함수는 인자를 2개 받습니다.");
    }

    if (auto* hashmap = dynamic_cast<HashMap*>(parameters[0].get())) {
        auto* key = dynamic_cast<String*>(parameters[1].get());
        if (!key) throw runtime_error("사전의 키는 문자열이어야 합니다.");
        return make_shared<Boolean>(hashmap->pairs.contains(key->value));
    }
    if (auto* array = dynamic_cast<Array*>(parameters[0].get())) {
        for (auto& elem : array->elements) {
            if (elem->ToString() == parameters[1]->ToString() && elem->type == parameters[1]->type) {
                return make_shared<Boolean>(true);
            }
        }
        return make_shared<Boolean>(false);
    }
    if (auto* str = dynamic_cast<String*>(parameters[0].get())) {
        auto* substr = dynamic_cast<String*>(parameters[1].get());
        if (!substr) throw runtime_error("문자열 포함 검사는 문자열 인자가 필요합니다.");
        return make_shared<Boolean>(str->value.find(substr->value) != string::npos);
    }

    throw runtime_error("포함 함수는 사전, 배열 또는 문자열만 지원합니다.");
}

std::shared_ptr<Object> MapSet::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 3) {
        throw runtime_error("설정 함수는 인자를 3개 받습니다 (사전, 키, 값).");
    }

    auto* hashmap = dynamic_cast<HashMap*>(parameters[0].get());
    if (!hashmap) {
        throw runtime_error("설정 함수의 첫 번째 인자는 사전이어야 합니다.");
    }
    auto* key = dynamic_cast<String*>(parameters[1].get());
    if (!key) {
        throw runtime_error("사전의 키는 문자열이어야 합니다.");
    }
    hashmap->pairs[key->value] = parameters[2];
    return nullptr;
}

std::shared_ptr<Object> Remove::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("삭제 함수는 인자를 2개 받습니다.");
    }

    if (auto* hashmap = dynamic_cast<HashMap*>(parameters[0].get())) {
        auto* key = dynamic_cast<String*>(parameters[1].get());
        if (!key) throw runtime_error("사전의 키는 문자열이어야 합니다.");
        hashmap->pairs.erase(key->value);
        return nullptr;
    }
    if (auto* array = dynamic_cast<Array*>(parameters[0].get())) {
        auto* index = dynamic_cast<Integer*>(parameters[1].get());
        if (!index) throw runtime_error("배열 삭제의 두 번째 인자는 정수(인덱스)이어야 합니다.");
        if (index->value < 0 || index->value >= static_cast<long long>(array->elements.size())) {
            throw runtime_error("배열의 범위 밖 인덱스입니다.");
        }
        array->elements.erase(array->elements.begin() + index->value);
        return nullptr;
    }

    throw runtime_error("삭제 함수는 사전 또는 배열만 지원합니다.");
}

std::shared_ptr<Object> FileRead::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("파일읽기 함수는 인자를 1개 받습니다.");
    }

    auto* filename = dynamic_cast<String*>(parameters[0].get());
    if (!filename) {
        throw runtime_error("파일읽기 함수의 인자는 문자열(파일 경로)이어야 합니다.");
    }

    ifstream file(filename->value);
    if (!file.is_open()) {
        throw runtime_error("파일을 열 수 없습니다: " + filename->value);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return make_shared<String>(buffer.str());
}

std::shared_ptr<Object> FileWrite::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("파일쓰기 함수는 인자를 2개 받습니다 (파일 경로, 내용).");
    }

    auto* filename = dynamic_cast<String*>(parameters[0].get());
    if (!filename) {
        throw runtime_error("파일쓰기 함수의 첫 번째 인자는 문자열(파일 경로)이어야 합니다.");
    }
    auto* content = dynamic_cast<String*>(parameters[1].get());
    if (!content) {
        throw runtime_error("파일쓰기 함수의 두 번째 인자는 문자열(내용)이어야 합니다.");
    }

    ofstream file(filename->value);
    if (!file.is_open()) {
        throw runtime_error("파일을 열 수 없습니다: " + filename->value);
    }

    file << content->value;
    return nullptr;
}
