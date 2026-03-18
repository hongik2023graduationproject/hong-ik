#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <iomanip>
#include <sstream>
#include <string>

namespace json_util {

// JSON 문자열 이스케이프
inline std::string escape(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 32) {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                    << static_cast<int>(static_cast<unsigned char>(c));
                result += oss.str();
            } else {
                result += c;
            }
        }
    }
    return result;
}

} // namespace json_util

#endif // JSON_UTIL_H
