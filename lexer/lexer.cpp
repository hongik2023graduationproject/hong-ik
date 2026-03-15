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
        {"반복", TokenType::반복},
        {"동안", TokenType::동안},
        {"중단", TokenType::중단},
        {"계속", TokenType::계속},
        {"논리", TokenType::논리},
        {"배열", TokenType::배열},
        {"사전", TokenType::사전},
        {"가져오기", TokenType::가져오기},
        {"각각", TokenType::각각},
        {"에서", TokenType::에서},
        {"시도", TokenType::시도},
        {"실패", TokenType::실패},
        {"없음", TokenType::없음},
        {"비교", TokenType::비교},
        {"경우", TokenType::경우},
        {"기본", TokenType::기본},
        {"클래스", TokenType::클래스},
        {"생성", TokenType::생성},
        {"자기", TokenType::자기},
        {"부모", TokenType::부모},
        {"부터", TokenType::부터},
        {"까지", TokenType::까지},
        {"true", TokenType::TRUE},
        {"false", TokenType::FALSE},
    };
    singleCharacterTokens = {
        {"+", TokenType::PLUS},
        {"*", TokenType::ASTERISK},
        {"/", TokenType::SLASH},
        {"%", TokenType::PERCENT},
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
        {"+=", TokenType::PLUS_ASSIGN},
        {"-=", TokenType::MINUS_ASSIGN},
        {"*=", TokenType::ASTERISK_ASSIGN},
        {"/=", TokenType::SLASH_ASSIGN},
        {"%=", TokenType::PERCENT_ASSIGN},
    };
}


std::vector<std::shared_ptr<Token>> Lexer::Tokenize(const std::vector<std::string>& characters) {
    this->characters      = characters;
    current_read_position = 0;
    next_read_position    = 1;
    at_line_start         = true;
    if (indent == 0) {
        line = 1;
    }
    tokens.clear();

    // 빈 문자열 예외 처리
    if (characters.empty()) {
        for (int i = indent; i >= 0; i--) {
            addToken(TokenType::END_BLOCK, "");
        }
        return tokens;
    }


    while (current_read_position < static_cast<long long>(characters.size())) {
        string current_character = characters[current_read_position];
        string next_character    = (next_read_position < static_cast<long long>(characters.size())) ? characters[next_read_position] : "";

        // 공백 무시 (토큰 생성 안함)
        if (at_line_start) {
            int space_counter = 0;
            while (current_read_position < static_cast<long long>(characters.size())
                   && (characters[current_read_position] == " " || characters[current_read_position] == "\t")) {
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
            indent        = current_indent;
            at_line_start = false;
            continue;
        }

        if (current_character == " ") {
            current_read_position++;
            next_read_position++;
            continue;
        }

        // \r 문자 무시 (Windows 줄바꿈 호환)
        if (current_character == "\r") {
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

        // 한 줄 주석 처리 (//)
        if (current_character == "/" && next_character == "/") {
            skipLineComment();
            continue;
        }

        // 여러 줄 주석 처리 (/* */)
        if (current_character == "/" && next_character == "*") {
            skipBlockComment();
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

        // 숫자 (정수 또는 실수)
        if (isNumber(current_character)) {
            bool isFloat = false;
            string number_string = readNumber(isFloat);
            if (isFloat) {
                addToken(TokenType::FLOAT, number_string);
            } else {
                addToken(TokenType::INTEGER, number_string);
            }
            continue;
        }

        // 식별자 | 키워드
        if (isLetter(current_character)) {
            string identifier = readLetter();
            handleIdentifierAndKeywords(identifier);
            continue;
        }

        // DOT 또는 ELLIPSIS
        if (current_character == ".") {
            // ... (ELLIPSIS) 체크
            if (next_character == "."
                && next_read_position + 1 < static_cast<long long>(characters.size())
                && characters[next_read_position + 1] == ".") {
                addToken(TokenType::ELLIPSIS, "...");
                current_read_position += 3;
                next_read_position += 3;
                continue;
            }
            addToken(TokenType::DOT, ".");
            current_read_position++;
            next_read_position++;
            continue;
        }

        throw UnknownCharacterException(current_character, line);
    }

    // NOTE: 파일 모드에서 입력이 끝나도 남아있는 들여쓰기에 대해 END_BLOCK을 자동으로
    // 발행하지 않음. 이는 REPL 모드에서 여러 줄에 걸친 입력을 지원하기 위한 의도적 설계이며,
    // 파일 모드에서는 외부 호출자(evalImport 등)가 END_BLOCK을 보충해야 함.
    // M6: REPL의 들여쓰기 추적은 공백 개수 기반 휴리스틱으로, 혼합 들여쓰기 시 부정확할 수 있음.
    // M7: Lexer는 호출 간에 indent 상태를 유지하여 여러 줄 입력을 지원함.
    return tokens;
}


bool Lexer::isNumber(const std::string& s) {
    return "0" <= s && s <= "9";
}

string Lexer::readNumber(bool& isFloat) {
    string number_string;
    isFloat = false;

    while (current_read_position < static_cast<long long>(characters.size()) && isNumber(characters[current_read_position])) {
        number_string += characters[current_read_position];
        current_read_position++;
        next_read_position++;
    }

    // 소수점 처리
    if (current_read_position < static_cast<long long>(characters.size()) && characters[current_read_position] == "."
        && next_read_position < static_cast<long long>(characters.size()) && isNumber(characters[next_read_position])) {
        isFloat = true;
        number_string += ".";
        current_read_position++;
        next_read_position++;

        while (current_read_position < static_cast<long long>(characters.size()) && isNumber(characters[current_read_position])) {
            number_string += characters[current_read_position];
            current_read_position++;
            next_read_position++;
        }
    }

    return number_string;
}

// TODO: 한글 범위 비교가 문자열 비교 기반이라 일부 멀티바이트 문자가 잘못 매칭될 수 있음
bool Lexer::isLetter(const std::string& s) {
    return (("a" <= s && s <= "z") || ("A" <= s && s <= "Z") || s == "_" || ("가" <= s && s <= "힣"));
}

std::string Lexer::readLetter() {
    string identifier;

    while (current_read_position < static_cast<long long>(characters.size())
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

    while (current_read_position < static_cast<long long>(characters.size())) {
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

void Lexer::skipLineComment() {
    // "//" 이후 줄 끝까지 무시
    current_read_position += 2;
    next_read_position += 2;

    while (current_read_position < static_cast<long long>(characters.size())
           && characters[current_read_position] != "\n") {
        current_read_position++;
        next_read_position++;
    }
}

void Lexer::skipBlockComment() {
    // "/*" 이후 "*/" 까지 무시
    current_read_position += 2;
    next_read_position += 2;

    while (current_read_position < static_cast<long long>(characters.size())) {
        string current_character = characters[current_read_position];
        string next_character = (next_read_position < static_cast<long long>(characters.size()))
                                    ? characters[next_read_position] : "";

        if (current_character == "\n") {
            line++;
        }

        if (current_character == "*" && next_character == "/") {
            current_read_position += 2;
            next_read_position += 2;
            return;
        }

        current_read_position++;
        next_read_position++;
    }
}

// literal이 주어지지 않는 경우 lexer가 바라보는 토큰을 literal으로 전달한다.
void Lexer::addToken(TokenType type) {
    if (current_read_position < static_cast<long long>(characters.size())) {
        addToken(type, characters[current_read_position]);
    }
}

void Lexer::addToken(TokenType type, string literal) {
    tokens.push_back(make_shared<Token>(Token{type, std::move(literal), line}));
}

void Lexer::handleIdentifierAndKeywords(std::string& identifier) {
    if (auto iterator = keywords.find(identifier); iterator != keywords.end()) {
        addToken(iterator->second, identifier);
    } else {
        addToken(TokenType::IDENTIFIER, identifier);
    }
}
