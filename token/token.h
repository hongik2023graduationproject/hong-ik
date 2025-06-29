#ifndef TOKEN_H
#define TOKEN_H

#include "token_type.h"
#include <string>

class Token {
public:
    TokenType type;
    std::string text;
    long long line;
};

std::string TokenTypeToString(TokenType type);

#endif // TOKEN_H
