#ifndef PARSER_H
#define PARSER_H

#include <map>
#include <vector>

#include "../token/token.h"
#include "../ast/program.h"

class Parser {
public:
    Program *Parsing(const std::vector<Token *> &tokens);

private:
    std::vector<Token *> tokens;

    Program *program;
    Token *current_token;
    Token *next_token;
    Token *next_next_token;
    long long current_read_position;

    void initialization();

    void setToken();

    void setNextToken();

    void skipToken(TokenType type);

    void checkToken(TokenType type);

    Statement *parseStatement();

    InitializationStatement *parseInitializationStatement();

    AssignmentStatement *parseAssignmentStatement();

    ExpressionStatement *parseExpressionStatement();

    ReturnStatement *parseReturnStatement();

    BlockStatement *parseBlockStatement();

    IfStatement *parseIfStatement();

    FunctionStatement *parseFunctionStatement();


    using PrefixParseFunction = Expression* (Parser::*)();
    using InfixParseFunction = Expression* (Parser::*)(Expression *);
    std::map<TokenType, PrefixParseFunction> prefixParseFunctions = {
        {TokenType::INTEGER, &Parser::parseIntegerLiteral},
        {TokenType::LPAREN, &Parser::parseGroupedExpression},
        {TokenType::MINUS, &Parser::parsePrefixExpression},
        {TokenType::IDENTIFIER, &Parser::parseIdentifierExpression},
        {TokenType::COLON, &Parser::parseCallExpression},
        {TokenType::TRUE, &Parser::parseBooleanLiteral},
        {TokenType::FALSE, &Parser::parseBooleanLiteral},
        {TokenType::STRING, &Parser::parseStringLiteral},
        {TokenType::LBRACKET, &Parser::parseArrayLiteral},
    };
    std::map<TokenType, InfixParseFunction> infixParseFunctions = {
        {TokenType::PLUS, &Parser::parseInfixExpression},
        {TokenType::MINUS, &Parser::parseInfixExpression},
        {TokenType::ASTERISK, &Parser::parseInfixExpression},
        {TokenType::SLASH, &Parser::parseInfixExpression},
        {TokenType::EQUAL, &Parser::parseInfixExpression},
        {TokenType::NOT_EQUAL, &Parser::parseInfixExpression},
        {TokenType::LOGICAL_AND, &Parser::parseInfixExpression},
        {TokenType::LOGICAL_OR, &Parser::parseInfixExpression},
        {TokenType::LBRACKET, &Parser::parseIndexExpression},
    };

    enum class Precedence {
        LOWEST,
        LOGICAL_OR, // ||
        LOGICAL_AND, // &&
        EQUALS, // ==
        LESS_GREATER, // <, >
        SUM, // +, -
        PRODUCT, // *, /
        PREFIX, // -X, !X
        CALL, // myFunction(x)
        INDEX, // array[index]
    };

    std::map<TokenType, Precedence> getPrecedence = {
        {TokenType::EQUAL, Precedence::EQUALS},
        {TokenType::NOT_EQUAL, Precedence::EQUALS},
        {TokenType::LESS_THAN, Precedence::LESS_GREATER},
        {TokenType::GREATER_THAN, Precedence::LESS_GREATER},
        {TokenType::LESS_EQUAL, Precedence::LESS_GREATER},
        {TokenType::GREATER_EQUAL, Precedence::LESS_GREATER},
        {TokenType::PLUS, Precedence::SUM},
        {TokenType::MINUS, Precedence::SUM},
        {TokenType::ASTERISK, Precedence::PRODUCT},
        {TokenType::SLASH, Precedence::PRODUCT},
        {TokenType::LBRACKET, Precedence::INDEX},
        {TokenType::LOGICAL_AND, Precedence::LOGICAL_AND},
        {TokenType::LOGICAL_OR, Precedence::LOGICAL_OR},
        {TokenType::COLON, Precedence::CALL}, // TODO: 이 부분은 검증할 것(생각대로 작성만 함)
    };

    Expression *parseExpression(Precedence precedence);

    Expression *parseInfixExpression(Expression *left);

    Expression *parseIndexExpression(Expression* left);


    Expression *parsePrefixExpression();

    Expression *parseGroupedExpression();

    Expression *parseIdentifierExpression();

    Expression *parseCallExpression();


    Expression *parseIntegerLiteral();

    Expression *parseBooleanLiteral();

    Expression *parseStringLiteral();

    Expression *parseArrayLiteral();
};


#endif //PARSER_H
