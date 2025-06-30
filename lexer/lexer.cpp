#include "lexer.h"

#include <unordered_map>

using namespace std;

Lexer::Lexer() {
    keywords = {
        {"정수", TokenType::정수},
        {"실수", TokenType::실수},
        {"문자", TokenType::문자},
        {"return", TokenType::RETURN},
        {"만약", TokenType::만약},
        {"라면", TokenType::라면},
        {"함수", TokenType::함수},
        {"true", TokenType::TRUE},
        {"false", TokenType::FALSE},
    };
    singleCharacterTokens = {
        {"+", TokenType::PLUS},
        {"*", TokenType::ASTERISK},
        {"/", TokenType::SLASH},
        {"\n", TokenType::NEW_LINE},
        {"\t", TokenType::TAB},
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
    line                  = 1;


    tokens.clear();

    // 현재 token type에 정의된 토큰 중 identifier, integer, float, string이 미구현
    // 추후에 hash 함수 이용한 switch문 같은 가독성, 효율 좋은 코드로 변경할 필요 있음
    while (current_read_position < characters.size()) {
        string current_character = characters[current_read_position];
        string next_character    = (next_read_position < characters.size()) ? characters[next_read_position] : "";

        // 공백 무시 (토큰 생성 안함)
        // 나중에 공백 처리할 때 변경될 예정
        if (current_character == " ") {
            current_read_position++;
            next_read_position++;
            continue;
        }

        // 2글자 연산자 처리
        if (handleMultiCharacterToken(current_character, next_character)) {
            continue;
        }


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
            handleIdentifier(identifier);
            continue;
        }

        // TODO: 에러 처리
        // 잘못된 문자
    }

    // EOF는 표현할 수 있는 문자열이 없으므로 빈 문자열을 추가
    // tokens.push_back(new Token(TokenType::END_OF_FILE, "", line));
    return tokens;
}


bool Lexer::isNumber(const std::string& s) {
    return ("0" <= s && s <= "9");
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

    // characters[current_read_position]은 "\""인 상황, 다음 토큰으로 넘겨서 문자열만 추출하기
    current_read_position++;
    next_read_position++;

    while (next_read_position < characters.size()) {
        string current_character = characters[current_read_position];

        if (current_character == "\"") {
            current_read_position++;
            next_read_position++;
            break;
        }

        // 이스케이프 문자 처리가 필요하면 할 것

        // 일반 문자 처리
        string_value += current_character;
        current_read_position++;
        next_read_position++;
    }

    return string_value;
}

void Lexer::addToken(TokenType type) {
    tokens.push_back(new Token{type, characters[current_read_position], line});
}

void Lexer::addToken(TokenType type, string literal) {
    tokens.push_back(new Token{type, literal, line});
}

int Lexer::handleMultiCharacterToken(std::string& current_character, std::string& next_character) {
    if (next_character.empty()) {
        return 0;
    }

    string potential_token = current_character + next_character;
    if (multiCharacterTokens.find(potential_token) != multiCharacterTokens.end()) {
        addToken(multiCharacterTokens.at(potential_token), potential_token);

        current_read_position += 2;
        next_read_position += 2;
        return 1;
    }
    return 0;
}

void Lexer::handleIdentifier(std::string& identifier) {
    if (auto iterator = keywords.find(identifier); iterator != keywords.end()) {
        addToken(iterator->second, identifier);
    } else {
        addToken(TokenType::IDENTIFIER, identifier);
    }
}
