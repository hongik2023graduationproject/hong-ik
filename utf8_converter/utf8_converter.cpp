#include "utf8_converter.h"

using namespace std;

std::vector<std::string> Utf8Converter::convert(const std::string &input) {
    vector<string> characters;

    for (int position = 0, length; position < input.length(); position += length) {
        length = getCharacterLength(input, position);
        characters.push_back(input.substr(position, length));
    }

    return characters;
}

int Utf8Converter::getCharacterLength(const std::string &input, const int position) {
    if (position < 0 || position >= input.size()) {
        return 0; // 유효하지 않은 위치
    }

    if (const auto firstByte = static_cast<unsigned char>(input[position]); firstByte < 0x80) {
        return 1; // 1바이트 문자
    } else if ((firstByte & 0xE0) == 0xC0) {
        return 2; // 2바이트 문자
    } else if ((firstByte & 0xF0) == 0xE0) {
        return 3; // 3바이트 문자
    } else if ((firstByte & 0xF8) == 0xF0) {
        return 4; // 4바이트 문자
    }

    return 0; // 잘못된 UTF-8 인코딩
}
