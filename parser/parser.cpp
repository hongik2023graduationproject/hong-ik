#include <stdexcept>
#include <utility>

#include "parser.h"

#include <iostream>
#include <ostream>
#include <set>

#include "../ast/literals.h"

using namespace std;

Program *Parser::Parsing(const std::vector<Token *> &tokens) {
    this->tokens = tokens;
    initialization();

    while (current_read_position < tokens.size()) {
        if (current_token->type == TokenType::END_OF_FILE) {
            break;
        }
        auto *statement = parseStatement();
        program->statements.push_back(statement);
    }

    return program;
}


void Parser::initialization() {
    delete program;
    program = new Program();

    current_read_position = 0;
    setToken();
}


void Parser::setToken() {
    current_token = tokens.size() > current_read_position ? tokens[current_read_position] : nullptr;
    next_token = tokens.size() > current_read_position + 1 ? tokens[current_read_position + 1] : nullptr;
    next_next_token = tokens.size() > current_read_position + 2 ? tokens[current_read_position + 2] : nullptr;
}

void Parser::setNextToken() {
    current_read_position++;
    setToken();
}

void Parser::skipToken(TokenType type) {
    if (current_token->type != type) {
        // error
        throw std::runtime_error(
            "Unexpected token, " + TokenTypeToString(current_token->type) + ", " + TokenTypeToString(type) + to_string(current_token->line));
    }
    setNextToken();
}

void Parser::checkToken(TokenType type) {
    if (current_token->type != type) {
        throw std::runtime_error("Unexpected token");
    }
}


// statement 파싱의 마지막에는 setNextToken()이 실행된다.
Statement *Parser::parseStatement() {
    if (current_token->type == TokenType::LBRACKET) {
        return parseAssignmentStatement();
    }
    if (current_token->type == TokenType::RETURN) {
        return parseReturnStatement();
    }
    if (current_token->type == TokenType::만약) {
        return parseIfStatement();
    }
    return parseExpressionStatement();
}

AssignmentStatement *Parser::parseAssignmentStatement() {
    auto *statement = new AssignmentStatement();
    skipToken(TokenType::LBRACKET);
    // TODO: 현재는 자료형 자리에 적절한 자료형이 왔는 지 체크하지 않는다. 추후에 체크하는 로직 추가 예정
    statement->type = current_token;
    setNextToken();
    skipToken(TokenType::RBRACKET);
    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);
    skipToken(TokenType::ASSIGN);
    statement->value = parseExpression(Precedence::LOWEST);
    setNextToken();
    return statement;
}

ExpressionStatement *Parser::parseExpressionStatement() {
    auto *expressionStatement = new ExpressionStatement();
    expressionStatement->expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    return expressionStatement;
}


ReturnStatement *Parser::parseReturnStatement() {
    skipToken(TokenType::RETURN);
    auto *statement = new ReturnStatement();
    statement->expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    return statement;
}

BlockStatement *Parser::parseBlockStatement() {
    auto *statement = new BlockStatement();
    skipToken(TokenType::LBRACE);
    // 임시로 BRACE를 활용해서 BLOCK을 판별한다(C 스타일)
    // 추후에 들여쓰기로 변경할 때 수정할 예정

    while (current_token->type != TokenType::RBRACE) {
        statement->statements.push_back(parseStatement());
    }
    skipToken(TokenType::RBRACE);

    return statement;
}

IfStatement *Parser::parseIfStatement() {
    auto *statement = new IfStatement();
    skipToken(TokenType::만약);
    statement->condition = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::라면);

    // new line 스킵은 추후에 추가
    // skipToken(TokenType::NEW_LINE);

    statement->consequence = parseBlockStatement();

    // 여기에 else 구문 추가시 작성할 것

    return statement;
}

Expression *Parser::parseExpression(Precedence precedence) {
    if (!prefixParseFunctions.contains(current_token->type)) {
        // 나중에 에러 처리 추가할 것
        throw runtime_error("No prefix function found");
    }

    PrefixParseFunction prefixParseFunction = prefixParseFunctions[current_token->type];
    Expression *expression = (this->*prefixParseFunction)();

    while (next_token->type != TokenType::END_OF_FILE && precedence < getPrecedence[next_token->type]) {
        if (!infixParseFunctions.contains(next_token->type)) {
            throw runtime_error("No infix function found");
        }
        InfixParseFunction infixParseFunction = infixParseFunctions[next_token->type];
        setNextToken();
        expression = (this->*infixParseFunction)(expression);
    }

    return expression;
}

Expression *Parser::parseInfixExpression(Expression *left) {
    InfixExpression *infixExpression = new InfixExpression;
    infixExpression->token = current_token;
    infixExpression->left = left;

    Precedence precedence = getPrecedence[current_token->type];
    setNextToken();

    infixExpression->right = parseExpression(precedence);

    return infixExpression;
}

Expression *Parser::parsePrefixExpression() {
    PrefixExpression *prefixExpression = new PrefixExpression;
    prefixExpression->token = current_token;
    setNextToken();

    prefixExpression->right = parseExpression(Precedence::PREFIX);

    return prefixExpression;
}


Expression *Parser::parseGroupedExpression() {
    skipToken(TokenType::LPAREN);
    Expression *expression = parseExpression(Precedence::LOWEST);
    setNextToken(); // current_token을 )으로 세팅하는 과정
    checkToken(TokenType::RPAREN);
    return expression;
}

Expression *Parser::parseIntegerLiteral() {
    auto *integerLiteral = new IntegerLiteral();
    integerLiteral->token = current_token;
    integerLiteral->value = stoll(current_token->text);
    return integerLiteral;
}
