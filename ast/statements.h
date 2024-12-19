#ifndef STATEMENTS_H
#define STATEMENTS_H

#include "node.h"
#include "expressions.h"

class Statement : public Node {
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

#endif //STATEMENTS_H
