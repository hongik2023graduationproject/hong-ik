#include "../built_in.h"

#include "../../util/utf8_utils.h"

#include <cctype>
#include <stdexcept>
#include <string>

using namespace std;

// ===== 문자열 내장함수 =====

std::shared_ptr<Object> Split::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("분리 함수는 인자를 2개 받습니다 (문자열, 구분자).");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    auto* delim = dynamic_cast<String*>(parameters[1].get());
    if (!str || !delim) {
        throw runtime_error("분리 함수의 인자는 문자열이어야 합니다.");
    }

    auto result = make_shared<Array>();
    string s = str->value;
    string d = delim->value;

    if (d.empty()) {
        // UTF-8 코드포인트 단위로 분리
        auto codePoints = utf8::toCodePoints(s);
        for (auto& cp : codePoints) {
            result->elements.push_back(make_shared<String>(cp));
        }
        return result;
    }

    size_t pos = 0;
    while ((pos = s.find(d)) != string::npos) {
        result->elements.push_back(make_shared<String>(s.substr(0, pos)));
        s.erase(0, pos + d.length());
    }
    result->elements.push_back(make_shared<String>(s));
    return result;
}

std::shared_ptr<Object> ToUpper::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("대문자 함수는 인자를 1개만 받습니다.");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    if (!str) throw runtime_error("대문자 함수의 인자는 문자열이어야 합니다.");
    string result = str->value;
    for (auto& c : result) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return make_shared<String>(result);
}

std::shared_ptr<Object> ToLower::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("소문자 함수는 인자를 1개만 받습니다.");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    if (!str) throw runtime_error("소문자 함수의 인자는 문자열이어야 합니다.");
    string result = str->value;
    for (auto& c : result) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return make_shared<String>(result);
}

std::shared_ptr<Object> Replace::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 3) {
        throw runtime_error("치환 함수는 인자를 3개 받습니다 (원본, 대상, 교체).");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    auto* target = dynamic_cast<String*>(parameters[1].get());
    auto* replacement = dynamic_cast<String*>(parameters[2].get());
    if (!str || !target || !replacement) {
        throw runtime_error("치환 함수의 인자는 모두 문자열이어야 합니다.");
    }
    string result = str->value;
    size_t pos = 0;
    while ((pos = result.find(target->value, pos)) != string::npos) {
        result.replace(pos, target->value.length(), replacement->value);
        pos += replacement->value.length();
    }
    return make_shared<String>(result);
}

std::shared_ptr<Object> Trim::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("자르기 함수는 인자를 1개만 받습니다.");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    if (!str) throw runtime_error("자르기 함수의 인자는 문자열이어야 합니다.");
    string result = str->value;
    size_t start = result.find_first_not_of(" \t\n\r");
    size_t end = result.find_last_not_of(" \t\n\r");
    if (start == string::npos) return make_shared<String>("");
    return make_shared<String>(result.substr(start, end - start + 1));
}

std::shared_ptr<Object> StartsWith::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("시작확인 함수는 인자를 2개 받습니다 (문자열, 접두사).");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    auto* prefix = dynamic_cast<String*>(parameters[1].get());
    if (!str || !prefix) {
        throw runtime_error("시작확인 함수의 인자는 모두 문자열이어야 합니다.");
    }
    return make_shared<Boolean>(utf8::startsWithStr(str->value, prefix->value));
}

std::shared_ptr<Object> EndsWith::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("끝확인 함수는 인자를 2개 받습니다 (문자열, 접미사).");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    auto* suffix = dynamic_cast<String*>(parameters[1].get());
    if (!str || !suffix) {
        throw runtime_error("끝확인 함수의 인자는 모두 문자열이어야 합니다.");
    }
    return make_shared<Boolean>(utf8::endsWithStr(str->value, suffix->value));
}

std::shared_ptr<Object> Repeat::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("반복 함수는 인자를 2개 받습니다 (문자열, 횟수).");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    auto* count = dynamic_cast<Integer*>(parameters[1].get());
    if (!str) {
        throw runtime_error("반복 함수의 첫 번째 인자는 문자열이어야 합니다.");
    }
    if (!count) {
        throw runtime_error("반복 함수의 두 번째 인자는 정수이어야 합니다.");
    }
    if (count->value < 0) {
        throw runtime_error("반복 횟수는 0 이상이어야 합니다.");
    }
    if (count->value > 100000) {
        throw runtime_error("반복 횟수는 100000 이하여야 합니다.");
    }
    string result;
    for (long long i = 0; i < count->value; i++) {
        result += str->value;
    }
    return make_shared<String>(result);
}

std::shared_ptr<Object> Pad::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 3) {
        throw runtime_error("채우기 함수는 인자를 3개 받습니다 (문자열, 길이, 채울문자).");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    auto* length = dynamic_cast<Integer*>(parameters[1].get());
    auto* padChar = dynamic_cast<String*>(parameters[2].get());
    if (!str) {
        throw runtime_error("채우기 함수의 첫 번째 인자는 문자열이어야 합니다.");
    }
    if (!length) {
        throw runtime_error("채우기 함수의 두 번째 인자는 정수이어야 합니다.");
    }
    if (!padChar) {
        throw runtime_error("채우기 함수의 세 번째 인자는 문자열이어야 합니다.");
    }
    if (utf8::codePointCount(padChar->value) != 1) {
        throw runtime_error("채우기 함수의 세 번째 인자는 한 글자여야 합니다.");
    }

    size_t currentLen = utf8::codePointCount(str->value);
    if (static_cast<long long>(currentLen) >= length->value) {
        return make_shared<String>(str->value);
    }
    string result = str->value;
    for (long long i = currentLen; i < length->value; i++) {
        result += padChar->value;
    }
    return make_shared<String>(result);
}

std::shared_ptr<Object> Substring::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 3) {
        throw runtime_error("부분문자 함수는 인자를 3개 받습니다 (문자열, 시작, 끝).");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    auto* start = dynamic_cast<Integer*>(parameters[1].get());
    auto* end = dynamic_cast<Integer*>(parameters[2].get());
    if (!str) {
        throw runtime_error("부분문자 함수의 첫 번째 인자는 문자열이어야 합니다.");
    }
    if (!start || !end) {
        throw runtime_error("부분문자 함수의 두 번째, 세 번째 인자는 정수이어야 합니다.");
    }
    long long s = start->value;
    long long e = end->value;
    if (s < 0) s = 0;
    if (e < 0) e = 0;
    return make_shared<String>(utf8::substringCodePoints(str->value,
        static_cast<size_t>(s), static_cast<size_t>(e)));
}
