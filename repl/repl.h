#ifndef REPL_H
#define REPL_H

#include "../lexer/lexer.h"
#include "../parser/parser.h"

class Repl {
public:
    Repl();

    void Run();

    void TestLexer();

    void TestParser();

private:
    Lexer *lexer;
    parser *parser;
};


#endif //REPL_H
