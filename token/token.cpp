#include "token.h"

using namespace std;

std::string TokenTypeToString(TokenType type) {
    switch (type) {
    case TokenType::ILLEGAL:
        return "ILLEGAL";
    case TokenType::END_OF_FILE:
        return "END_OF_FILE";
    case TokenType::NEW_LINE:
        return "NEW_LINE";
    case TokenType::SPACE:
        return "SPACE";
    case TokenType::TAB:
        return "TAB";
    case TokenType::IDENTIFIER:
        return "IDENTIFIER";
    case TokenType::INTEGER:
        return "INTEGER";
    case TokenType::FLOAT:
        return "FLOAT";
    case TokenType::STRING:
        return "STRING";
    case TokenType::PLUS:
        return "PLUS";
    case TokenType::MINUS:
        return "MINUS";
    case TokenType::ASTERISK:
        return "ASTERISK";
    case TokenType::SLASH:
        return "SLASH";
    case TokenType::ASSIGN:
        return "ASSIGN";
    case TokenType::EQUAL:
        return "EQUAL";
    case TokenType::NOT_EQUAL:
        return "NOT_EQUAL";
    case TokenType::LPAREN:
        return "LPAREN";
    case TokenType::RPAREN:
        return "RPAREN";
    case TokenType::LBRACE:
        return "LBRACE";
    case TokenType::RBRACE:
        return "RBRACE";
    case TokenType::LBRACKET:
        return "LBRACKET";
    case TokenType::RBRACKET:
        return "RBRACKET";
    case TokenType::COLON:
        return "COLON";
    case TokenType::SEMICOLON:
        return "SEMICOLON";
    case TokenType::정수:
        return "정수";
    case TokenType::실수:
        return "실수";
    case TokenType::문자:
        return "문자";
    case TokenType::RETURN:
        return "RETURN";
    case TokenType::만약:
        return "만약";
    case TokenType::라면:
        return "라면";
    case TokenType::함수:
        return "함수";
    case TokenType::TRUE:
        return "TRUE";
    case TokenType::FALSE:
        return "FALSE";
    case TokenType::BITWISE_AND:
        return "BITWISE_AND";
    case TokenType::LOGICAL_AND:
        return "LOGICAL_AND";
    case TokenType::BITWISE_OR:
        return "BITWISE_OR";
    case TokenType::LOGICAL_OR:
        return "LOGICAL_OR";
    case TokenType::COMMA:
        return "COMMA";
    case TokenType::RIGHT_ARROW:
        return "RIGHT_ARROW";
    case TokenType::START_BLOCK:
        return "START_BLOCK";
    case TokenType::END_BLOCK:
        return "END_BLOCK";
    case TokenType::BANG:
        return "BANG";
    case TokenType::LESS_THAN:
        return "LESS_THAN";
    case TokenType::GREATER_THAN:
        return "GREATER_THAN";
    case TokenType::LESS_EQUAL:
        return "LESS_EQUAL";
    case TokenType::GREATER_EQUAL:
        return "GREATER_EQUAL";

    default:
        break;
    }
    // 토큰 타입이 알 수 없는 경우는 일반적으로 발생하지 않지만
    // 토큰 타입이 지정되지 않은 토큰이 있을 경우가 있을 수 있음
    // 에러 처리 하는 게 현명해 보임
    return "UNKNOWN";
}
