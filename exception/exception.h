#ifndef EXCEPTION_H
#define EXCEPTION_H
#include <exception>
#include <string>
#include "../token/token.h"


// 위치 정보를 보유한 진단용 예외 베이스 (렉서/파서 → LSP·CLI 공용).
// column/endColumn은 0-based 코드포인트, 미상이면 0.
class ParseError : public std::exception {
public:
    long long line = 0;
    long long column = 0;
    long long endColumn = 0;
    ParseError(std::string msg, long long line, long long column, long long endColumn)
        : line(line), column(column), endColumn(endColumn), message_(std::move(msg)) {}
    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

// lexer exception — 시그니처 유지 (lexer.cpp 무수정), 위치는 line만
class UnknownCharacterException : public ParseError {
public:
    explicit UnknownCharacterException(std::string c, long long line)
        : ParseError("[줄 " + std::to_string(line) + "] 알 수 없는 문자 '" + c + "'가 입력되었습니다.", line, 0, 0) {}
};

class UnterminatedStringException : public ParseError {
public:
    explicit UnterminatedStringException(std::string s, long long line)
        : ParseError("[줄 " + std::to_string(line) + "] 문자열: " + s
                         + " 이 닫는 따옴표 없이 끝났습니다. '\"'가 필요합니다.",
                     line, 0, 0) {}
};

// parser exception — Token 참조로 전환 (위치 전파)
class UnknownPrefixParseFunctionException : public ParseError {
public:
    UnknownPrefixParseFunctionException(const TokenType type, const Token& tok)
        : ParseError("[줄 " + std::to_string(tok.line) + "] 해당 토큰 타입 '" + TokenTypeToString(type)
                         + "' 에 대한 prefix 파서 함수가 정의되어 있지 않습니다.",
                     tok.line, tok.column, tok.endColumn) {}
};

class UnexpectedTokenException : public ParseError {
public:
    UnexpectedTokenException(TokenType got, TokenType expected, const Token& tok)
        : ParseError("[줄 " + std::to_string(tok.line)
                         + "] 예상하지 못한 토큰입니다. "
                           "현재 토큰: '"
                         + TokenTypeToString(got) + "', 예상 토큰: '" + TokenTypeToString(expected) + "'",
                     tok.line, tok.column, tok.endColumn) {}
};

class UnknownInfixParseFunctionException : public ParseError {
public:
    UnknownInfixParseFunctionException(const TokenType type, const Token& tok)
        : ParseError("[줄 " + std::to_string(tok.line) + "] 해당 토큰 타입 '" + TokenTypeToString(type)
                         + "' 에 대한 infix 파서 함수가 정의되어 있지 않습니다.",
                     tok.line, tok.column, tok.endColumn) {}
};

class NoTokenException : public ParseError {
public:
    NoTokenException(const Token& lastTok, TokenType expected)
        : ParseError("[줄 " + std::to_string(lastTok.line) + "] 토큰 타입: " + TokenTypeToString(expected)
                         + "이 예상되었으나 토큰이 존재하지 않습니다.",
                     lastTok.line, lastTok.column, lastTok.endColumn) {}
};


// evaluator exception
class RuntimeException : public std::exception {
private:
    std::string message;

public:
    RuntimeException(const std::string& msg, long long line)
        : message(line > 0 ? ("[줄 " + std::to_string(line) + "] " + msg) : msg) {}

    explicit RuntimeException(const std::string& msg)
        : message(msg) {}

    const char* what() const noexcept override {
        return message.c_str();
    }
};


#endif // EXCEPTION_H
