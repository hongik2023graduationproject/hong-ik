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
    bool isCaptured = false; // 클로저에 캡처되었는지
};

struct UpvalueInfo {
    uint16_t index;  // enclosing 함수의 로컬 슬롯 또는 업밸류 슬롯
    bool isLocal;    // true: enclosing의 로컬, false: enclosing의 업밸류
};

struct LoopContext {
    size_t loopStart;
    size_t continueTarget; // for continue: where to jump (may differ from loopStart for for-range)
    std::vector<size_t> breakJumps;
    std::vector<size_t> continueJumps;
    int scopeDepth;
};

struct CompilerState {
    std::shared_ptr<CompiledFunction> function;
    CompilerState* enclosing = nullptr;
    std::vector<Local> locals;
    std::vector<UpvalueInfo> upvalues;
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
    void compileForRange(ForRangeStatement* stmt);
    void compileReturn(ReturnStatement* stmt);
    void compileFunction(FunctionStatement* stmt);
    void compileClass(ClassStatement* stmt);
    void compileMatch(MatchStatement* stmt);
    void compileTryCatch(TryCatchStatement* stmt);
    void compileImport(ImportStatement* stmt);
    void compileIndexAssignment(IndexAssignmentStatement* stmt);
    void compileYield(YieldStatement* stmt);
    bool containsYield(BlockStatement* block);

    // Expressions
    void compileInfix(InfixExpression* expr);
    void compilePrefix(PrefixExpression* expr);
    void compileIdentifier(IdentifierExpression* expr);
    void compileIdentifier(const std::string& name, long long line);
    void compileCall(CallExpression* expr);
    void compileMemberAccess(MemberAccessExpression* expr);
    void compileMethodCall(MethodCallExpression* expr);
    void compileIndex(IndexExpression* expr);
    void compileSlice(SliceExpression* expr);

    // Scope
    void beginScope();
    void endScope(long long line);
    uint16_t declareLocal(const std::string& name);
    int resolveLocal(CompilerState* state, const std::string& name);
    int resolveUpvalue(CompilerState* state, const std::string& name);
    uint16_t addUpvalue(CompilerState* state, uint16_t index, bool isLocal);
    uint16_t identifierConstant(const std::string& name);

    // Optimization
    bool tryConstantFold(InfixExpression* expr);

    // Helpers
    CompiledFunction& chunk();
    void emitConstant(std::shared_ptr<Object> value, long long line);
};

#endif // COMPILER_H
