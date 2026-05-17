#include "../built_in.h"

#include "../../util/utf8_utils.h"

#include <stdexcept>

using namespace std;

// ===== 컬렉션 내장함수 (문자열/배열/사전 공통) =====

std::shared_ptr<Object> Length::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("길이 함수는 인자를 1개만 받습니다.");
    }

    if (auto str = dynamic_cast<String*>(parameters[0].get())) {
        return make_shared<Integer>(utf8::codePointCount(str->value));
    }
    if (auto array = dynamic_cast<Array*>(parameters[0].get())) {
        return make_shared<Integer>(array->elements.size());
    }
    if (auto hashmap = dynamic_cast<HashMap*>(parameters[0].get())) {
        return make_shared<Integer>(hashmap->pairs.size());
    }

    throw runtime_error("길이 함수는 문자열, 배열 또는 사전만 지원합니다.");
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
