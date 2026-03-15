#ifndef COMPILER_H
#define COMPILER_H

#include "../ast/program.h"
#include "../ast/statements.h"
#include "../ast/expressions.h"
#include "../ast/literals.h"
#include "chunk.h"
#include "opcode.h"
#include <memory>
#include <string>
#include <vector>

struct Local {
    std::string name;
    int depth;
};

struct LoopContext {
    size_t loopStart;
    std::vector<size_t> breakJumps;
    int scopeDepth;
};

struct CompilerState {
    std::shared_ptr<CompiledFunction> function;
    std::vector<Local> locals;
    int scopeDepth = 0;
    std::vector<LoopContext> loops;
};

class Compiler {
public:
    std::shared_ptr<CompiledFunction> Compile(std::shared_ptr<Program> program);

private:
    CompilerState* current = nullptr;

    void compileNode(Node* node);
    void compileStatement(Statement* stmt);
    void compileExpression(Expression* expr);

    // Statements
    void compileInitialization(InitializationStatement* stmt);
    void compileAssignment(AssignmentStatement* stmt);
    void compileCompoundAssignment(CompoundAssignmentStatement* stmt);
    void compileBlock(BlockStatement* stmt);
    void compileIf(IfStatement* stmt);
    void compileWhile(WhileStatement* stmt);
    void compileForEach(ForEachStatement* stmt);
    void compileReturn(ReturnStatement* stmt);
    void compileFunction(FunctionStatement* stmt);
    void compileClass(ClassStatement* stmt);
    void compileMatch(MatchStatement* stmt);
    void compileTryCatch(TryCatchStatement* stmt);
    void compileImport(ImportStatement* stmt);

    // Expressions
    void compileInfix(InfixExpression* expr);
    void compilePrefix(PrefixExpression* expr);
    void compileIdentifier(IdentifierExpression* expr);
    void compileIdentifier(const std::string& name, long long line);
    void compileCall(CallExpression* expr);
    void compileMemberAccess(MemberAccessExpression* expr);
    void compileMethodCall(MethodCallExpression* expr);
    void compileIndex(IndexExpression* expr);

    // Scope
    void beginScope();
    void endScope(long long line);
    uint16_t declareLocal(const std::string& name);
    int resolveLocal(const std::string& name);
    uint16_t identifierConstant(const std::string& name);

    // Helpers
    CompiledFunction& chunk();
    void emitConstant(std::shared_ptr<Object> value, long long line);
};

#endif // COMPILER_H
