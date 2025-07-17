#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include "../token/token.h"
#include "node.h"
#include <vector>

class Expression : public Node {};

class InfixExpression : public Expression {
public:
    Token* token;
    Expression* left;
    Expression* right;

    InfixExpression() = default;
    InfixExpression(Token* token, Expression* left, Expression* right) : token(token), left(left), right(right) {}

    std::string String() override {
        return "(" + left->String() + " " + token->text + " " + right->String() + ")";
    }
};

class PrefixExpression : public Expression {
public:
    Token* token;
    Expression* right;

    std::string String() override {
        return "(" + token->text + right->String() + ")";
        ;
    }
};

class IdentifierExpression : public Expression {
public:
    std::string name;

    std::string String() override {
        return name;
    }
};

// 함수 호출을 위한 표현식
class CallExpression : public Expression {
public:
    Expression* function;
    std::vector<Expression*> arguments;

    std::string String() override {
        std::string s = ":" + function->String();
        s += " (";
        for (auto arg : arguments) {
            s += arg->String();
            s += ", ";
        }
        s += ")";
        return s;
    }
};

class IndexExpression : public Expression {
public:
    Expression* name;
    Expression* index;

    std::string String() override {
        return name->String() + "[" + index->String() + "]";
    }
};

#endif // EXPRESSIONS_H
