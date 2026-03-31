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
        if (i + charLen > str.size()) break;
        i += charLen;
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

        if (i + charLen > str.size()) break;
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
        if (i + charLen > str.size()) break;
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

// UTF-8 코드포인트 단위로 부분 문자열 추출 (start~end, 0-based, end 미포함)
inline std::string substringCodePoints(const std::string& str, size_t start, size_t end) {
    auto cps = toCodePoints(str);
    if (start >= cps.size()) return "";
    if (end > cps.size()) end = cps.size();
    if (start >= end) return "";
    std::string result;
    for (size_t i = start; i < end; i++) {
        result += cps[i];
    }
    return result;
}

// UTF-8 문자열이 특정 접두사로 시작하는지 확인
inline bool startsWithStr(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}

// UTF-8 문자열이 특정 접미사로 끝나는지 확인
inline bool endsWithStr(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace utf8

#endif // UTF8_UTILS_H
