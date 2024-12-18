#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>

#include "../token/token.h"

class Lexer {
public:
    std::vector<Token *> tokenize(const std::vector<std::string> &code);
};


#endif //LEXER_H
