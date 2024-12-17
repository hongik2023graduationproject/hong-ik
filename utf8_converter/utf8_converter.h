#ifndef UTF8_CONVERTER_H
#define UTF8_CONVERTER_H

#include <string>
#include <vector>

class Utf8Converter {
public:
    static std::vector<std::string> convert(const std::string &input);
private:
    static int getCharacterLength(const std::string &input, int position);
};


#endif //UTF8_CONVERTER_H
