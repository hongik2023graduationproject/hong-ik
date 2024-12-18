#include "lexer.h"

using namespace std;

std::vector<Token *> Lexer::tokenize(const std::vector<std::string> &codes) {
    vector<Token *> tokens;

    long long line = 1;

    // 현재 token type에 정의된 토큰 중 identifier, integer, float, string이 미구현
    // 추후에 hash 함수 이용한 switch문 같은 가독성, 효율 좋은 코드로 변경할 필요 있음
    for (int i = 0; i < codes.size(); ++i) {
        if (codes[i] == "+") {
            tokens.push_back(new Token{TokenType::PLUS, codes[i], line});
        } else if (codes[i] == "-") {
            tokens.push_back(new Token{TokenType::MINUS, codes[i], line});
        } else if (codes[i] == "*") {
            tokens.push_back(new Token{TokenType::ASTERISK, codes[i], line});
        } else if (codes[i] == "/") {
            tokens.push_back(new Token{TokenType::SLASH, codes[i], line});
        } else if (codes[i] == "=") {
            if (codes[i + 1] == "=") {
                tokens.push_back(new Token{TokenType::EQUAL, codes[i] + codes[i + 1], line});
                i++;
            } else {
                tokens.push_back(new Token{TokenType::ASSIGN, codes[i], line});
            }
        } else if (codes[i] == "!") {
            if (codes[i + 1] == "=") {
                tokens.push_back(new Token{TokenType::NOT_EQUAL, codes[i] + codes[i + 1], line});
                i++;
            } else {
                tokens.push_back(new Token{TokenType::BANG, codes[i], line});
            }
        } else if (codes[i] == "\n") {
            tokens.push_back(new Token{TokenType::NEW_LINE, codes[i], line});
        } else if (codes[i] == " ") {
            tokens.push_back(new Token{TokenType::SPACE, codes[i], line});
        } else if (codes[i] == "\t") {
            tokens.push_back(new Token{TokenType::TAB, codes[i], line});
        } else {
            tokens.push_back(new Token{TokenType::ILLEGAL, codes[i], line});
        }
    }

    // EOF는 표현할 수 있는 문자열이 없으므로 빈 문자열을 추가
    tokens.push_back(new Token(TokenType::END_OF_FILE, "", line));
    return tokens;
}
