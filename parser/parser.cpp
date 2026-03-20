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
        try {
            auto statement = parseStatement();
            if (statement != nullptr) {
                program->statements.push_back(statement);
            }
        } catch (const std::exception& e) {
            errors.push_back(e.what());
            skipToNextLine();
        }
    }

    return program;
}


void Parser::initialization() {
    program               = make_shared<Program>();
    current_read_position = 0;
    errors.clear();
    setToken();
}

void Parser::skipToNextLine() {
    while (current_read_position < static_cast<long long>(tokens.size())
           && current_token != nullptr
           && current_token->type != TokenType::NEW_LINE
           && current_token->type != TokenType::END_OF_FILE) {
        setNextToken();
    }
    if (current_token != nullptr && current_token->type == TokenType::NEW_LINE) {
        setNextToken();
    }
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
    // 클래스 타입 변수 선언: 클래스명 변수명 = 값
    if (current_token->type == TokenType::IDENTIFIER && next_token != nullptr
        && next_token->type == TokenType::IDENTIFIER && next_next_token != nullptr
        && next_next_token->type == TokenType::ASSIGN) {
        return parseInitializationStatement();
    }
    // 멤버 대입: 자기.필드 = 값
    if (current_token->type == TokenType::자기 && next_token != nullptr
        && next_token->type == TokenType::DOT) {
        // 멤버 대입인지 확인: 자기.필드 = 값 또는 자기.필드 += 값
        // 이 경우는 expression statement로 처리하되, 특별한 대입 처리는 evaluator에서 함
        // 여기서는 parseExpressionStatement로 넘김
        // 하지만 대입문인지 확인 필요
        // tokens[pos]=자기, pos+1=DOT, pos+2=IDENTIFIER, pos+3=ASSIGN?
        auto pos = current_read_position;
        if (pos + 3 < static_cast<long long>(tokens.size())
            && tokens[pos + 1]->type == TokenType::DOT
            && tokens[pos + 2]->type == TokenType::IDENTIFIER
            && tokens[pos + 3]->type == TokenType::ASSIGN) {
            // 멤버 대입문: 자기.필드 = 값
            auto stmt = make_shared<AssignmentStatement>();
            skipToken(TokenType::자기);
            skipToken(TokenType::DOT);
            stmt->name = "자기." + current_token->text;
            skipToken(TokenType::IDENTIFIER);
            skipToken(TokenType::ASSIGN);
            stmt->value = parseExpression(Precedence::LOWEST);
            setNextToken();
            skipToken(TokenType::NEW_LINE);
            return stmt;
        }
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
        // 반복 정수 i = 0 부터 10 까지: (for-range) vs 반복 조건 동안: (while)
        // Detect: 반복 TYPE IDENT = ...
        if (next_token != nullptr && isTypeKeyword(next_token->type)) {
            return parseForRangeStatement();
        }
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
    if (current_token->type == TokenType::계속) {
        skipToken(TokenType::계속);
        skipToken(TokenType::NEW_LINE);
        return make_shared<ContinueStatement>();
    }
    if (current_token->type == TokenType::함수 && next_token != nullptr
        && next_token->type == TokenType::IDENTIFIER) {
        return parseFunctionStatement();
    }
    if (current_token->type == TokenType::가져오기) {
        return parseImportStatement();
    }
    if (current_token->type == TokenType::비교) {
        return parseMatchStatement();
    }
    if (current_token->type == TokenType::클래스) {
        return parseClassStatement();
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

    while (current_token != nullptr && current_token->type != TokenType::END_BLOCK) {
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

            // 기본값 (선택): = 표현식
            if (current_token != nullptr && current_token->type == TokenType::ASSIGN) {
                skipToken(TokenType::ASSIGN);
                statement->defaultValues.push_back(parseExpression(Precedence::LOWEST));
                setNextToken();
            } else {
                statement->defaultValues.push_back(nullptr);
            }
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

// 비교 표현식:
//     경우 값1:
//         ...
//     경우 값2:
//         ...
//     기본:
//         ...
shared_ptr<MatchStatement> Parser::parseMatchStatement() {
    auto statement = make_shared<MatchStatement>();
    skipToken(TokenType::비교);
    statement->subject = parseExpression(Precedence::LOWEST);
    setNextToken();
    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    skipToken(TokenType::START_BLOCK);

    while (current_token != nullptr && current_token->type != TokenType::END_BLOCK) {
        if (current_token->type == TokenType::경우) {
            skipToken(TokenType::경우);
            auto caseValue = parseExpression(Precedence::LOWEST);
            statement->caseValues.push_back(caseValue);
            setNextToken();
            skipToken(TokenType::COLON);
            skipToken(TokenType::NEW_LINE);
            statement->caseBodies.push_back(parseBlockStatement());
        } else if (current_token->type == TokenType::기본) {
            skipToken(TokenType::기본);
            skipToken(TokenType::COLON);
            skipToken(TokenType::NEW_LINE);
            statement->defaultBody = parseBlockStatement();
        } else if (current_token->type == TokenType::NEW_LINE) {
            skipToken(TokenType::NEW_LINE);
        } else {
            break;
        }
    }

    skipToken(TokenType::END_BLOCK);
    return statement;
}

// 클래스 이름:
//     타입 필드명
//     생성(타입 파라미터):
//         자기.필드 = 값
//     함수 메서드(타입 파라미터) -> 리턴타입:
//         ...
shared_ptr<ClassStatement> Parser::parseClassStatement() {
    auto statement = make_shared<ClassStatement>();
    skipToken(TokenType::클래스);

    checkToken(TokenType::IDENTIFIER);
    statement->name = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    // 상속: 클래스 자식 < 부모:
    if (current_token != nullptr && current_token->type == TokenType::LESS_THAN) {
        skipToken(TokenType::LESS_THAN);
        checkToken(TokenType::IDENTIFIER);
        statement->parentName = current_token->text;
        skipToken(TokenType::IDENTIFIER);
    }

    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    skipToken(TokenType::START_BLOCK);

    while (current_token != nullptr && current_token->type != TokenType::END_BLOCK) {
        if (current_token->type == TokenType::NEW_LINE) {
            skipToken(TokenType::NEW_LINE);
            continue;
        }

        // 필드 선언: 타입 필드명
        if (isTypeKeyword(current_token->type) && next_token != nullptr
            && next_token->type == TokenType::IDENTIFIER
            && (next_next_token == nullptr || next_next_token->type == TokenType::NEW_LINE)) {
            statement->fieldTypes.push_back(current_token);
            setNextToken();
            statement->fieldNames.push_back(current_token->text);
            skipToken(TokenType::IDENTIFIER);
            skipToken(TokenType::NEW_LINE);
            continue;
        }

        // 생성자: 생성(타입 파라미터):
        if (current_token->type == TokenType::생성) {
            skipToken(TokenType::생성);
            skipToken(TokenType::LPAREN);
            if (current_token->type != TokenType::RPAREN) {
                do {
                    statement->constructorParamTypes.push_back(current_token);
                    setNextToken();
                    checkToken(TokenType::IDENTIFIER);
                    auto ident = make_shared<IdentifierExpression>();
                    ident->name = current_token->text;
                    statement->constructorParams.push_back(ident);
                    skipToken(TokenType::IDENTIFIER);
                } while (current_token->type == TokenType::COMMA && (skipToken(TokenType::COMMA), true));
            }
            skipToken(TokenType::RPAREN);
            skipToken(TokenType::COLON);
            skipToken(TokenType::NEW_LINE);
            statement->constructorBody = parseBlockStatement();
            continue;
        }

        // 메서드: 함수 이름(타입 파라미터) -> 리턴타입:
        if (current_token->type == TokenType::함수) {
            statement->methods.push_back(parseFunctionStatement());
            continue;
        }

        break;
    }

    skipToken(TokenType::END_BLOCK);
    return statement;
}

shared_ptr<Expression> Parser::parseExpression(Precedence precedence) {
    if (!prefixParseFunctions.contains(current_token->type)) {
        throw UnknownPrefixParseFunctionException(current_token->type, current_token->line);
    }

    PrefixParseFunction prefixParseFunction = prefixParseFunctions[current_token->type];
    shared_ptr<Expression> expression       = (this->*prefixParseFunction)();

    while (next_token != nullptr) {
        auto precIt = getPrecedence.find(next_token->type);
        Precedence nextPrec = (precIt != getPrecedence.end()) ? precIt->second : Precedence::LOWEST;
        if (precedence >= nextPrec) break;
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
    // ** (거듭제곱)은 우결합성: 2 ** 3 ** 2 → 2 ** (3 ** 2)
    if (current_token->type == TokenType::POWER) {
        precedence = static_cast<Precedence>(static_cast<int>(precedence) - 1);
    }
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

    // 튜플 체크: (a, b, c)
    if (next_token != nullptr && next_token->type == TokenType::COMMA) {
        auto tuple = make_shared<TupleLiteral>();
        tuple->elements.push_back(expression);
        setNextToken(); // expression 뒤로
        while (current_token->type == TokenType::COMMA) {
            skipToken(TokenType::COMMA);
            tuple->elements.push_back(parseExpression(Precedence::LOWEST));
            setNextToken();
        }
        checkToken(TokenType::RPAREN);
        return tuple;
    }

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

shared_ptr<Expression> Parser::parseSelfExpression() {
    auto identifier_expression  = make_shared<IdentifierExpression>();
    identifier_expression->name = "자기";
    return identifier_expression;
}

shared_ptr<Expression> Parser::parseParentExpression() {
    auto identifier_expression  = make_shared<IdentifierExpression>();
    identifier_expression->name = "부모";
    return identifier_expression;
}

// object.member 또는 object.method(args)
shared_ptr<Expression> Parser::parseMemberAccessExpression(shared_ptr<Expression> left) {
    skipToken(TokenType::DOT);

    checkToken(TokenType::IDENTIFIER);
    string memberName = current_token->text;

    // 메서드 호출 체크: object.method(args)
    if (next_token != nullptr && next_token->type == TokenType::LPAREN) {
        auto methodCall = make_shared<MethodCallExpression>();
        methodCall->object = std::move(left);
        methodCall->method = memberName;
        skipToken(TokenType::IDENTIFIER);
        skipToken(TokenType::LPAREN);
        if (current_token != nullptr && current_token->type != TokenType::RPAREN) {
            do {
                methodCall->arguments.push_back(parseExpression(Precedence::LOWEST));
                setNextToken();
            } while (current_token->type == TokenType::COMMA && (skipToken(TokenType::COMMA), true));
        }
        checkToken(TokenType::RPAREN);
        return methodCall;
    }

    auto memberAccess = make_shared<MemberAccessExpression>();
    memberAccess->object = std::move(left);
    memberAccess->member = memberName;
    return memberAccess;
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

    checkToken(TokenType::RBRACKET);
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

shared_ptr<Expression> Parser::parseNullLiteral() {
    auto nullLiteral = make_shared<NullLiteral>();
    nullLiteral->token = current_token;
    return nullLiteral;
}

// 람다: 함수(타입 이름, ...) 표현식
shared_ptr<Expression> Parser::parseLambdaExpression() {
    skipToken(TokenType::함수);
    skipToken(TokenType::LPAREN);

    auto lambda = make_shared<LambdaExpression>();

    // 파라미터 파싱
    if (current_token->type != TokenType::RPAREN) {
        do {
            lambda->parameterTypes.push_back(current_token);
            setNextToken();

            checkToken(TokenType::IDENTIFIER);
            auto ident = make_shared<IdentifierExpression>();
            ident->name = current_token->text;
            lambda->parameters.push_back(ident);
            skipToken(TokenType::IDENTIFIER);
        } while (current_token->type == TokenType::COMMA && (skipToken(TokenType::COMMA), true));
    }
    skipToken(TokenType::RPAREN);

    // 리턴 타입 (선택)
    if (current_token != nullptr && current_token->type == TokenType::RIGHT_ARROW) {
        skipToken(TokenType::RIGHT_ARROW);
        lambda->returnType = current_token;
        setNextToken();
    }

    // 본문: 단일 표현식
    lambda->body = parseExpression(Precedence::LOWEST);

    return lambda;
}

// 반복 정수 i = 0 부터 10 까지:
shared_ptr<ForRangeStatement> Parser::parseForRangeStatement() {
    auto statement = make_shared<ForRangeStatement>();
    skipToken(TokenType::반복);

    statement->varType = current_token;
    setNextToken();

    checkToken(TokenType::IDENTIFIER);
    statement->varName = current_token->text;
    skipToken(TokenType::IDENTIFIER);

    skipToken(TokenType::ASSIGN);

    statement->startExpr = parseExpression(Precedence::LOWEST);
    setNextToken();

    skipToken(TokenType::부터);

    statement->endExpr = parseExpression(Precedence::LOWEST);
    setNextToken();

    skipToken(TokenType::까지);
    skipToken(TokenType::COLON);
    skipToken(TokenType::NEW_LINE);

    statement->body = parseBlockStatement();

    return statement;
}

// 후위 표현식: 변수++ 또는 변수--
shared_ptr<Expression> Parser::parsePostfixExpression(shared_ptr<Expression> left) {
    auto postfix = make_shared<PostfixExpression>();
    postfix->token = current_token;
    postfix->left = std::move(left);
    return postfix;
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
