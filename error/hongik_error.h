#ifndef HONGIK_ERROR_H
#define HONGIK_ERROR_H

#include <exception>
#include <string>

// 에러 위치 정보
struct ErrorLocation {
    long long line = 0;
    long long column = 0;
    long long endLine = 0;
    long long endColumn = 0;
};

// hongik 에러 타입
enum class HongIkErrorType {
    SYNTAX_ERROR = 0,     // 문법 에러
    RUNTIME_ERROR = 1,    // 런타임 에러
    TYPE_ERROR = 2,       // 타입 에러
    REFERENCE_ERROR = 3,  // 참조 에러
    FILE_ERROR = 4        // 파일 에러
};

// 구조화된 hongik 에러
class HongIkError : public std::exception {
public:
    HongIkErrorType type;
    std::string message;
    ErrorLocation location;
    std::string code;          // 소스 코드 일부
    std::string suggestion;    // 수정 제안 (선택)
    std::string formattedMessage; // 위치 정보 포함 메시지

    HongIkError(
        HongIkErrorType errorType,
        const std::string& msg,
        const ErrorLocation& loc = ErrorLocation(),
        const std::string& codeSnippet = "",
        const std::string& sug = ""
    )
        : type(errorType)
        , message(msg)
        , location(loc)
        , code(codeSnippet)
        , suggestion(sug)
    {
        if (location.line > 0) {
            formattedMessage = "[줄 " + std::to_string(location.line) + "] " + message;
        } else {
            formattedMessage = message;
        }
    }

    const char* what() const noexcept override {
        return formattedMessage.c_str();
    }

    // 에러 정보를 JSON 문자열로 반환
    std::string toJsonString() const;

    // 기본 런타임 에러 생성 헬퍼
    static HongIkError RuntimeError(const std::string& msg, long long line = 0) {
        ErrorLocation loc;
        loc.line = line;
        return HongIkError(HongIkErrorType::RUNTIME_ERROR, msg, loc);
    }

    // 타입 에러 생성 헬퍼
    static HongIkError TypeError(const std::string& msg, long long line = 0) {
        ErrorLocation loc;
        loc.line = line;
        return HongIkError(HongIkErrorType::TYPE_ERROR, msg, loc);
    }

    // 문법 에러 생성 헬퍼
    static HongIkError SyntaxError(const std::string& msg, long long line = 0) {
        ErrorLocation loc;
        loc.line = line;
        return HongIkError(HongIkErrorType::SYNTAX_ERROR, msg, loc);
    }
};

#endif // HONGIK_ERROR_H
