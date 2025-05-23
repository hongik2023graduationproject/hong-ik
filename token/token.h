#ifndef TOKEN_H
#define TOKEN_H

#include <string>

#include "token_type.h"

class Token {
public:
    TokenType type;
    std::string text;
    long long line;
};

std::string TokenTypeToString(TokenType type);

#endif //TOKEN_H
