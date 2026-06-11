#ifndef REPL_H
#define REPL_H

#include "../analyzer/type_checker.h"
#include "../evaluator/evaluator.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../vm/compiler.h"
#include "../vm/vm.h"
#include <memory>

class Repl {
public:
    // 기본값 true: REPL은 VM 백엔드로 실행된다. 트리워킹 평가기는 명시적 false 전달 시에만.
    Repl(bool useVM = true, TypeCheckMode typeCheckMode = TypeCheckMode::Warn);

    void Run();

    // strict 모드에서 타입 진단 발생 시 1, 그 외 0 반환.
    int FileMode(const std::string& filename);

    void TestLexer();

    void TestParser();

private:
    static constexpr const char* EXIT_COMMAND = "종료하기";

    bool useVM;
    TypeCheckMode typeCheckMode;
    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<Parser> parser;
    std::unique_ptr<Evaluator> evaluator;
    std::unique_ptr<TypeChecker> typeChecker;  // REPL 세션 동안 단일 인스턴스 (spec 2.2)
};


#endif // REPL_H
