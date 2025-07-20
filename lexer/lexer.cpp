#include "lexer.h"

#include "../exception/exception.h"
#include <stdexcept>
#include <unordered_map>

using namespace std;

Lexer::Lexer() {
    keywords = {
        {"정수", TokenType::정수},
        {"실수", TokenType::실수},
        {"문자", TokenType::문자},
        {"리턴", TokenType::리턴},
        {"만약", TokenType::만약},
        {"라면", TokenType::라면},
        {"아니면", TokenType::아니면},
        {"함수", TokenType::함수},
        {"true", TokenType::TRUE},
        {"false", TokenType::FALSE},
    };
    singleCharacterTokens = {
        {"+", TokenType::PLUS},
        {"*", TokenType::ASTERISK},
        {"/", TokenType::SLASH},
        {"(", TokenType::LPAREN},
        {")", TokenType::RPAREN},
        {"{", TokenType::LBRACE},
        {"}", TokenType::RBRACE},
        {"[", TokenType::LBRACKET},
        {"]", TokenType::RBRACKET},
        {":", TokenType::COLON},
        {";", TokenType::SEMICOLON},
        {",", TokenType::COMMA},
        {"-", TokenType::MINUS},
        {"=", TokenType::ASSIGN},
        {"!", TokenType::BANG},
        {"<", TokenType::LESS_THAN},
        {">", TokenType::GREATER_THAN},
        {"&", TokenType::BITWISE_AND},
        {"|", TokenType::BITWISE_OR},
    };
    multiCharacterTokens = {
        {"->", TokenType::RIGHT_ARROW},
        {"==", TokenType::EQUAL},
        {"!=", TokenType::NOT_EQUAL},
        {"&&", TokenType::LOGICAL_AND},
        {"||", TokenType::LOGICAL_OR},
        {"<=", TokenType::LESS_EQUAL},
        {">=", TokenType::GREATER_EQUAL},
    };
}


std::vector<Token*> Lexer::Tokenize(const std::vector<std::string>& characters) {
    this->characters      = characters;
    current_read_position = 0;
    next_read_position    = 1;
    at_line_start         = true;
    line                  = 1;
    tokens.clear();

    while (current_read_position < characters.size()) {
        string current_character = characters[current_read_position];
        string next_character    = (next_read_position < characters.size()) ? characters[next_read_position] : "";

        // 공백 무시 (토큰 생성 안함)
        if (at_line_start) {
            int space_counter = 0;
            while (characters[current_read_position] == " " || characters[current_read_position] == "\t") {
                if (characters[current_read_position] == " ") {
                    space_counter++;
                }

                if (characters[current_read_position] == "\t") {
                    space_counter += 4;
                }

                current_read_position++;
                next_read_position++;
            }

            const int current_indent = space_counter / 4;
            if (indent < current_indent) {
                for (int i = indent; i < current_indent; i++) {
                    addToken(TokenType::START_BLOCK, "");
                }
            } else if (indent > current_indent) {
                for (int i = indent; i > current_indent; i--) {
                    addToken(TokenType::END_BLOCK, "");
                }
            }
            indent = current_indent;
            at_line_start = false;
            continue;
        }

        if (current_character == " ") {
            current_read_position++;
            next_read_position++;
            continue;
        }

        if (current_character == "\n") {
            addToken(TokenType::NEW_LINE, "\n");
            line++;
            current_read_position++;
            next_read_position++;
            at_line_start = true;
            continue;
        }

        // 2글자 연산자 처리
        if (auto iterator = multiCharacterTokens.find(current_character + next_character);
            iterator != multiCharacterTokens.end()) {
            addToken(iterator->second, current_character + next_character);
            current_read_position += 2;
            next_read_position += 2;
            continue;
        }
        // handleMultiCharacterToken(current_character, next_character)) {
        //     continue;
        // }

        // 1글자 연산자 처리
        if (auto iterator = singleCharacterTokens.find(current_character); iterator != singleCharacterTokens.end()) {
            addToken(iterator->second);
            current_read_position++;
            next_read_position++;
            continue;
        }

        // 문자열
        if (current_character == "\"") {
            string string_value = readString();
            addToken(TokenType::STRING, string_value);
            continue;
        }

        // 숫자 (나중에 부동소수점 수 추가하면 수정)
        if (isNumber(current_character)) {
            string number_string = readInteger();
            addToken(TokenType::INTEGER, number_string);
            continue;
        }

        // 식별자 | 키워드
        if (isLetter(current_character)) {
            string identifier = readLetter();
            handleIdentifierAndKeywords(identifier);
            continue;
        }

        throw UnknownCharacterException(current_character, line);
    }

    // EOF는 표현할 수 있는 문자열이 없으므로 빈 문자열을 추가
    // tokens.push_back(new Token(TokenType::END_OF_FILE, "", line));
    return tokens;
}


bool Lexer::isNumber(const std::string& s) {
    return "0" <= s && s <= "9";
}

string Lexer::readInteger() {
    string integer_string;

    while (current_read_position < characters.size() && isNumber(characters[current_read_position])) {
        integer_string += characters[current_read_position];
        current_read_position++;
        next_read_position++;
    }

    return integer_string;
}

bool Lexer::isLetter(const std::string& s) {
    return ("a" <= s && s <= "z" || "A" <= s && s <= "Z" || s == "_" || "가" <= s && s <= "힣");
}

std::string Lexer::readLetter() {
    string identifier;

    while (current_read_position < characters.size()
           && (isLetter(characters[current_read_position]) || isNumber(characters[current_read_position]))) {
        identifier += characters[current_read_position];
        current_read_position++;
        next_read_position++;
    }
    return identifier;
}

string Lexer::readString() {
    string string_value;
    bool escape_mode = false;

    // characters[current_read_position]은 "\""인 상황, 다음 토큰으로 넘겨서 문자열만 추출하기
    current_read_position++;
    next_read_position++;

    while (current_read_position < characters.size()) {
        string current_character = characters[current_read_position];

        // 이스케이프 모드
        if (escape_mode) {
            if (current_character == "n") {
                string_value += "\n";
            } else if (current_character == "t") {
                string_value += "\t";
            } else if (current_character == "\"") {
                string_value += "\"";
            } else {
                string_value += "\\" + current_character;
            }
            escape_mode = false;
            current_read_position++;
            next_read_position++;
            continue;
        }

        // 이스케이프 문자
        if (current_character == "\\") {
            escape_mode = true;
            current_read_position++;
            next_read_position++;
            continue;
        }

        if (current_character == "\"") {
            current_read_position++;
            next_read_position++;
            return string_value;
        }

        // 일반 문자 처리
        string_value += current_character;
        current_read_position++;
        next_read_position++;
    }

    throw UnterminatedStringException(string_value, line);
}

// literal이 주어지지 않는 경우 lexer가 바라보는 토큰을 literal으로 전달한다.
void Lexer::addToken(TokenType type) {
    addToken(type, characters[current_read_position]);
}

void Lexer::addToken(TokenType type, string literal) {
    tokens.push_back(new Token{type, literal, line});
    if (type == TokenType::NEW_LINE) {
        line++;
    }
}

void Lexer::handleIdentifierAndKeywords(std::string& identifier) {
    if (auto iterator = keywords.find(identifier); iterator != keywords.end()) {
        addToken(iterator->second, identifier);
    } else {
        addToken(TokenType::IDENTIFIER, identifier);
    }
}
