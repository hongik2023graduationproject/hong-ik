#include "../built_in.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

using namespace std;

// ===== 수학 내장함수 =====

std::shared_ptr<Object> Abs::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Sqrt::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Max::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Min::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Random::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Sin::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Cos::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Tan::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Log::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Ln::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Power::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Pi::function(const std::vector<std::shared_ptr<Object>>& parameters) {
    if (!parameters.empty()) {
        throw runtime_error("파이 함수는 인자를 받지 않습니다.");
    }
    return make_shared<Float>(3.14159265358979323846);
}

std::shared_ptr<Object> EulerE::function(const std::vector<std::shared_ptr<Object>>& parameters) {
    if (!parameters.empty()) {
        throw runtime_error("자연수e 함수는 인자를 받지 않습니다.");
    }
    return make_shared<Float>(2.71828182845904523536);
}

// ===== 반올림 계열 =====

std::shared_ptr<Object> Round::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Ceil::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> Floor::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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
