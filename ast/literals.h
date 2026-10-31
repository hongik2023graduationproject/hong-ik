#ifndef LITERALS_H
#define LITERALS_H

#include "expressions.h"

class Literal : public Expression {};

class IntegerLiteral : public Literal {
public:
    std::shared_ptr<Token> token;
    long long value;

    IntegerLiteral() = default;
    IntegerLiteral(std::shared_ptr<Token> token, long long value) : token(std::move(token)), value(value) {}

    std::string String() override {
        return token->text;
    }
};

class FloatLiteral : public Literal {
public:
    std::shared_ptr<Token> token;
    double value;

    FloatLiteral() = default;
    FloatLiteral(std::shared_ptr<Token> token, double value) : token(std::move(token)), value(value) {}

    std::string String() override {
        return token->text;
    }
};

class BooleanLiteral : public Literal {
public:
    std::shared_ptr<Token> token;
    bool value;

    std::string String() override {
        return token->text;
    }
};

class StringLiteral : public Literal {
public:
    std::shared_ptr<Token> token;
    std::string value;

    std::string String() override {
        return token->text;
    }
};

// 보간이 든 문자열 리터럴 "글자{식}글자{식}글자". 파서가 만든다 (Parser::parseStringLiteral).
// literals.size() == expressions.size() + 1 이고, 값은 literals[0] + 문자열(expressions[0]) + literals[1] + ...
// literals의 각 조각은 이미 이스케이프가 풀린 글자 그대로다 ({{ → {, }} → }).
// 보간이 하나도 없는 문자열은 StringLiteral로 남는다.
class InterpolatedStringLiteral : public Literal {
public:
    std::shared_ptr<Token> token;
    std::vector<std::string> literals;
    std::vector<std::shared_ptr<Expression>> expressions;

    std::string String() override {
        std::string s = "\"";
        for (size_t i = 0; i < literals.size(); i++) {
            for (char c : literals[i]) {
                if (c == '{' || c == '}') {
                    s += c; // 다시 파싱해도 같은 값이 되도록 {{ }}로 적는다
                }
                s += c;
            }
            if (i < expressions.size()) {
                s += "{" + expressions[i]->String() + "}";
            }
        }
        return s + "\"";
    }
};

class ArrayLiteral : public Literal {
public:
    std::shared_ptr<Token> token;
    std::vector<std::shared_ptr<Expression>> elements;

    std::string String() override {
        std::string s;
        s += "[";
        for (size_t i = 0; i < elements.size(); i++) {
            if (i > 0) s += ", ";
            s += elements[i]->String();
        }
        s += "]";
        return s;
    }
};

class NullLiteral : public Literal {
public:
    std::shared_ptr<Token> token;

    std::string String() override {
        return "없음";
    }
};

class TupleLiteral : public Literal {
public:
    std::vector<std::shared_ptr<Expression>> elements;

    std::string String() override {
        std::string s = "(";
        for (size_t i = 0; i < elements.size(); i++) {
            if (i > 0) s += ", ";
            s += elements[i]->String();
        }
        s += ")";
        return s;
    }
};

class HashMapLiteral : public Literal {
public:
    std::vector<std::shared_ptr<Expression>> keys;
    std::vector<std::shared_ptr<Expression>> values;

    std::string String() override {
        std::string s = "{";
        for (size_t i = 0; i < keys.size(); i++) {
            if (i > 0) s += ", ";
            s += keys[i]->String() + ": " + values[i]->String();
        }
        s += "}";
        return s;
    }
};

#endif // LITERALS_H
