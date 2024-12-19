#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include "node.h"

class Expression : public Node {
};


class InfixExpression : public Expression {
public:
    Token *token;
    Expression *left;
    Expression *right;

    std::string String() override {
        return "(" + left->String() + " " + token->text + " " + right->String() + ")";
    }
};

#endif //EXPRESSIONS_H
