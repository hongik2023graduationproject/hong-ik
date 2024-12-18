#ifndef REPL_H
#define REPL_H

#include "../lexer/lexer.h"

class Repl {
public:
    Repl();
    void Run();

private:
    Lexer* lexer;
};



#endif //REPL_H
