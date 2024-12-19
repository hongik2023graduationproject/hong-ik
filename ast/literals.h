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


#endif //LITERALS_H
