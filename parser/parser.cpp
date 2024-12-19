#include <stdexcept>
#include <utility>

#include "parser.h"
#include "../ast/literals.h"

using namespace std;

Program *parser::Parsing(const std::vector<Token *> &tokens) {
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


void parser::initialization() {
    delete program;
    program = new Program();

    current_read_position = 0;
    setToken();
}


void parser::setToken() {
    current_token = tokens.size() > current_read_position ? tokens[current_read_position] : nullptr;
    next_token = tokens.size() > current_read_position + 1 ? tokens[current_read_position + 1] : nullptr;
    next_next_token = tokens.size() > current_read_position + 2 ? tokens[current_read_position + 2] : nullptr;
}

void parser::setNextToken() {
    current_read_position++;
    setToken();
}


// statement 파싱의 마지막에는 setNextToken()이 실행된다.
Statement *parser::parseStatement() {
    return parseExpressionStatement();
}

ExpressionStatement *parser::parseExpressionStatement() {
    auto *expressionStatement = new ExpressionStatement();
    expressionStatement->expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    return expressionStatement;
}


Expression *parser::parseExpression(Precedence precedence) {
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

Expression *parser::parseInfixExpression(Expression *left) {
    InfixExpression *infixExpression = new InfixExpression;
    infixExpression->token = current_token;
    infixExpression->left = left;

    Precedence precedence = getPrecedence[current_token->type];
    setNextToken();

    infixExpression->right = parseExpression(precedence);

    return infixExpression;
}

Expression *parser::parseIntegerLiteral() {
    auto *integerLiteral = new IntegerLiteral();
    integerLiteral->token = current_token;
    integerLiteral->value = stoll(current_token->text);
    return integerLiteral;
}