#ifndef STATEMENTS_H
#define STATEMENTS_H

#include "expressions.h"
#include "node.h"
#include <memory>

class Statement : public Node {};

class ImportStatement : public Statement {
public:
    std::string filename;

    std::string String() override {
        return "가져오기 \"" + filename + "\"";
    }
};

class InitializationStatement : public Statement {
public:
    std::shared_ptr<Token> type;
    std::string name;
    std::shared_ptr<Expression> value;

    std::string String() override {
        return "[" + type->text + "] " + name + " = " + value->String();
    }
};

class AssignmentStatement : public Statement {
public:
    std::string name;
    std::shared_ptr<Expression> value;

    std::string String() override {
        return name + " = " + value->String();
    }
};

class ExpressionStatement : public Statement {
public:
    std::shared_ptr<Expression> expression;

    ExpressionStatement() = default;
    explicit ExpressionStatement(std::shared_ptr<Expression> expression) : expression(std::move(expression)) {}

    std::string String() override {
        if (expression == nullptr) {
            return "";
        }
        return expression->String();
    }
};

class ReturnStatement : public Statement {
public:
    std::shared_ptr<Expression> expression;

    std::string String() override {
        return "return " + expression->String();
    }
};

class BlockStatement : public Statement {
public:
    std::vector<std::shared_ptr<Statement>> statements;

    std::string String() override {
        std::string s;
        for (auto& stm : statements) {
            s += "  " + stm->String() + '\n';
        }
        return s;
    }
};

class IfStatement : public Statement {
public:
    std::shared_ptr<Expression> condition;
    std::shared_ptr<BlockStatement> consequence;
    std::shared_ptr<BlockStatement> then;

    std::string String() override {
        std::string s = "만약 " + condition->String() + " 라면\n" + consequence->String();
        if (then != nullptr) {
            s += "아니면\n" + then->String();
        }
        return s;
    }
};

class WhileStatement : public Statement {
public:
    std::shared_ptr<Expression> condition;
    std::shared_ptr<BlockStatement> body;

    std::string String() override {
        std::string s = "반복 " + condition->String() + " 동안\n" + body->String();
        return s;
    }
};

class BreakStatement : public Statement {
public:
    std::string String() override {
        return "중단";
    }
};

class CompoundAssignmentStatement : public Statement {
public:
    std::string name;
    std::shared_ptr<Token> op;
    std::shared_ptr<Expression> value;

    std::string String() override {
        return name + " " + op->text + " " + value->String();
    }
};

class ForEachStatement : public Statement {
public:
    std::shared_ptr<Token> elementType;
    std::string elementName;
    std::shared_ptr<Expression> iterable;
    std::shared_ptr<BlockStatement> body;

    std::string String() override {
        std::string s = "각각 [" + elementType->text + "]" + elementName + " ";
        s += iterable->String() + " 에서\n" + body->String();
        return s;
    }
};

class TryCatchStatement : public Statement {
public:
    std::shared_ptr<BlockStatement> tryBody;
    std::string errorName;
    std::shared_ptr<BlockStatement> catchBody;

    std::string String() override {
        std::string s = "시도\n" + tryBody->String();
        s += "실패 " + errorName + "\n" + catchBody->String();
        return s;
    }
};

class FunctionStatement : public Statement {
public:
    std::vector<std::shared_ptr<Token>> parameterTypes;
    std::vector<std::shared_ptr<IdentifierExpression>> parameters;
    std::string name;
    std::shared_ptr<BlockStatement> body;
    std::shared_ptr<Token> returnType;

    std::string String() override {
        std::string str = "함수: ";

        for (size_t i = 0; i < parameterTypes.size(); i++) {
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
