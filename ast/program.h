#ifndef PROGRAM_H
#define PROGRAM_H

#include "node.h"
#include "statements.h"
#include <vector>

class Program : public Node {
public:
    std::vector<Statement*> statements;


    Program() = default;
    explicit Program(std::vector<Statement*> statements) : statements(statements) {}

    std::string String() override {
        std::string s;
        for (auto& statement : statements) {
            s += statement->String() + '\n';
        }
        return s;
    }
};

#endif // PROGRAM_H
