#include "built_in.h"

#include "../util/json_util.h"
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
    if (minVal->value > maxVal->value) {
        throw runtime_error("난수: 최소값이 최대값보다 클 수 없습니다.");
    }
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<long long> dist(minVal->value, maxVal->value);
    return make_shared<Integer>(dist(gen));
}

// ===== 삼각함수 =====

std::shared_ptr<Object> Sin::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("사인 함수는 인자를 1개만 받습니다.");
    }
    double val;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        val = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        val = f->value;
    } else {
        throw runtime_error("사인 함수는 정수 또는 실수만 지원합니다.");
    }
    return make_shared<Float>(std::sin(val));
}

std::shared_ptr<Object> Cos::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("코사인 함수는 인자를 1개만 받습니다.");
    }
    double val;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        val = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        val = f->value;
    } else {
        throw runtime_error("코사인 함수는 정수 또는 실수만 지원합니다.");
    }
    return make_shared<Float>(std::cos(val));
}

std::shared_ptr<Object> Tan::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("탄젠트 함수는 인자를 1개만 받습니다.");
    }
    double val;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        val = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        val = f->value;
    } else {
        throw runtime_error("탄젠트 함수는 정수 또는 실수만 지원합니다.");
    }
    return make_shared<Float>(std::tan(val));
}

// ===== 로그/지수 =====

std::shared_ptr<Object> Log::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("로그 함수는 인자를 1개만 받습니다.");
    }
    double val;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        val = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        val = f->value;
    } else {
        throw runtime_error("로그 함수는 정수 또는 실수만 지원합니다.");
    }
    if (val <= 0) {
        throw runtime_error("로그 함수의 인자는 양수여야 합니다.");
    }
    return make_shared<Float>(std::log10(val));
}

std::shared_ptr<Object> Ln::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("자연로그 함수는 인자를 1개만 받습니다.");
    }
    double val;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        val = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        val = f->value;
    } else {
        throw runtime_error("자연로그 함수는 정수 또는 실수만 지원합니다.");
    }
    if (val <= 0) {
        throw runtime_error("자연로그 함수의 인자는 양수여야 합니다.");
    }
    return make_shared<Float>(std::log(val));
}

std::shared_ptr<Object> Power::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 2) {
        throw runtime_error("거듭제곱 함수는 인자를 2개 받습니다 (밑, 지수).");
    }
    double base, exp;
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        base = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        base = f->value;
    } else {
        throw runtime_error("거듭제곱 함수는 숫자만 지원합니다.");
    }
    if (auto* i = dynamic_cast<Integer*>(parameters[1].get())) {
        exp = static_cast<double>(i->value);
    } else if (auto* f = dynamic_cast<Float*>(parameters[1].get())) {
        exp = f->value;
    } else {
        throw runtime_error("거듭제곱 함수는 숫자만 지원합니다.");
    }
    return make_shared<Float>(std::pow(base, exp));
}

// ===== 상수 =====

std::shared_ptr<Object> Pi::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (!parameters.empty()) {
        throw runtime_error("파이 함수는 인자를 받지 않습니다.");
    }
    return make_shared<Float>(3.14159265358979323846);
}

std::shared_ptr<Object> EulerE::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (!parameters.empty()) {
        throw runtime_error("자연수e 함수는 인자를 받지 않습니다.");
    }
    return make_shared<Float>(2.71828182845904523536);
}

// ===== 반올림 계열 =====

std::shared_ptr<Object> Round::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("반올림 함수는 인자를 1개만 받습니다.");
    }
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        return parameters[0];
    }
    if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        return make_shared<Integer>(static_cast<long long>(std::round(f->value)));
    }
    throw runtime_error("반올림 함수는 정수 또는 실수만 지원합니다.");
}

std::shared_ptr<Object> Ceil::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("올림 함수는 인자를 1개만 받습니다.");
    }
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        return parameters[0];
    }
    if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        return make_shared<Integer>(static_cast<long long>(std::ceil(f->value)));
    }
    throw runtime_error("올림 함수는 정수 또는 실수만 지원합니다.");
}

std::shared_ptr<Object> Floor::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("내림 함수는 인자를 1개만 받습니다.");
    }
    if (auto* i = dynamic_cast<Integer*>(parameters[0].get())) {
        return parameters[0];
    }
    if (auto* f = dynamic_cast<Float*>(parameters[0].get())) {
        return make_shared<Integer>(static_cast<long long>(std::floor(f->value)));
    }
    throw runtime_error("내림 함수는 정수 또는 실수만 지원합니다.");
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

// ===== JSON 내장함수 =====

// JSON 파서: 재귀 하강 파서
namespace {

class JsonParser {
public:
    explicit JsonParser(const string& input) : src(input), pos(0) {}

    shared_ptr<Object> parse() {
        skipWhitespace();
        auto result = parseValue();
        skipWhitespace();
        if (pos != src.size()) {
            throw runtime_error("JSON 파싱 오류: 예상치 못한 문자가 남아있습니다.");
        }
        return result;
    }

private:
    const string& src;
    size_t pos;

    char peek() const {
        if (pos >= src.size()) throw runtime_error("JSON 파싱 오류: 예상치 못한 입력 끝.");
        return src[pos];
    }

    char advance() {
        if (pos >= src.size()) throw runtime_error("JSON 파싱 오류: 예상치 못한 입력 끝.");
        return src[pos++];
    }

    void expect(char c) {
        skipWhitespace();
        if (advance() != c) {
            throw runtime_error(string("JSON 파싱 오류: '") + c + "' 문자가 필요합니다.");
        }
    }

    void skipWhitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) {
            pos++;
        }
    }

    shared_ptr<Object> parseValue() {
        skipWhitespace();
        if (pos >= src.size()) throw runtime_error("JSON 파싱 오류: 예상치 못한 입력 끝.");

        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBoolean();
        if (c == 'n') return parseNull();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();

        throw runtime_error(string("JSON 파싱 오류: 예상치 못한 문자 '") + c + "'.");
    }

    shared_ptr<Object> parseString() {
        expect('"');
        string result;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\') {
                pos++;
                if (pos >= src.size()) throw runtime_error("JSON 파싱 오류: 문자열이 완료되지 않았습니다.");
                switch (src[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    if (pos + 4 >= src.size()) throw runtime_error("JSON 파싱 오류: 유니코드 이스케이프가 불완전합니다.");
                    string hex = src.substr(pos + 1, 4);
                    unsigned long cp = stoul(hex, nullptr, 16);
                    pos += 4;
                    // UTF-16 surrogate pair 처리
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos + 1 < src.size() && src[pos + 1] == '\\' && pos + 2 < src.size() && src[pos + 2] == 'u') {
                            if (pos + 6 >= src.size()) throw runtime_error("JSON 파싱 오류: 유니코드 surrogate pair가 불완전합니다.");
                            string hex2 = src.substr(pos + 3, 4);
                            unsigned long low = stoul(hex2, nullptr, 16);
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                pos += 6;
                            } else {
                                throw runtime_error("JSON 파싱 오류: 잘못된 surrogate pair입니다.");
                            }
                        } else {
                            throw runtime_error("JSON 파싱 오류: high surrogate 뒤에 low surrogate가 필요합니다.");
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        throw runtime_error("JSON 파싱 오류: 예상치 못한 low surrogate입니다.");
                    }
                    // UTF-8 인코딩
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (cp >> 18));
                        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default:
                    throw runtime_error(string("JSON 파싱 오류: 알 수 없는 이스케이프 시퀀스 '\\") + src[pos] + "'.");
                }
                pos++;
            } else {
                result += src[pos++];
            }
        }
        if (pos >= src.size()) throw runtime_error("JSON 파싱 오류: 문자열이 완료되지 않았습니다.");
        pos++; // closing "
        return make_shared<String>(result);
    }

    shared_ptr<Object> parseNumber() {
        size_t start = pos;
        if (src[pos] == '-') pos++;
        while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') pos++;
        bool isFloat = false;
        if (pos < src.size() && src[pos] == '.') {
            isFloat = true;
            pos++;
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') pos++;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            isFloat = true;
            pos++;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) pos++;
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') pos++;
        }
        string numStr = src.substr(start, pos - start);
        if (isFloat) {
            return make_shared<Float>(stod(numStr));
        }
        return make_shared<Integer>(stoll(numStr));
    }

    shared_ptr<Object> parseObject() {
        expect('{');
        auto obj = make_shared<HashMap>();
        skipWhitespace();
        if (pos < src.size() && peek() == '}') {
            pos++;
            return obj;
        }
        while (true) {
            skipWhitespace();
            if (peek() != '"') throw runtime_error("JSON 파싱 오류: 사전의 키는 문자열이어야 합니다.");
            auto keyObj = parseString();
            string key = dynamic_cast<String*>(keyObj.get())->value;
            expect(':');
            auto value = parseValue();
            obj->pairs[key] = value;
            skipWhitespace();
            if (peek() == ',') {
                pos++;
            } else {
                break;
            }
        }
        expect('}');
        return obj;
    }

    shared_ptr<Object> parseArray() {
        expect('[');
        auto arr = make_shared<Array>();
        skipWhitespace();
        if (pos < src.size() && peek() == ']') {
            pos++;
            return arr;
        }
        while (true) {
            arr->elements.push_back(parseValue());
            skipWhitespace();
            if (peek() == ',') {
                pos++;
            } else {
                break;
            }
        }
        expect(']');
        return arr;
    }

    shared_ptr<Object> parseBoolean() {
        if (src.compare(pos, 4, "true") == 0) {
            pos += 4;
            return make_shared<Boolean>(true);
        }
        if (src.compare(pos, 5, "false") == 0) {
            pos += 5;
            return make_shared<Boolean>(false);
        }
        throw runtime_error("JSON 파싱 오류: 잘못된 값입니다.");
    }

    shared_ptr<Object> parseNull() {
        if (src.compare(pos, 4, "null") == 0) {
            pos += 4;
            return make_shared<Null>();
        }
        throw runtime_error("JSON 파싱 오류: 잘못된 값입니다.");
    }
};

// hong-ik 객체를 JSON 문자열로 직렬화
string serializeToJson(const shared_ptr<Object>& obj) {
    if (!obj || dynamic_cast<Null*>(obj.get())) {
        return "null";
    }
    if (auto* str = dynamic_cast<String*>(obj.get())) {
        return "\"" + json_util::escape(str->value) + "\"";
    }
    if (auto* i = dynamic_cast<Integer*>(obj.get())) {
        return to_string(i->value);
    }
    if (auto* f = dynamic_cast<Float*>(obj.get())) {
        ostringstream oss;
        oss << f->value;
        return oss.str();
    }
    if (auto* b = dynamic_cast<Boolean*>(obj.get())) {
        return b->value ? "true" : "false";
    }
    if (auto* arr = dynamic_cast<Array*>(obj.get())) {
        string result = "[";
        for (size_t i = 0; i < arr->elements.size(); i++) {
            if (i > 0) result += ",";
            result += serializeToJson(arr->elements[i]);
        }
        result += "]";
        return result;
    }
    if (auto* map = dynamic_cast<HashMap*>(obj.get())) {
        string result = "{";
        size_t i = 0;
        for (auto& [key, value] : map->pairs) {
            if (i > 0) result += ",";
            result += "\"" + json_util::escape(key) + "\":" + serializeToJson(value);
            i++;
        }
        result += "}";
        return result;
    }
    throw runtime_error("JSON_직렬화 오류: 지원하지 않는 타입입니다.");
}

} // anonymous namespace

std::shared_ptr<Object> JsonParse::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("JSON_파싱 함수는 인자를 1개만 받습니다.");
    }
    auto* str = dynamic_cast<String*>(parameters[0].get());
    if (!str) {
        throw runtime_error("JSON_파싱 함수의 인자는 문자열이어야 합니다.");
    }
    JsonParser parser(str->value);
    return parser.parse();
}

std::shared_ptr<Object> JsonSerialize::function(std::vector<std::shared_ptr<Object>> parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("JSON_직렬화 함수는 인자를 1개만 받습니다.");
    }
    return make_shared<String>(serializeToJson(parameters[0]));
}
