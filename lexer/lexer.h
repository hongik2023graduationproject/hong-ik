#ifndef LEXER_H
#define LEXER_H

#include <map>
#include <string>
#include <vector>
#include <map>

#include "../token/token.h"

class Lexer {
public:
    Lexer();

    std::vector<Token *> Tokenize(const std::vector<std::string> &characters);

private:
    std::vector<std::string> characters;
    long long current_read_position;
    long long next_read_position;
    long long line;


    bool isNumber(const std::string &s);
    bool isLetter(const std::string &s);

    std::string readInteger();
    std::string readLetter();

    std::map<std::string, TokenType> keywords;
};


#endif //LEXER_H
