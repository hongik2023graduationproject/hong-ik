#include "parser.h"

#include "../ast/literals.h"
#include "../exception/exception.h"
#include <iostream>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>

using namespace std;

Program* Parser::Parsing(const std::vector<Token*>& tokens) {
    this->tokens = tokens;
    initialization();

    while (current_read_position < tokens.size()) {
        if (current_token->type == TokenType::END_OF_FILE) {
            break;
        }
        auto* statement = parseStatement();
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
    current_token   = tokens.size() > current_read_position ? tokens[current_read_position] : nullptr;
    next_token      = tokens.size() > current_read_position + 1 ? tokens[current_read_position + 1] : nullptr;
    next_next_token = tokens.size() > current_read_position + 2 ? tokens[current_read_position + 2] : nullptr;
}

void Parser::setNextToken() {
    current_read_position++;
    setToken();
}

void Parser::skipToken(const TokenType type) {
    if (current_token->type != type) {
        throw UnexpectedTokenException(current_token->type, type, current_token->line);
    }
    setNextToken();
}

void Parser::checkToken(TokenType type) {
    if (current_token->type != type) {
        throw UnexpectedTokenException(current_token->type, type, current_token->line);
    }
}


// statement 파싱의 마지막에는 setNextToken()이 실행된다.
Statement* Parser::parseStatement() {
    if (current_token->type == TokenType::LBRACKET) {
        return parseInitializationStatement();
    }
    if (current_token->type == TokenType::IDENTIFIER && next_token != nullptr
        && next_token->type == TokenType::ASSIGN) {
        return parseAssignmentStatement();
    }
    if (current_token->type == TokenType::리턴) {
        return parseReturnStatement();
    }
    if (current_token->type == TokenType::만약) {
        return parseIfStatement();
    }
    if (current_token->type == TokenType::함수) {
        return parseFunctionStatement();
    }
    return parseExpressionStatement();
}

InitializationStatement* Parser::parseInitializationStatement() {
    auto* statement = new InitializationStatement();
    skipToken(TokenType::LBRACKET);

    // TODO: 현재는 자료형 자리에 적절한 자료형이 왔는 지 체크하지 않는다. 추후에 체크하는 로직 추가 예정
    // 25.07.17(tolelom): 런타임에 추가되는 타입이 있을 수 있으므로 evaluator에 위임해도 될 것으로 보임
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

AssignmentStatement* Parser::parseAssignmentStatement() {
    auto* statement = new AssignmentStatement();

    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    skipToken(TokenType::ASSIGN);

    statement->value = parseExpression(Precedence::LOWEST);
    setNextToken();
    return statement;
}

ExpressionStatement* Parser::parseExpressionStatement() {
    auto* expressionStatement       = new ExpressionStatement();
    expressionStatement->expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    return expressionStatement;
}


ReturnStatement* Parser::parseReturnStatement() {
    skipToken(TokenType::리턴);
    auto* statement       = new ReturnStatement();
    statement->expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    return statement;
}

BlockStatement* Parser::parseBlockStatement() {
    auto* statement = new BlockStatement();
    skipToken(TokenType::LBRACE);
    // 임시로 BRACE를 활용해서 BLOCK을 판별한다(C 스타일)
    // 추후에 들여쓰기로 변경할 때 수정할 예정

    while (current_token->type != TokenType::RBRACE) {
        statement->statements.push_back(parseStatement());
    }
    skipToken(TokenType::RBRACE);

    return statement;
}

IfStatement* Parser::parseIfStatement() {
    auto* statement = new IfStatement();
    skipToken(TokenType::만약);
    statement->condition = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::라면);

    // new line 스킵은 추후에 추가
    // skipToken(TokenType::NEW_LINE);

    statement->consequence = parseBlockStatement();

    if (current_token != nullptr && current_token->type == TokenType::아니면) {
        skipToken(TokenType::아니면);
        statement->then = parseBlockStatement();
    }

    return statement;
}

FunctionStatement* Parser::parseFunctionStatement() {
    auto* statement = new FunctionStatement();
    skipToken(TokenType::함수);
    skipToken(TokenType::COLON);

    if (current_token->type == TokenType::LBRACKET) {
        // TODO: goto 문 제거하기
    FLAG:
        skipToken(TokenType::LBRACKET);

        // TODO: type 체크 안하는 중 && 토큰 하나만 넘기는 중
        statement->parameterTypes.push_back(current_token);
        setNextToken();

        skipToken(TokenType::RBRACKET);

        statement->parameters.push_back(dynamic_cast<IdentifierExpression*>(parseExpression(Precedence::LOWEST)));
        setNextToken();

        if (current_token->type == TokenType::COMMA) {
            skipToken(TokenType::COMMA);
            goto FLAG;
        }
    }

    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    if (current_token->type == TokenType::RIGHT_ARROW) {
        skipToken(TokenType::RIGHT_ARROW);
        skipToken(TokenType::LBRACKET);
        statement->returnType = current_token;
        setNextToken();
        skipToken(TokenType::RBRACKET);
    }

    statement->body = parseBlockStatement();

    return statement;
}

Expression* Parser::parseExpression(Precedence precedence) {
    if (!prefixParseFunctions.contains(current_token->type)) {
        throw UnknownPrefixParseFunctionException(current_token->type, current_token->line);
    }

    PrefixParseFunction prefixParseFunction = prefixParseFunctions[current_token->type];
    Expression* expression                  = (this->*prefixParseFunction)();

    while (next_token != nullptr && precedence < getPrecedence[next_token->type]) {
        if (!infixParseFunctions.contains(next_token->type)) {
            throw UnknownInfixParseFunctionException(next_token->type, next_token->line);
        }
        InfixParseFunction infixParseFunction = infixParseFunctions[next_token->type];
        setNextToken();
        expression = (this->*infixParseFunction)(expression);
    }

    return expression;
}

Expression* Parser::parseInfixExpression(Expression* left) {
    InfixExpression* infixExpression = new InfixExpression;
    infixExpression->token           = current_token;
    infixExpression->left            = left;

    Precedence precedence = getPrecedence[current_token->type];
    setNextToken();

    infixExpression->right = parseExpression(precedence);

    return infixExpression;
}

Expression* Parser::parsePrefixExpression() {
    PrefixExpression* prefixExpression = new PrefixExpression;
    prefixExpression->token            = current_token;
    setNextToken();

    prefixExpression->right = parseExpression(Precedence::PREFIX);

    return prefixExpression;
}


Expression* Parser::parseGroupedExpression() {
    skipToken(TokenType::LPAREN);
    Expression* expression = parseExpression(Precedence::LOWEST);
    setNextToken(); // current_token을 )으로 세팅하는 과정
    checkToken(TokenType::RPAREN);
    return expression;
}

Expression* Parser::parseIdentifierExpression() {
    checkToken(TokenType::IDENTIFIER);

    auto identifier_expression  = new IdentifierExpression();
    identifier_expression->name = current_token->text;
    return identifier_expression;
}

Expression* Parser::parseCallExpression() {
    skipToken(TokenType::COLON);

    auto call_expression      = new CallExpression();
    call_expression->function = parseExpression(Precedence::LOWEST);
    skipToken(TokenType::IDENTIFIER);

    if (current_token != nullptr && current_token->type == TokenType::LPAREN) {
        skipToken(TokenType::LPAREN);
    FLAG:
        call_expression->arguments.push_back(parseExpression(Precedence::LOWEST));
        setNextToken();
        if (current_token->type == TokenType::COMMA) {
            skipToken(TokenType::COMMA);
            goto FLAG;
        }
    }

    return call_expression;
}

Expression* Parser::parseIndexExpression(Expression* left) {
    auto index_expression  = new IndexExpression();
    index_expression->name = left;
    skipToken(TokenType::LBRACKET);
    index_expression->index = parseExpression(Precedence::LOWEST);
    setNextToken();
    checkToken(TokenType::RBRACKET);

    return index_expression;
}


Expression* Parser::parseIntegerLiteral() {
    auto* integerLiteral  = new IntegerLiteral();
    integerLiteral->token = current_token;
    integerLiteral->value = stoll(current_token->text);
    return integerLiteral;
}

Expression* Parser::parseBooleanLiteral() {
    auto* booleanLiteral  = new BooleanLiteral();
    booleanLiteral->token = current_token;
    booleanLiteral->value = current_token->type == TokenType::TRUE;
    return booleanLiteral;
}

Expression* Parser::parseStringLiteral() {
    auto* stringLiteral  = new StringLiteral();
    stringLiteral->token = current_token;
    stringLiteral->value = current_token->text;
    return stringLiteral;
}

Expression* Parser::parseArrayLiteral() {
    auto* arrayLiteral = new ArrayLiteral();

    skipToken(TokenType::LBRACKET);
    while (current_token != nullptr && current_token->type != TokenType::RBRACKET) {
    flag:
        Expression* element = parseExpression(Precedence::LOWEST);
        arrayLiteral->elements.push_back(element);
        setNextToken();

        if (current_token != nullptr && current_token->type == TokenType::COMMA) {
            skipToken(TokenType::COMMA);
            goto flag;
        }
    }

    return arrayLiteral;
}
