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

class MemberAccessExpression : public Expression {
public:
    std::shared_ptr<Expression> object;
    std::string member;

    std::string String() override {
        return object->String() + "." + member;
    }
};

class MethodCallExpression : public Expression {
public:
    std::shared_ptr<Expression> object;
    std::string method;
    std::vector<std::shared_ptr<Expression>> arguments;

    std::string String() override {
        std::string s = object->String() + "." + method + "(";
        for (size_t i = 0; i < arguments.size(); i++) {
            if (i > 0) s += ", ";
            s += arguments[i]->String();
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

class LambdaExpression : public Expression {
public:
    std::vector<std::shared_ptr<Token>> parameterTypes;
    std::vector<std::shared_ptr<IdentifierExpression>> parameters;
    std::shared_ptr<Expression> body;
    std::shared_ptr<Token> returnType;

    std::string String() override {
        std::string s = "함수(";
        for (size_t i = 0; i < parameterTypes.size(); i++) {
            if (i > 0) s += ", ";
            s += parameterTypes[i]->text + " " + parameters[i]->name;
        }
        s += ") ";
        if (body) s += body->String();
        return s;
    }
};

#endif // EXPRESSIONS_H
