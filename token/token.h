#ifndef TOKEN_H
#define TOKEN_H

#include "token_type.h"
#include <string>

class Token {
public:
    TokenType type;
    std::string text;
    long long line;


    // 연산자 오버로딩
    bool operator==(const Token& other) const {
        return type == other.type && text == other.text && line == other.line;
    }
    bool operator!=(const Token& other) const {
        return !(*this == other);
    }
};

std::string TokenTypeToString(TokenType type);

#endif // TOKEN_H
