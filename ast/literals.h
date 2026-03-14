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
