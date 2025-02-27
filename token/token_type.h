#ifndef TOKEN_TYPE_H
#define TOKEN_TYPE_H

enum class TokenType {
    ILLEGAL, // 알 수 없는 문자

    // 파일과 관련된 문자들
    END_OF_FILE, // 파일의 끝을 의미
    NEW_LINE, // 개행 문자

    SPACE,
    TAB,
    IDENTIFIER, // 식별자

    // 기본 자료형
    INTEGER, // 정수
    FLOAT, // 부동 소수점 수
    STRING, // 문자열

    // 예약어
    _INTEGER,
    _FLOAT,
    RETURN, // return statement에서 사용
    IF, // 만약
    END_IF, // 라면

    // 코드 블록
    START_BLOCK,
    END_BLOCK,

    // 기호
    PLUS, // +
    MINUS, // -
    ASTERISK, // *
    SLASH, // /
    ASSIGN, // =
    EQUAL, // ==
    NOT_EQUAL, // !=

    BANG, // !

    LPAREN, // (
    RPAREN, // )
    LBRACKET, // [
    RBRACKET, // ]
};

#endif //TOKEN_TYPE_H
