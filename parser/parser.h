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

    bool isTypeKeyword(TokenType type);

    std::shared_ptr<Statement> parseStatement();

    std::shared_ptr<InitializationStatement> parseInitializationStatement();

    std::shared_ptr<AssignmentStatement> parseAssignmentStatement();

    std::shared_ptr<CompoundAssignmentStatement> parseCompoundAssignmentStatement();

    std::shared_ptr<ExpressionStatement> parseExpressionStatement();

    std::shared_ptr<ReturnStatement> parseReturnStatement();

    std::shared_ptr<BlockStatement> parseBlockStatement();

    std::shared_ptr<IfStatement> parseIfStatement();

    std::shared_ptr<WhileStatement> parseWhileStatement();

    std::shared_ptr<ForEachStatement> parseForEachStatement();

    std::shared_ptr<TryCatchStatement> parseTryCatchStatement();

    std::shared_ptr<FunctionStatement> parseFunctionStatement();

    std::shared_ptr<ImportStatement> parseImportStatement();

    std::shared_ptr<MatchStatement> parseMatchStatement();

    std::shared_ptr<ClassStatement> parseClassStatement();


    using PrefixParseFunction                                     = std::shared_ptr<Expression> (Parser::*) ();
    using InfixParseFunction                                      = std::shared_ptr<Expression> (Parser::*) (std::shared_ptr<Expression>);
    std::map<TokenType, PrefixParseFunction> prefixParseFunctions = {
        {TokenType::LPAREN, &Parser::parseGroupedExpression},
        {TokenType::MINUS, &Parser::parsePrefixExpression},
        {TokenType::BANG, &Parser::parsePrefixExpression},
        {TokenType::IDENTIFIER, &Parser::parseIdentifierExpression},
        {TokenType::자기, &Parser::parseSelfExpression},
        {TokenType::INTEGER, &Parser::parseIntegerLiteral},
        {TokenType::FLOAT, &Parser::parseFloatLiteral},
        {TokenType::TRUE, &Parser::parseBooleanLiteral},
        {TokenType::FALSE, &Parser::parseBooleanLiteral},
        {TokenType::STRING, &Parser::parseStringLiteral},
        {TokenType::LBRACKET, &Parser::parseArrayLiteral},
        {TokenType::LBRACE, &Parser::parseHashMapLiteral},
        {TokenType::없음, &Parser::parseNullLiteral},
    };
    std::map<TokenType, InfixParseFunction> infixParseFunctions = {
        {TokenType::PLUS, &Parser::parseInfixExpression},
        {TokenType::MINUS, &Parser::parseInfixExpression},
        {TokenType::ASTERISK, &Parser::parseInfixExpression},
        {TokenType::SLASH, &Parser::parseInfixExpression},
        {TokenType::PERCENT, &Parser::parseInfixExpression},
        {TokenType::EQUAL, &Parser::parseInfixExpression},
        {TokenType::NOT_EQUAL, &Parser::parseInfixExpression},
        {TokenType::LESS_THAN, &Parser::parseInfixExpression},
        {TokenType::GREATER_THAN, &Parser::parseInfixExpression},
        {TokenType::LESS_EQUAL, &Parser::parseInfixExpression},
        {TokenType::GREATER_EQUAL, &Parser::parseInfixExpression},
        {TokenType::LOGICAL_AND, &Parser::parseInfixExpression},
        {TokenType::LOGICAL_OR, &Parser::parseInfixExpression},
        {TokenType::BITWISE_AND, &Parser::parseInfixExpression},
        {TokenType::BITWISE_OR, &Parser::parseInfixExpression},
        {TokenType::LBRACKET, &Parser::parseIndexExpression},
        {TokenType::LPAREN, &Parser::parseCallInfixExpression},
        {TokenType::DOT, &Parser::parseMemberAccessExpression},
    };

    enum class Precedence {
        LOWEST,
        LOGICAL_OR, // ||
        LOGICAL_AND, // &&
        BITWISE_OR_P, // |
        BITWISE_AND_P, // &
        EQUALS, // ==
        LESS_GREATER, // <, >
        SUM, // +, -
        PRODUCT, // *, /, %
        PREFIX, // -X, !X
        CALL, // function(x)
        INDEX, // array[index]
        MEMBER, // object.field
    };

    std::map<TokenType, Precedence> getPrecedence = {
        {TokenType::EQUAL, Precedence::EQUALS}, {TokenType::NOT_EQUAL, Precedence::EQUALS},
        {TokenType::LESS_THAN, Precedence::LESS_GREATER}, {TokenType::GREATER_THAN, Precedence::LESS_GREATER},
        {TokenType::LESS_EQUAL, Precedence::LESS_GREATER}, {TokenType::GREATER_EQUAL, Precedence::LESS_GREATER},
        {TokenType::PLUS, Precedence::SUM}, {TokenType::MINUS, Precedence::SUM},
        {TokenType::ASTERISK, Precedence::PRODUCT}, {TokenType::SLASH, Precedence::PRODUCT},
        {TokenType::PERCENT, Precedence::PRODUCT},
        {TokenType::LPAREN, Precedence::CALL},
        {TokenType::LBRACKET, Precedence::INDEX},
        {TokenType::LOGICAL_AND, Precedence::LOGICAL_AND},
        {TokenType::LOGICAL_OR, Precedence::LOGICAL_OR},
        {TokenType::BITWISE_AND, Precedence::BITWISE_AND_P},
        {TokenType::BITWISE_OR, Precedence::BITWISE_OR_P},
        {TokenType::DOT, Precedence::MEMBER},
    };

    std::shared_ptr<Expression> parseExpression(Precedence precedence);

    std::shared_ptr<Expression> parseInfixExpression(std::shared_ptr<Expression> left);

    std::shared_ptr<Expression> parseIndexExpression(std::shared_ptr<Expression> left);

    std::shared_ptr<Expression> parseCallInfixExpression(std::shared_ptr<Expression> left);

    std::shared_ptr<Expression> parseMemberAccessExpression(std::shared_ptr<Expression> left);


    std::shared_ptr<Expression> parsePrefixExpression();

    std::shared_ptr<Expression> parseGroupedExpression();

    std::shared_ptr<Expression> parseIdentifierExpression();

    std::shared_ptr<Expression> parseSelfExpression();


    std::shared_ptr<Expression> parseIntegerLiteral();

    std::shared_ptr<Expression> parseFloatLiteral();

    std::shared_ptr<Expression> parseBooleanLiteral();

    std::shared_ptr<Expression> parseStringLiteral();

    std::shared_ptr<Expression> parseArrayLiteral();

    std::shared_ptr<Expression> parseHashMapLiteral();

    std::shared_ptr<Expression> parseNullLiteral();

    bool isCompoundAssignToken(TokenType type);
};


#endif // PARSER_H
