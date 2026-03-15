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
    PERCENT, // %
    ASSIGN, // =
    EQUAL, // ==
    NOT_EQUAL, // !=

    // 복합 대입 연산자
    PLUS_ASSIGN, // +=
    MINUS_ASSIGN, // -=
    ASTERISK_ASSIGN, // *=
    SLASH_ASSIGN, // /=
    PERCENT_ASSIGN, // %=

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
    아니면,
    함수,
    TRUE,
    FALSE,
    반복, // while 루프
    동안, // while 조건 종료 키워드
    중단, // break
    계속, // continue
    논리, // boolean type
    배열, // array type
    사전, // map type
    각각, // for-each
    에서, // in (for-each)
    시도, // try
    실패, // catch
    가져오기, // import
    없음, // null
    비교, // match/switch
    경우, // case
    기본, // default
    클래스, // class
    생성, // constructor
    자기, // self/this
    부모, // parent/super
    부터, // from (for range loop)
    까지, // to (for range loop)

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

    DOT, // .
    ELLIPSIS, // ...
};

#endif // TOKEN_TYPE_H
