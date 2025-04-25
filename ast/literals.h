#ifndef LITERALS_H
#define LITERALS_H

#include "expressions.h"

class Literal : public Expression {
};

class IntegerLiteral : public Literal {
public:
    Token *token;
    long long value;

    std::string String() override {
        return token->text;
    }
};

class BooleanLiteral : public Literal {
public:
    Token *token;
    bool value;

    std::string String() override {
        return token->text;
    }
};

class StringLiteral : public Literal {
public:
    Token *token;
    std::string value;

    std::string String() override {
        return token->text;
    }
};

class ArrayLiteral : public Literal {
public:
    Token *token;
    std::vector<Expression *> elements;

    std::string String() override {
        std::string s;
        s += "[";
        for (int i = 0; i < elements.size(); i++) {
            s += elements[i]->String();
            s += ", ";
        }
        s += "]";
        return s;
    }
};

#endif //LITERALS_H
