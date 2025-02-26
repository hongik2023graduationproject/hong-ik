#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include "node.h"
#include "../token/token.h"

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

class PrefixExpression : public Expression {
public:
    Token *token;
    Expression* right;

    std::string String() override {
        return "(" + token->text + right->String() + ")";;
    }
};

#endif //EXPRESSIONS_H
