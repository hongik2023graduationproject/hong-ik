#ifndef SYMBOL_COLLECTOR_H
#define SYMBOL_COLLECTOR_H

#include "../ast/program.h"
#include "../ast/statements.h"
#include <memory>
#include <string>
#include <vector>

enum class DocSymbolKind { Variable, Function, Class, Field, Method, Parameter };

struct SymbolInfo {
    std::string name;
    DocSymbolKind kind;
    std::string typeText;
    std::string container;
    long long line = 0;
    long long column = 0;
    long long endColumn = 0;
};

// AST 1회 순회로 선언 심볼을 수집한다 (LSP hover/completion/definition/documentSymbol 재료).
// hong-ik은 선언 타입 명시 언어라 타입 추론 없이 문법에서 typeText를 얻는다 (spec v1.1 P0-3).
class SymbolCollector {
public:
    std::vector<SymbolInfo> collect(const std::shared_ptr<Program>& program);

private:
    void walkStatement(const std::shared_ptr<Statement>& stmt, const std::string& container,
                       std::vector<SymbolInfo>& out);
    void walkBlock(const std::shared_ptr<BlockStatement>& block, const std::string& container,
                   std::vector<SymbolInfo>& out);
    void addFunction(const std::shared_ptr<FunctionStatement>& fn, const std::string& container,
                     DocSymbolKind kind, std::vector<SymbolInfo>& out);
};

#endif // SYMBOL_COLLECTOR_H
