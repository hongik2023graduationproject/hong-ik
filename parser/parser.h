#ifndef PARSER_H
#define PARSER_H

#include <map>
#include <vector>

#include "../token/token.h"
#include "../ast/program.h"

class parser {
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

    Statement *parseStatement();

    ExpressionStatement *parseExpressionStatement();


    using PrefixParseFunction = Expression* (parser::*)();
    using InfixParseFunction = Expression* (parser::*)(Expression *);
    std::map<TokenType, PrefixParseFunction> prefixParseFunctions = {
        {TokenType::INTEGER, &parser::parseIntegerLiteral},
    };
    std::map<TokenType, InfixParseFunction> infixParseFunctions = {
        {TokenType::PLUS, &parser::parseInfixExpression},
        {TokenType::MINUS, &parser::parseInfixExpression},
        {TokenType::ASTERISK, &parser::parseInfixExpression},
        {TokenType::SLASH, &parser::parseInfixExpression},
        {TokenType::EQUAL, &parser::parseInfixExpression},
        {TokenType::NOT_EQUAL, &parser::parseInfixExpression},
    };

    enum class Precedence {
        LOWEST,
        EQUALS, // ==
        LESSGREATER, // <, >
        SUM, // +
        PRODUCT, // *
        PREFIX, // -X, !X
        CALL, // myFunction(x)
        INDEX, // array[index]
    };

    std::map<TokenType, Precedence> getPrecedence = {
        {TokenType::EQUAL, Precedence::EQUALS},
        {TokenType::NOT_EQUAL, Precedence::EQUALS},
        // {TokenType::LESS_THAN, Precedence::LESSGREATER},
        // {TokenType::GREATER_THAN, Precedence::LESSGREATER},
        // {TokenType::LESS_EQUAL, Precedence::LESSGREATER},
        // {TokenType::GREATER_EQUAL, Precedence::LESSGREATER},
        {TokenType::PLUS, Precedence::SUM},
        {TokenType::MINUS, Precedence::SUM},
        {TokenType::ASTERISK, Precedence::PRODUCT},
        {TokenType::SLASH, Precedence::PRODUCT},
        // {TokenType::LBRACKET, Precedence::INDEX},
    };

    Expression *parseExpression(Precedence precedence);

    Expression *parseInfixExpression(Expression *left);

    Expression *parseIntegerLiteral();
};


#endif //PARSER_H
