#ifndef REPL_H
#define REPL_H

#include "../evaluator/evaluator.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"

class Repl {
public:
    Repl();

    void Run();

    void FileMode(std::string& filename);

    void TestLexer();

    void TestParser();

private:
    Lexer* lexer;
    Parser* parser;
    Evaluator* evaluator;
};


#endif // REPL_H
