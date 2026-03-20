#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H

#include <string>
#include <vector>
#include <stdexcept>

namespace utf8 {

// UTF-8 문자열에서 코드포인트 개수를 반환
inline size_t codePointCount(const std::string& str) {
    size_t count = 0;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c < 0x80) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            i += 4;
        } else {
            i += 1; // 잘못된 바이트는 1바이트로 처리
        }
        count++;
    }
    return count;
}

// UTF-8 문자열에서 n번째 코드포인트를 문자열로 추출 (0-based)
inline std::string nthCodePoint(const std::string& str, size_t n) {
    size_t count = 0;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t charLen = 1;
        if (c < 0x80) {
            charLen = 1;
        } else if ((c & 0xE0) == 0xC0) {
            charLen = 2;
        } else if ((c & 0xF0) == 0xE0) {
            charLen = 3;
        } else if ((c & 0xF8) == 0xF0) {
            charLen = 4;
        }

        if (count == n) {
            return str.substr(i, charLen);
        }
        i += charLen;
        count++;
    }
    throw std::out_of_range("UTF-8 문자열의 범위 밖 인덱스입니다.");
}

// UTF-8 문자열을 코드포인트 단위로 분리하여 벡터로 반환
inline std::vector<std::string> toCodePoints(const std::string& str) {
    std::vector<std::string> result;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t charLen = 1;
        if (c < 0x80) {
            charLen = 1;
        } else if ((c & 0xE0) == 0xC0) {
            charLen = 2;
        } else if ((c & 0xF0) == 0xE0) {
            charLen = 3;
        } else if ((c & 0xF8) == 0xF0) {
            charLen = 4;
        }
        result.push_back(str.substr(i, charLen));
        i += charLen;
    }
    return result;
}

// UTF-8 코드포인트 단위로 문자열 뒤집기
inline std::string reverseCodePoints(const std::string& str) {
    auto cps = toCodePoints(str);
    std::string result;
    for (auto it = cps.rbegin(); it != cps.rend(); ++it) {
        result += *it;
    }
    return result;
}

} // namespace utf8

#endif // UTF8_UTILS_H
