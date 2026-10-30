#ifndef SLICE_UTILS_H
#define SLICE_UTILS_H

#include "../object/object.h"

namespace slice_utils {

    // 슬라이스 [시작:끝]의 인덱스를 길이 len 기준으로 정규화한다 (평가기·VM 공용).
    // 음수는 끝에서부터 세고, 시작은 0 이상·끝은 len 이하로 자른다. 생략된 쪽(nullptr)은 0 / len.
    // 결과가 start >= end이면 빈 슬라이스, 아니면 0 <= start < end <= len이 보장된다.
    // 인덱스가 정수가 아니면 한국어 오류 메시지를, 정상이면 nullptr를 돌려준다 — 예외·줄 번호는 호출부가 정한다.
    inline const char* normalizeSliceBounds(
        const Object* startObj, const Object* endObj, long long len, long long& start, long long& end) {
        start = 0;
        end   = len;
        if (startObj) {
            auto* startInt = dynamic_cast<const Integer*>(startObj);
            if (!startInt) {
                return "슬라이스 시작 인덱스는 정수여야 합니다.";
            }
            start = startInt->value;
            if (start < 0) {
                start = len + start;
            }
            if (start < 0) {
                start = 0;
            }
        }
        if (endObj) {
            auto* endInt = dynamic_cast<const Integer*>(endObj);
            if (!endInt) {
                return "슬라이스 끝 인덱스는 정수여야 합니다.";
            }
            end = endInt->value;
            if (end < 0) {
                end = len + end;
            }
            if (end > len) {
                end = len;
            }
        }
        return nullptr;
    }

} // namespace slice_utils

#endif // SLICE_UTILS_H
