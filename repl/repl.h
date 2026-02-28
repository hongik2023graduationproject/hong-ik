#ifndef REPL_H
#define REPL_H

#include "../evaluator/evaluator.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"

class Repl {
public:
    Repl();
    ~Repl();

    void Run();

    void FileMode(const std::string& filename);

    void TestLexer();

    void TestParser();

private:
    static constexpr const char* EXIT_COMMAND = "종료하기";

    Lexer* lexer;
    Parser* parser;
    Evaluator* evaluator;
};


#endif // REPL_H
