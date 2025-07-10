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

    // 기호
    PLUS, // +
    MINUS, // -
    ASTERISK, // *
    SLASH, // /
    ASSIGN, // =
    EQUAL, // ==
    NOT_EQUAL, // !=

    LPAREN, // (
    RPAREN, // )
    LBRACE, // {
    RBRACE, // }
    LBRACKET, // [
    RBRACKET, // ]

    COLON, // :
    SEMICOLON, // ;

    // 예약어
    정수,
    실수,
    문자,
    리턴, // return statement에서 사용
    만약,
    라면,
    함수,
    TRUE,
    FALSE,

    BITWISE_AND, // &
    LOGICAL_AND, // &&
    BITWISE_OR, // |
    LOGICAL_OR, // ||

    COMMA, // ,
    RIGHT_ARROW, // ->

    // 코드 블록
    START_BLOCK,
    END_BLOCK,

    BANG, // !

    LESS_THAN, // <
    GREATER_THAN, // >
    LESS_EQUAL, // <=
    GREATER_EQUAL, // >=
};

#endif // TOKEN_TYPE_H
