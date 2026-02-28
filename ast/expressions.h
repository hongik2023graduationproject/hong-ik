#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include "../token/token.h"
#include "node.h"
#include <memory>
#include <vector>

class Expression : public Node {};

class InfixExpression : public Expression {
public:
    std::shared_ptr<Token> token;
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;

    InfixExpression() = default;
    InfixExpression(std::shared_ptr<Token> token, std::shared_ptr<Expression> left, std::shared_ptr<Expression> right)
        : token(std::move(token)), left(std::move(left)), right(std::move(right)) {}

    std::string String() override {
        return "(" + left->String() + " " + token->text + " " + right->String() + ")";
    }
};

class PrefixExpression : public Expression {
public:
    std::shared_ptr<Token> token;
    std::shared_ptr<Expression> right;

    std::string String() override {
        return "(" + token->text + right->String() + ")";
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
    std::shared_ptr<Expression> function;
    std::vector<std::shared_ptr<Expression>> arguments;

    std::string String() override {
        std::string s = ":" + function->String();
        s += " (";
        for (auto& arg : arguments) {
            s += arg->String();
            s += ", ";
        }
        s += ")";
        return s;
    }
};

class IndexExpression : public Expression {
public:
    std::shared_ptr<Expression> name;
    std::shared_ptr<Expression> index;

    std::string String() override {
        return name->String() + "[" + index->String() + "]";
    }
};

#endif // EXPRESSIONS_H
