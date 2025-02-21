#include <stdexcept>
#include <utility>

#include "parser.h"
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
        throw std::runtime_error("Unexpected token, " + TokenTypeToString(current_token->type) + ", " + TokenTypeToString(type));
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
