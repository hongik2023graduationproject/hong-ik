#ifndef PROGRAM_H
#define PROGRAM_H

#include "node.h"
#include "statements.h"
#include <memory>
#include <vector>

class Program : public Node {
public:
    std::vector<std::shared_ptr<Statement>> statements;

    Program() = default;
    explicit Program(std::vector<std::shared_ptr<Statement>> statements) : statements(std::move(statements)) {}

    std::string String() override {
        std::string s;
        for (auto& statement : statements) {
            s += statement->String() + '\n';
        }
        return s;
    }
};

#endif // PROGRAM_H
