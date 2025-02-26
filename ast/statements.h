#ifndef STATEMENTS_H
#define STATEMENTS_H

#include "node.h"
#include "expressions.h"

class Statement : public Node {
};

class AssignmentStatement : public Statement {
public:
    Token *type; // TODO: ast에서는 Token을 사용하지 않게끔 변경하고 싶다.
    std::string name;
    Expression *value;

    std::string String() override {
        return "[" + type->text + "] " + name + " = " + value->String();
    }
};

class ExpressionStatement : public Statement {
public:
    Expression *expression;

    std::string String() override {
        if (expression == nullptr) {
            return "";
        }
        return expression->String();
    }
};

class ReturnStatement : public Statement {
public:
    Expression *expression;

    std::string String() override {
        return "return " + expression->String();
    }
};

class BlockStatement : public Statement {
public:
    std::vector<Statement *> statements;

    std::string String() override {
        std::string s;
        for (Statement *stm: statements) {
            s += "  " + stm->String() + '\n';
        }
        return s;
    }
};

class IfStatement : public Statement {
public:
    Expression *condition;
    Statement *consequence;

    std::string String() override {
        return "if " + condition->String() + "\n" + consequence->String();
    }
};

#endif //STATEMENTS_H
