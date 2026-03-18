#ifndef LEXER_H
#define LEXER_H

#include "../token/token.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

class Lexer {
public:
    Lexer();

    std::vector<std::shared_ptr<Token>> Tokenize(const std::vector<std::string>& characters);

private:
    bool at_line_start = true;
    int indent = 0;
    std::vector<std::string> characters;
    std::vector<std::shared_ptr<Token>> tokens;
    long long current_read_position = 0;
    long long next_read_position = 0;
    long long line = 1;
    long long line_start_pos = 0;  // 현재 줄 시작의 character 인덱스
    long long token_start = 0;    // 현재 토큰 시작 위치 (addToken에서 column 계산용)
    std::unordered_map<std::string, TokenType> keywords;
    std::unordered_map<std::string, TokenType> singleCharacterTokens;
    std::unordered_map<std::string, TokenType> multiCharacterTokens;

    void handleIdentifierAndKeywords(std::string& identifier);

    bool isNumber(const std::string& s);
    bool isLetter(const std::string& s);

    std::string readNumber(bool& isFloat);
    std::string readLetter();
    std::string readString();

    void skipLineComment();
    void skipBlockComment();

    void addToken(TokenType type);
    void addToken(TokenType type, std::string literal);
};


#endif // LEXER_H
