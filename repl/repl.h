#ifndef REPL_H
#define REPL_H

#include "../lexer/lexer.h"
#include "../parser/Parser.h"

class Repl {
public:
    Repl();

    void Run();

    void TestLexer();

    void TestParser();

private:
    Lexer *lexer;
    Parser *parser;
};


#endif //REPL_H
