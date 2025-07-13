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
    explicit UnterminatedStringException(long long line)
        : message("문자열이 line: " + std::to_string(line) + "에서 닫는 따옴표 없이 끝났습니다. '\"'가 필요합니다.") {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};



#endif // EXCEPTION_H
