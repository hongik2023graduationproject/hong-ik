#include "../built_in.h"

#include "../../util/json_util.h"

#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

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

std::shared_ptr<Object> JsonParse::function(const std::vector<std::shared_ptr<Object>>& parameters) {
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

std::shared_ptr<Object> JsonSerialize::function(const std::vector<std::shared_ptr<Object>>& parameters) {
    if (parameters.size() != 1) {
        throw runtime_error("JSON_직렬화 함수는 인자를 1개만 받습니다.");
    }
    return make_shared<String>(serializeToJson(parameters[0]));
}
