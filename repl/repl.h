#ifndef REPL_H
#define REPL_H

#include "../evaluator/evaluator.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../vm/compiler.h"
#include "../vm/vm.h"
#include <memory>

class Repl {
public:
    // 기본값 true: REPL은 VM 백엔드로 실행된다. 트리워킹 평가기는 명시적 false 전달 시에만.
    Repl(bool useVM = true);

    void Run();

    void FileMode(const std::string& filename);

    void TestLexer();

    void TestParser();

private:
    static constexpr const char* EXIT_COMMAND = "종료하기";

    bool useVM;
    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<Parser> parser;
    std::unique_ptr<Evaluator> evaluator;
};


#endif // REPL_H
