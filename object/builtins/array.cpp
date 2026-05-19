#include "../built_in.h"

#include "../../util/utf8_utils.h"

#include <algorithm>
#include <stdexcept>

using namespace std;

// ===== 배열 내장함수 =====

std::shared_ptr<Object> Push::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("추가 함수는 인자를 2개 받습니다.");
    }
    if (auto array = dynamic_cast<Array*>(parameters[0].get())) {
        array->elements.push_back(parameters[1]);
        return make_shared<Null>();
    }

    throw runtime_error("추가 함수의 첫 번째 인자는 배열이어야 합니다.");
}

std::shared_ptr<Object> Sort::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("정렬 함수는 인자를 1개만 받습니다.");
    }
    auto* arr = dynamic_cast<Array*>(parameters[0].get());
    if (!arr) throw runtime_error("정렬 함수의 인자는 배열이어야 합니다.");

    auto result = make_shared<Array>();
    result->elements = arr->elements;

    std::sort(result->elements.begin(), result->elements.end(),
              [](const shared_ptr<Object>& a, const shared_ptr<Object>& b) {
                  // 정수 비교
                  auto* ai = dynamic_cast<Integer*>(a.get());
                  auto* bi = dynamic_cast<Integer*>(b.get());
                  if (ai && bi) return ai->value < bi->value;
                  // 실수 비교
                  auto* af = dynamic_cast<Float*>(a.get());
                  auto* bf = dynamic_cast<Float*>(b.get());
                  if (af && bf) return af->value < bf->value;
                  // 문자열 비교
                  return a->ToString() < b->ToString();
              });
    return result;
}

std::shared_ptr<Object> Reverse::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("뒤집기 함수는 인자를 1개만 받습니다.");
    }
    if (auto* arr = dynamic_cast<Array*>(parameters[0].get())) {
        auto result = make_shared<Array>();
        result->elements = arr->elements;
        std::reverse(result->elements.begin(), result->elements.end());
        return result;
    }
    if (auto* str = dynamic_cast<String*>(parameters[0].get())) {
        // UTF-8 코드포인트 단위로 뒤집기
        return make_shared<String>(utf8::reverseCodePoints(str->value));
    }
    throw runtime_error("뒤집기 함수는 배열 또는 문자열만 지원합니다.");
}

std::shared_ptr<Object> Find::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("찾기 함수는 인자를 2개 받습니다 (배열, 값).");
    }
    auto* arr = dynamic_cast<Array*>(parameters[0].get());
    if (!arr) throw runtime_error("찾기 함수의 첫 번째 인자는 배열이어야 합니다.");

    for (size_t i = 0; i < arr->elements.size(); i++) {
        if (arr->elements[i]->ToString() == parameters[1]->ToString()
            && arr->elements[i]->type == parameters[1]->type) {
            return make_shared<Integer>(static_cast<long long>(i));
        }
    }
    return make_shared<Integer>(-1);
}

std::shared_ptr<Object> Slice::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 3) {
        throw runtime_error("조각 함수는 인자를 3개 받습니다 (배열, 시작, 끝).");
    }
    auto* arr = dynamic_cast<Array*>(parameters[0].get());
    auto* start = dynamic_cast<Integer*>(parameters[1].get());
    auto* end = dynamic_cast<Integer*>(parameters[2].get());
    if (!arr || !start || !end) {
        throw runtime_error("조각 함수의 인자 형식이 잘못되었습니다 (배열, 정수, 정수).");
    }

    long long s = start->value;
    long long e = end->value;
    long long len = static_cast<long long>(arr->elements.size());
    if (s < 0) s = 0;
    if (e > len) e = len;
    if (s >= e) return make_shared<Array>();

    auto result = make_shared<Array>();
    for (long long i = s; i < e; i++) {
        result->elements.push_back(arr->elements[i]);
    }
    return result;
}
