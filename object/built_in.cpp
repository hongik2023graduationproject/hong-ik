#include "built_in.h"

#include "../util/utf8_utils.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace std;


// UTF-8 코드포인트 기반 문자열 길이 반환
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

std::shared_ptr<Object> Print::function(std::vector<std::shared_ptr<Object>> parameters) {
    string output;
    for (size_t i = 0; i < parameters.size(); i++) {
        if (auto str = dynamic_cast<String*>(parameters[i].get())) {
            output += str->value;
        } else if (auto integer = dynamic_cast<Integer*>(parameters[i].get())) {
            output += to_string(integer->value);
        } else if (auto float_obj = dynamic_cast<Float*>(parameters[i].get())) {
            output += float_obj->ToString();
        } else if (auto boolean = dynamic_cast<Boolean*>(parameters[i].get())) {
            output += (boolean->value ? "true" : "false");
        } else if (auto array = dynamic_cast<Array*>(parameters[i].get())) {
            output += array->ToString();
        } else if (auto hashmap = dynamic_cast<HashMap*>(parameters[i].get())) {
            output += hashmap->ToString();
        } else if (auto func = dynamic_cast<Function*>(parameters[i].get())) {
            output += func->ToString();
        } else if (dynamic_cast<Null*>(parameters[i].get())) {
            output += "없음";
        } else if (auto tuple = dynamic_cast<Tuple*>(parameters[i].get())) {
            output += tuple->ToString();
        } else if (auto inst = dynamic_cast<Instance*>(parameters[i].get())) {
            output += inst->ToString();
        } else {
            throw runtime_error("출력 함수에서 지원하지 않는 타입입니다.");
        }

        if (i < parameters.size() - 1) {
            output += " ";
        }
    }
    output += "\n";

    // IOContext가 있으면 사용, 없으면 기본 cout 사용
    if (ioCtx && ioCtx->print) {
        ioCtx->print(output);
    } else {
        cout << output;
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

std::shared_ptr<Object> Input::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() > 1) {
        throw runtime_error("입력 함수는 인자를 0개 또는 1개 받습니다.");
    }

    string prompt;
    if (parameters.size() == 1) {
        if (auto str = dynamic_cast<String*>(parameters[0].get())) {
            prompt = str->value;
        }
    }

    // IOContext가 있으면 사용, 없으면 기본 getline 사용
    string input;
    if (ioCtx && ioCtx->input) {
        input = ioCtx->input(prompt);
    } else {
        if (!prompt.empty()) {
            cout << prompt;
        }
        getline(cin, input);
    }
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

    // IOContext가 있으면 사용, 없으면 기본 파일 I/O 사용
    if (ioCtx && ioCtx->fileRead) {
        string content = ioCtx->fileRead(filename->value);
        return make_shared<String>(content);
    }

    // 기본 파일 I/O
    // 경로 탐색 방지 (완전한 샌드박스는 아님)
    if (filename->value.find("..") != string::npos) {
        throw runtime_error("파일 경로에 '..'을 사용할 수 없습니다.");
    }
    if (!filename->value.empty() && (filename->value[0] == '/' || filename->value[0] == '\\')) {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
    }
    if (filename->value.size() >= 2 && filename->value[1] == ':') {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
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

    // IOContext가 있으면 사용, 없으면 기본 파일 I/O 사용
    if (ioCtx && ioCtx->fileWrite) {
        ioCtx->fileWrite(filename->value, content->value);
        return nullptr;
    }

    // 기본 파일 I/O
    // 경로 탐색 방지 (완전한 샌드박스는 아님)
    if (filename->value.find("..") != string::npos) {
        throw runtime_error("파일 경로에 '..'을 사용할 수 없습니다.");
    }
    if (!filename->value.empty() && (filename->value[0] == '/' || filename->value[0] == '\\')) {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
    }
    if (filename->value.size() >= 2 && filename->value[1] == ':') {
        throw runtime_error("절대 경로는 사용할 수 없습니다.");
    }

    ofstream file(filename->value);
    if (!file.is_open()) {
        throw runtime_error("파일을 열 수 없습니다: " + filename->value);
    }

    file << content->value;
    return nullptr;
}

// ===== 수학 내장함수 =====

std::shared_ptr<Object> Abs::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("절대값 함수는 인자를 1개만 받습니다.");
    }
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        return make_shared<Integer>(std::abs(i->value));
    }
    if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        return make_shared<Float>(std::abs(f->value));
    }
    throw runtime_error("절대값 함수는 정수 또는 실수만 지원합니다.");
}

std::shared_ptr<Object> Sqrt::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("제곱근 함수는 인자를 1개만 받습니다.");
    }
    double val;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        val = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        val = f->value;
    } else {
        throw runtime_error("제곱근 함수는 정수 또는 실수만 지원합니다.");
    }
    if (val < 0) {
        throw runtime_error("음수의 제곱근은 구할 수 없습니다.");
    }
    return make_shared<Float>(std::sqrt(val));
}

std::shared_ptr<Object> Max::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("최대 함수는 인자를 2개 받습니다.");
    }
    if (auto* a = dynamic_cast<Integer*>(parameters[0].get())) {
        if (auto* b = dynamic_cast<Integer*>(parameters[1].get())) {
            return make_shared<Integer>(std::max(a->value, b->value));
        }
    }
    // 실수로 변환하여 비교
    double va, vb;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) va = static_cast<double>(i->value);
    else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) va = f->value;
    else throw runtime_error("최대 함수는 숫자만 지원합니다.");
    if (auto* i = dynamic_cast<Integer*>(parameters[1].get())) vb = static_cast<double>(i->value);
    else if (auto* f = dynamic_cast<Float*>(parameters[1].get())) vb = f->value;
    else throw runtime_error("최대 함수는 숫자만 지원합니다.");
    return make_shared<Float>(std::max(va, vb));
}

std::shared_ptr<Object> Min::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("최소 함수는 인자를 2개 받습니다.");
    }
    if (auto* a = dynamic_cast<Integer*>(parameters[0].get())) {
        if (auto* b = dynamic_cast<Integer*>(parameters[1].get())) {
            return make_shared<Integer>(std::min(a->value, b->value));
        }
    }
    double va, vb;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) va = static_cast<double>(i->value);
    else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) va = f->value;
    else throw runtime_error("최소 함수는 숫자만 지원합니다.");
    if (auto* i = dynamic_cast<Integer*>(parameters[1].get())) vb = static_cast<double>(i->value);
    else if (auto* f = dynamic_cast<Float*>(parameters[1].get())) vb = f->value;
    else throw runtime_error("최소 함수는 숫자만 지원합니다.");
    return make_shared<Float>(std::min(va, vb));
}

std::shared_ptr<Object> Random::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("난수 함수는 인자를 2개 받습니다 (최소, 최대).");
    }
    auto* minVal = dynamic_cast<Integer*>(parameters[0].get());
    auto* maxVal = dynamic_cast<Integer*>(parameters[1].get());
    if (!minVal || !maxVal) {
        throw runtime_error("난수 함수의 인자는 정수이어야 합니다.");
    }
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<long long> dist(minVal->value, maxVal->value);
    return make_shared<Integer>(dist(gen));
}

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

// ===== 배열 내장함수 =====

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
