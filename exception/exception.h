#ifndef EXCEPTION_H
#define EXCEPTION_H
#include <exception>
#include <string>


// lexer exception
class UnknownCharacterException : public std::exception {
private:
    std::string message;

public:
    explicit UnknownCharacterException(std::string c, long long line)
        : message(std::string("알 수 없는 문자 '") + c + "'가 line: " + std::to_string(line) + "에서 입력되었습니다.") {
    }
    const char* what() const noexcept override {
        return message.c_str();
    }
};

class UnterminatedStringException : public std::exception {
private:
    std::string message;

public:
    explicit UnterminatedStringException(std::string s, long long line)
        : message("문자열: " + s + " 이 line: " + std::to_string(line) + "에서 닫는 따옴표 없이 끝났습니다. '\"'가 필요합니다.") {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

// parser exception
class UnknownPrefixParseFunctionException : public std::exception {
private:
    std::string message;

public:
    explicit UnknownPrefixParseFunctionException(const TokenType type, long long line)
        : message("line " + std::to_string(line) + ": 해당 토큰 타입 '" + TokenTypeToString(type)
                  + "' 에 대한 prefix 파서 함수가 정의되어 있지 않습니다.") {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

class UnexpectedTokenException : public std::exception {
private:
    std::string message;

public:
    UnexpectedTokenException(TokenType got, TokenType expected, long long line)
        : message("line " + std::to_string(line) + ": 예상하지 못한 토큰입니다. "
                  "현재 토큰: '" + TokenTypeToString(got) +
                  "', 예상 토큰: '" + TokenTypeToString(expected) + "'") {}

    const char* what() const noexcept override {
        return message.c_str();
    }
};

class UnknownInfixParseFunctionException : public std::exception {
private:
    std::string message;

public:
    UnknownInfixParseFunctionException(const TokenType type, long long line)
        : message("line " + std::to_string(line) + ": "
                  "해당 토큰 타입 '" + TokenTypeToString(type) +
                  "' 에 대한 infix 파서 함수가 정의되어 있지 않습니다.") {}

    const char* what() const noexcept override {
        return message.c_str();
    }
};

#endif // EXCEPTION_H
