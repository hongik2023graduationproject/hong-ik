#ifndef PROGRAM_H
#define PROGRAM_H

#include <vector>

#include "node.h"
#include "statements.h"

class Program : public Node {
public:
    std::vector<Statement *> statements;

    std::string String() override {
        std::string s;
        for (auto &statement: statements) {
            s += statement->String() + '\n';
        }
        return s;
    }
};

#endif //PROGRAM_H
