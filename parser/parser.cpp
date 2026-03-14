#include "parser.h"

#include "../ast/literals.h"
#include "../exception/exception.h"
#include <iostream>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>

using namespace std;

shared_ptr<Program> Parser::Parsing(const std::vector<std::shared_ptr<Token>>& tokens) {
    this->tokens = tokens;
    initialization();

    while (current_read_position < static_cast<long long>(tokens.size())) {
        if (current_token->type == TokenType::END_OF_FILE) {
            break;
        }
        auto statement = parseStatement();
        if (statement != nullptr) {
            program->statements.push_back(statement);
        }
    }

    return program;
}


void Parser::initialization() {
    program               = make_shared<Program>();
    current_read_position = 0;
    setToken();
}


void Parser::setToken() {
    current_token   = static_cast<long long>(tokens.size()) > current_read_position ? tokens[current_read_position] : nullptr;
    next_token      = static_cast<long long>(tokens.size()) > current_read_position + 1 ? tokens[current_read_position + 1] : nullptr;
    next_next_token = static_cast<long long>(tokens.size()) > current_read_position + 2 ? tokens[current_read_position + 2] : nullptr;
}

void Parser::setNextToken() {
    current_read_position++;
    setToken();
}

void Parser::skipToken(const TokenType type) {
    if (current_token == nullptr) {
        throw NoTokenException(tokens[current_read_position - 1]->line, type);
    }
    if (current_token->type != type) {
        throw UnexpectedTokenException(current_token->type, type, current_token->line);
    }
    setNextToken();
}

void Parser::checkToken(TokenType type) {
    if (current_token == nullptr) {
        throw NoTokenException(tokens[current_read_position - 1]->line, type);
    }
    if (current_token->type != type) {
        throw UnexpectedTokenException(current_token->type, type, current_token->line);
    }
}

bool Parser::isTypeKeyword(TokenType type) {
    return type == TokenType::정수 || type == TokenType::실수 || type == TokenType::문자
           || type == TokenType::논리 || type == TokenType::배열 || type == TokenType::사전;
}

bool Parser::isCompoundAssignToken(TokenType type) {
    return type == TokenType::PLUS_ASSIGN || type == TokenType::MINUS_ASSIGN
           || type == TokenType::ASTERISK_ASSIGN || type == TokenType::SLASH_ASSIGN
           || type == TokenType::PERCENT_ASSIGN;
}


// statement 파싱의 마지막에는 setNextToken()이 실행된다.
shared_ptr<Statement> Parser::parseStatement() {
    // 변수 선언: 정수 x = 10
    if (isTypeKeyword(current_token->type) && next_token != nullptr
        && next_token->type == TokenType::IDENTIFIER && next_next_token != nullptr
        && next_next_token->type == TokenType::ASSIGN) {
        return parseInitializationStatement();
    }
    if (current_token->type == TokenType::IDENTIFIER && next_token != nullptr
        && next_token->type == TokenType::ASSIGN) {
        return parseAssignmentStatement();
    }
    if (current_token->type == TokenType::IDENTIFIER && next_token != nullptr
        && isCompoundAssignToken(next_token->type)) {
        return parseCompoundAssignmentStatement();
    }
    if (current_token->type == TokenType::리턴) {
        return parseReturnStatement();
    }
    if (current_token->type == TokenType::만약) {
        return parseIfStatement();
    }
    if (current_token->type == TokenType::반복) {
        return parseWhileStatement();
    }
    if (current_token->type == TokenType::각각) {
        return parseForEachStatement();
    }
    if (current_token->type == TokenType::시도) {
        return parseTryCatchStatement();
    }
    if (current_token->type == TokenType::중단) {
        skipToken(TokenType::중단);
        skipToken(TokenType::NEW_LINE);
        return make_shared<BreakStatement>();
    }
    if (current_token->type == TokenType::함수) {
        return parseFunctionStatement();
    }
    if (current_token->type == TokenType::가져오기) {
        return parseImportStatement();
    }
    // 빈 줄은 무시
    if (current_token->type == TokenType::NEW_LINE) {
        skipToken(TokenType::NEW_LINE);
        return nullptr;
    }
    return parseExpressionStatement();
}

// 정수 x = 10
shared_ptr<InitializationStatement> Parser::parseInitializationStatement() {
    auto statement = make_shared<InitializationStatement>();

    statement->type = current_token;
    setNextToken();

    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);
    skipToken(TokenType::ASSIGN);
    statement->value = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::NEW_LINE);
    return statement;
}

shared_ptr<AssignmentStatement> Parser::parseAssignmentStatement() {
    auto statement = make_shared<AssignmentStatement>();

    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    skipToken(TokenType::ASSIGN);

    statement->value = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::NEW_LINE);
    return statement;
}

shared_ptr<CompoundAssignmentStatement> Parser::parseCompoundAssignmentStatement() {
    auto statement = make_shared<CompoundAssignmentStatement>();

    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    statement->op = current_token;
    setNextToken();

    statement->value = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::NEW_LINE);
    return statement;
}

shared_ptr<ExpressionStatement> Parser::parseExpressionStatement() {
    auto expressionStatement       = make_shared<ExpressionStatement>();
    expressionStatement->expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::NEW_LINE);
    return expressionStatement;
}


shared_ptr<ReturnStatement> Parser::parseReturnStatement() {
    skipToken(TokenType::리턴);
    auto statement       = make_shared<ReturnStatement>();
    statement->expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::NEW_LINE);
    return statement;
}

shared_ptr<BlockStatement> Parser::parseBlockStatement() {
    auto statement = make_shared<BlockStatement>();
    skipToken(TokenType::START_BLOCK);

    while (current_token->type != TokenType::END_BLOCK) {
        auto stmt = parseStatement();
        if (stmt != nullptr) {
            statement->statements.push_back(stmt);
        }
    }
    skipToken(TokenType::END_BLOCK);

    return statement;
}

// 만약 조건 라면:
shared_ptr<IfStatement> Parser::parseIfStatement() {
    auto statement = make_shared<IfStatement>();
    skipToken(TokenType::만약);
    statement->condition = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::라면);
    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    statement->consequence = parseBlockStatement();

    if (current_token != nullptr && current_token->type == TokenType::아니면) {
        skipToken(TokenType::아니면);
        skipToken(TokenType::COLON);
        skipToken(TokenType::NEW_LINE);
        statement->then = parseBlockStatement();
    }

    return statement;
}

// 반복 조건 동안:
shared_ptr<WhileStatement> Parser::parseWhileStatement() {
    auto statement = make_shared<WhileStatement>();
    skipToken(TokenType::반복);
    statement->condition = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::동안);
    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    statement->body = parseBlockStatement();

    return statement;
}

// 각각 정수 원소 배열 에서:
shared_ptr<ForEachStatement> Parser::parseForEachStatement() {
    auto statement = make_shared<ForEachStatement>();
    skipToken(TokenType::각각);

    // 타입 원소명
    statement->elementType = current_token;
    setNextToken();

    checkToken(TokenType::IDENTIFIER);
    statement->elementName = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    // 반복 대상 표현식
    statement->iterable = parseExpression(Precedence::LOWEST);
    setNextToken();

    skipToken(TokenType::에서);
    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    statement->body = parseBlockStatement();

    return statement;
}

// 시도:
//     ...
// 실패 오류:
//     ...
shared_ptr<TryCatchStatement> Parser::parseTryCatchStatement() {
    auto statement = make_shared<TryCatchStatement>();
    skipToken(TokenType::시도);
    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    statement->tryBody = parseBlockStatement();

    skipToken(TokenType::실패);
    checkToken(TokenType::IDENTIFIER);
    statement->errorName = current_token->text;
    skipToken(TokenType::IDENTIFIER);
    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    statement->catchBody = parseBlockStatement();

    return statement;
}

// 함수 이름(타입 파라미터, 타입 파라미터2) -> 리턴타입:
shared_ptr<FunctionStatement> Parser::parseFunctionStatement() {
    auto statement = make_shared<FunctionStatement>();
    skipToken(TokenType::함수);

    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    skipToken(TokenType::LPAREN);

    // 파라미터 파싱
    if (current_token->type != TokenType::RPAREN) {
        do {
            // 타입 키워드
            statement->parameterTypes.push_back(current_token);
            setNextToken();

            // 파라미터 이름
            checkToken(TokenType::IDENTIFIER);
            auto ident = make_shared<IdentifierExpression>();
            ident->name = current_token->text;
            statement->parameters.push_back(ident);
            skipToken(TokenType::IDENTIFIER);
        } while (current_token->type == TokenType::COMMA && (skipToken(TokenType::COMMA), true));
    }
    skipToken(TokenType::RPAREN);

    // 리턴 타입 (선택)
    if (current_token->type == TokenType::RIGHT_ARROW) {
        skipToken(TokenType::RIGHT_ARROW);
        statement->returnType = current_token;
        setNextToken();
    }

    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    statement->body = parseBlockStatement();

    return statement;
}

shared_ptr<Expression> Parser::parseExpression(Precedence precedence) {
    if (!prefixParseFunctions.contains(current_token->type)) {
        throw UnknownPrefixParseFunctionException(current_token->type, current_token->line);
    }

    PrefixParseFunction prefixParseFunction = prefixParseFunctions[current_token->type];
    shared_ptr<Expression> expression       = (this->*prefixParseFunction)();

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

shared_ptr<Expression> Parser::parseInfixExpression(shared_ptr<Expression> left) {
    auto infixExpression   = make_shared<InfixExpression>();
    infixExpression->token = current_token;
    infixExpression->left  = std::move(left);

    Precedence precedence = getPrecedence[current_token->type];
    setNextToken();

    infixExpression->right = parseExpression(precedence);

    return infixExpression;
}

shared_ptr<Expression> Parser::parsePrefixExpression() {
    auto prefixExpression   = make_shared<PrefixExpression>();
    prefixExpression->token = current_token;
    setNextToken();

    prefixExpression->right = parseExpression(Precedence::PREFIX);

    return prefixExpression;
}


shared_ptr<Expression> Parser::parseGroupedExpression() {
    skipToken(TokenType::LPAREN);
    auto expression = parseExpression(Precedence::LOWEST);
    setNextToken();
    checkToken(TokenType::RPAREN);
    return expression;
}

shared_ptr<Expression> Parser::parseIdentifierExpression() {
    checkToken(TokenType::IDENTIFIER);

    auto identifier_expression  = make_shared<IdentifierExpression>();
    identifier_expression->name = current_token->text;
    return identifier_expression;
}

// 함수명(인자1, 인자2) - LPAREN이 infix로 동작
shared_ptr<Expression> Parser::parseCallInfixExpression(shared_ptr<Expression> left) {
    auto call_expression = make_shared<CallExpression>();
    call_expression->function = std::move(left);

    skipToken(TokenType::LPAREN);
    if (current_token != nullptr && current_token->type != TokenType::RPAREN) {
        do {
            call_expression->arguments.push_back(parseExpression(Precedence::LOWEST));
            setNextToken();
        } while (current_token->type == TokenType::COMMA && (skipToken(TokenType::COMMA), true));
    }
    checkToken(TokenType::RPAREN);

    return call_expression;
}

shared_ptr<Expression> Parser::parseIndexExpression(shared_ptr<Expression> left) {
    auto index_expression  = make_shared<IndexExpression>();
    index_expression->name = std::move(left);
    skipToken(TokenType::LBRACKET);
    index_expression->index = parseExpression(Precedence::LOWEST);
    setNextToken();
    checkToken(TokenType::RBRACKET);

    return index_expression;
}


shared_ptr<Expression> Parser::parseIntegerLiteral() {
    auto integerLiteral  = make_shared<IntegerLiteral>();
    integerLiteral->token = current_token;
    try {
        integerLiteral->value = stoll(current_token->text);
    } catch (const std::exception&) {
        throw runtime_error("정수 리터럴 파싱 실패: " + current_token->text);
    }
    return integerLiteral;
}

shared_ptr<Expression> Parser::parseFloatLiteral() {
    auto floatLiteral  = make_shared<FloatLiteral>();
    floatLiteral->token = current_token;
    try {
        floatLiteral->value = stod(current_token->text);
    } catch (const std::exception&) {
        throw runtime_error("실수 리터럴 파싱 실패: " + current_token->text);
    }
    return floatLiteral;
}

shared_ptr<Expression> Parser::parseBooleanLiteral() {
    auto booleanLiteral  = make_shared<BooleanLiteral>();
    booleanLiteral->token = current_token;
    booleanLiteral->value = current_token->type == TokenType::TRUE;
    return booleanLiteral;
}

shared_ptr<Expression> Parser::parseStringLiteral() {
    auto stringLiteral  = make_shared<StringLiteral>();
    stringLiteral->token = current_token;
    stringLiteral->value = current_token->text;
    return stringLiteral;
}

shared_ptr<Expression> Parser::parseArrayLiteral() {
    auto arrayLiteral = make_shared<ArrayLiteral>();

    skipToken(TokenType::LBRACKET);
    if (current_token != nullptr && current_token->type != TokenType::RBRACKET) {
        do {
            auto element = parseExpression(Precedence::LOWEST);
            arrayLiteral->elements.push_back(element);
            setNextToken();
        } while (current_token != nullptr && current_token->type == TokenType::COMMA && (skipToken(TokenType::COMMA), true));
    }

    return arrayLiteral;
}

shared_ptr<Expression> Parser::parseHashMapLiteral() {
    auto hashMapLiteral = make_shared<HashMapLiteral>();

    skipToken(TokenType::LBRACE);
    if (current_token != nullptr && current_token->type != TokenType::RBRACE) {
        do {
            auto key = parseExpression(Precedence::LOWEST);
            hashMapLiteral->keys.push_back(key);
            setNextToken();
            skipToken(TokenType::COLON);
            auto value = parseExpression(Precedence::LOWEST);
            hashMapLiteral->values.push_back(value);
            setNextToken();
        } while (current_token != nullptr && current_token->type == TokenType::COMMA && (skipToken(TokenType::COMMA), true));
    }
    checkToken(TokenType::RBRACE);

    return hashMapLiteral;
}

// 가져오기 "파일명.hik"
shared_ptr<ImportStatement> Parser::parseImportStatement() {
    auto statement = make_shared<ImportStatement>();
    skipToken(TokenType::가져오기);
    checkToken(TokenType::STRING);
    statement->filename = current_token->text;
    skipToken(TokenType::STRING);
    skipToken(TokenType::NEW_LINE);
    return statement;
}
