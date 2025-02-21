#include "lexer.h"

using namespace std;

std::vector<Token *> Lexer::Tokenize(const std::vector<std::string> &characters) {
    this->characters = characters;
    current_read_position = 0;
    next_read_position = 1;
    line = 1;


    vector<Token *> tokens;
    // 현재 token type에 정의된 토큰 중 identifier, integer, float, string이 미구현
    // 추후에 hash 함수 이용한 switch문 같은 가독성, 효율 좋은 코드로 변경할 필요 있음
    while (current_read_position < characters.size()) {
        if (characters[current_read_position] == "+") {
            tokens.push_back(new Token{TokenType::PLUS, characters[current_read_position], line});
        } else if (characters[current_read_position] == "-") {
            tokens.push_back(new Token{TokenType::MINUS, characters[current_read_position], line});
        } else if (characters[current_read_position] == "*") {
            tokens.push_back(new Token{TokenType::ASTERISK, characters[current_read_position], line});
        } else if (characters[current_read_position] == "/") {
            tokens.push_back(new Token{TokenType::SLASH, characters[current_read_position], line});
        } else if (characters[current_read_position] == "=") {
            if (next_read_position < characters.size() && characters[next_read_position] == "=") {
                tokens.push_back(new Token{
                    TokenType::EQUAL, characters[current_read_position] + characters[next_read_position], line
                });
                current_read_position++;
                next_read_position++;
            } else {
                tokens.push_back(new Token{TokenType::ASSIGN, characters[current_read_position], line});
            }
        } else if (characters[current_read_position] == "!") {
            if (next_read_position < characters.size() && characters[next_read_position] == "=") {
                tokens.push_back(new Token{
                    TokenType::NOT_EQUAL, characters[current_read_position] + characters[next_read_position], line
                });
                current_read_position++;
                next_read_position++;
            } else {
                tokens.push_back(new Token{TokenType::BANG, characters[current_read_position], line});
            }
        } else if (characters[current_read_position] == "\n") {
            tokens.push_back(new Token{TokenType::NEW_LINE, characters[current_read_position], line});
        } else if (characters[current_read_position] == " ") {
            // space 토큰은 문법을 명확하게 작성하도록 강요하는 용도로 만들었다.
            // 하지만 리팩토링하는 지금 시점에서 다시 생각해보면 나중에 추가할 수 있는 부분이고
            // parser의 구조를 복잡하게 만드는 요소이므로 추후에 추가하는 것을 고려하기로 함
            // tokens.push_back(new Token{TokenType::SPACE, characters[current_read_position], line});
        } else if (characters[current_read_position] == "\t") {
            tokens.push_back(new Token{TokenType::TAB, characters[current_read_position], line});
        } else if (characters[current_read_position] == "(") {
            tokens.push_back(new Token{TokenType::LPAREN, characters[current_read_position], line});
        } else if (characters[current_read_position] == ")") {
            tokens.push_back(new Token{TokenType::RPAREN, characters[current_read_position], line});
        } else if (isNumber(characters[current_read_position])) {
            string integer_string = readInteger();
            tokens.push_back(new Token{TokenType::INTEGER, integer_string, line});
        } else if (isLetter(characters[current_read_position])) {
            string letter = readLetter();
            // TODO: 예약어 처리는 여기서 진행할 것
            tokens.push_back(new Token{TokenType::IDENTIFIER, letter, line});
        } else {
            tokens.push_back(new Token{TokenType::ILLEGAL, characters[current_read_position], line});
        }

        current_read_position++;
        next_read_position++;
    }

    // EOF는 표현할 수 있는 문자열이 없으므로 빈 문자열을 추가
    tokens.push_back(new Token(TokenType::END_OF_FILE, "", line));
    return tokens;
}


bool Lexer::isNumber(const std::string &s) {
    return ("0" <= s && s <= "9");
}

string Lexer::readInteger() {
    string integer_string = characters[current_read_position];

    while (next_read_position < characters.size() && isNumber(characters[next_read_position])) {
        current_read_position++;
        next_read_position++;
        integer_string += characters[current_read_position];
    }

    return integer_string;
}

bool Lexer::isLetter(const std::string &s) {
    return ("a" <= s && s <= "z" || "A" <= s && s <= "Z" || s == "_" || "가" <= s && s <= "힣");
}

std::string Lexer::readLetter() {
    string identifier = characters[current_read_position];

    while (next_read_position < characters.size() && isLetter(characters[next_read_position]) || isNumber(characters[next_read_position])) {
        current_read_position++;
        next_read_position++;
        identifier += characters[current_read_position];
    }
    return identifier;
}
