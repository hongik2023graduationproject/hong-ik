#ifndef STATEMENTS_H
#define STATEMENTS_H

#include "expressions.h"
#include "node.h"

class Statement : public Node {};

class InitializationStatement : public Statement {
public:
    Token* type; // TODO: ast에서는 Token을 사용하지 않게끔 변경하고 싶다.
    std::string name;
    Expression* value;

    std::string String() override {
        return "[" + type->text + "] " + name + " = " + value->String();
    }
};

class AssignmentStatement : public Statement {
public:
    Token* type;
    std::string name;
    Expression* value;

    std::string String() override {
        return name + " = " + value->String();
    }
};

class ExpressionStatement : public Statement {
public:
    Expression* expression;

    ExpressionStatement() = default;
    explicit ExpressionStatement(Expression* expression) : expression(expression) {}

    std::string String() override {
        if (expression == nullptr) {
            return "";
        }
        return expression->String();
    }
};

class ReturnStatement : public Statement {
public:
    Expression* expression;

    std::string String() override {
        return "return " + expression->String();
    }
};

class BlockStatement : public Statement {
public:
    std::vector<Statement*> statements;

    std::string String() override {
        std::string s;
        for (Statement* stm : statements) {
            s += "  " + stm->String() + '\n';
        }
        return s;
    }
};

class IfStatement : public Statement {
public:
    Expression* condition;
    BlockStatement* consequence;
    BlockStatement* then;

    std::string String() override {
        std::string s = "만약 " + condition->String() + " 라면\n" + consequence->String();
        if (then != nullptr) {
            s += "아니면\n" + then->String();
        }
        return s;
    }
};

class FunctionStatement : public Statement {
public:
    std::vector<Token*> parameterTypes;
    // TODO: Expression을 Identifier Expression으로 수정해야 한다.
    std::vector<Expression*> parameters;
    std::string name;
    BlockStatement* body;
    Token* returnType;

    std::string String() override {
        std::string str = "함수: ";

        for (int i = 0; i < parameterTypes.size(); i++) {
            str += "[" + parameterTypes[i]->text += "]" + parameters[i]->String() + ", ";
        }
        str += name;

        if (returnType != nullptr) {
            str += " -> ";
            str += "[" + returnType->text + "]";
        }

        str += " {\n";
        str += body->String();
        str += "}\n";
        return str;
    }
};

#endif // STATEMENTS_H
