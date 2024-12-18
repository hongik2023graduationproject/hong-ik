#include "token.h"

using namespace std;

std::string tokenTypeToString(TokenType type) {
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
    }
    // 토큰 타입이 알 수 없는 경우는 일반적으로 발생하지 않지만
    // 토큰 타입이 지정되지 않은 토큰이 있을 경우가 있을 수 있음
    // 에러 처리 하는 게 현명해 보임
    return "UNKNOWN";
}
