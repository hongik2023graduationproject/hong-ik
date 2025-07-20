#ifndef LEXER_H
#define LEXER_H

#include "../token/token.h"
#include <unordered_map>
#include <string>
#include <vector>

class Lexer {
public:
    Lexer();

    std::vector<Token*> Tokenize(const std::vector<std::string>& characters);

private:
    bool at_line_start;
    int indent = 0;
    std::vector<std::string> characters;
    std::vector<Token*> tokens;
    long long current_read_position;
    long long next_read_position;
    long long line;
    std::unordered_map<std::string, TokenType> keywords;
    std::unordered_map<std::string, TokenType> singleCharacterTokens;
    std::unordered_map<std::string, TokenType> multiCharacterTokens;

    void handleIdentifierAndKeywords(std::string& identifier);

    bool isNumber(const std::string& s);
    bool isLetter(const std::string& s);

    std::string readInteger();
    std::string readLetter();
    std::string readString();

    void addToken(TokenType type);
    void addToken(TokenType type, std::string literal);


};


#endif // LEXER_H
