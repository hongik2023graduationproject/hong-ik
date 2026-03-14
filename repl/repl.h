#ifndef REPL_H
#define REPL_H

#include "../evaluator/evaluator.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include <memory>

class Repl {
public:
    Repl();

    void Run();

    void FileMode(const std::string& filename);

    void TestLexer();

    void TestParser();

private:
    static constexpr const char* EXIT_COMMAND = "종료하기";

    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<Parser> parser;
    std::unique_ptr<Evaluator> evaluator;
};


#endif // REPL_H
