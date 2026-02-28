#ifndef PARSER_H
#define PARSER_H

#include "../ast/program.h"
#include "../token/token.h"
#include <map>
#include <memory>
#include <vector>

class Parser {
public:
    std::shared_ptr<Program> Parsing(const std::vector<std::shared_ptr<Token>>& tokens);

private:
    std::vector<std::shared_ptr<Token>> tokens;

    std::shared_ptr<Program> program;
    std::shared_ptr<Token> current_token;
    std::shared_ptr<Token> next_token;
    std::shared_ptr<Token> next_next_token;
    long long current_read_position;

    void initialization();

    void setToken();

    void setNextToken();

    void skipToken(TokenType type);

    void checkToken(TokenType type);

    std::shared_ptr<Statement> parseStatement();

    std::shared_ptr<InitializationStatement> parseInitializationStatement();

    std::shared_ptr<AssignmentStatement> parseAssignmentStatement();

    std::shared_ptr<ExpressionStatement> parseExpressionStatement();

    std::shared_ptr<ReturnStatement> parseReturnStatement();

    std::shared_ptr<BlockStatement> parseBlockStatement();

    std::shared_ptr<IfStatement> parseIfStatement();

    std::shared_ptr<FunctionStatement> parseFunctionStatement();


    using PrefixParseFunction                                     = std::shared_ptr<Expression> (Parser::*) ();
    using InfixParseFunction                                      = std::shared_ptr<Expression> (Parser::*) (std::shared_ptr<Expression>);
    std::map<TokenType, PrefixParseFunction> prefixParseFunctions = {
        {TokenType::LPAREN, &Parser::parseGroupedExpression},
        {TokenType::MINUS, &Parser::parsePrefixExpression},
        {TokenType::IDENTIFIER, &Parser::parseIdentifierExpression},
        {TokenType::COLON, &Parser::parseCallExpression},
        {TokenType::INTEGER, &Parser::parseIntegerLiteral},
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
        {TokenType::EQUAL, Precedence::EQUALS}, {TokenType::NOT_EQUAL, Precedence::EQUALS},
        {TokenType::LESS_THAN, Precedence::LESS_GREATER}, {TokenType::GREATER_THAN, Precedence::LESS_GREATER},
        {TokenType::LESS_EQUAL, Precedence::LESS_GREATER}, {TokenType::GREATER_EQUAL, Precedence::LESS_GREATER},
        {TokenType::PLUS, Precedence::SUM}, {TokenType::MINUS, Precedence::SUM},
        {TokenType::ASTERISK, Precedence::PRODUCT}, {TokenType::SLASH, Precedence::PRODUCT},
        {TokenType::LBRACKET, Precedence::INDEX}, {TokenType::LOGICAL_AND, Precedence::LOGICAL_AND},
        {TokenType::LOGICAL_OR, Precedence::LOGICAL_OR},
    };

    std::shared_ptr<Expression> parseExpression(Precedence precedence);

    std::shared_ptr<Expression> parseInfixExpression(std::shared_ptr<Expression> left);

    std::shared_ptr<Expression> parseIndexExpression(std::shared_ptr<Expression> left);


    std::shared_ptr<Expression> parsePrefixExpression();

    std::shared_ptr<Expression> parseGroupedExpression();

    std::shared_ptr<Expression> parseIdentifierExpression();

    std::shared_ptr<Expression> parseCallExpression();


    std::shared_ptr<Expression> parseIntegerLiteral();

    std::shared_ptr<Expression> parseBooleanLiteral();

    std::shared_ptr<Expression> parseStringLiteral();

    std::shared_ptr<Expression> parseArrayLiteral();
};


#endif // PARSER_H
