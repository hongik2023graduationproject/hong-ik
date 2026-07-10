#include "compiler.h"

#include "../exception/exception.h"
#include "../util/type_utils.h"
#include "peephole.h"
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std;


CompiledFunction& Compiler::chunk() {
    return *current->function;
}


void Compiler::emitConstant(shared_ptr<Object> value, long long line) {
    uint16_t idx = chunk().addConstant(std::move(value));
    chunk().emitOpAndUint16(OpCode::OP_CONSTANT, idx, line);
}


uint16_t Compiler::identifierConstant(const string& name) {
    return chunk().addConstant(make_shared<String>(name));
}


void Compiler::beginScope() {
    current->scopeDepth++;
}


void Compiler::endScope(long long line) {
    current->scopeDepth--;
    while (!current->locals.empty() && current->locals.back().depth > current->scopeDepth) {
        chunk().emitOp(OpCode::OP_POP, line);
        current->locals.pop_back();
    }
}


uint16_t Compiler::declareLocal(const string& name) {
    current->locals.push_back({name, current->scopeDepth, false});
    return static_cast<uint16_t>(current->locals.size() - 1);
}


int Compiler::resolveLocal(CompilerState* state, const string& name) {
    for (int i = static_cast<int>(state->locals.size()) - 1; i >= 0; i--) {
        if (state->locals[i].name == name) {
            return i;
        }
    }
    return -1;
}


uint16_t Compiler::addUpvalue(CompilerState* state, uint16_t index, bool isLocal) {
    // 이미 동일한 업밸류가 있는지 확인
    for (uint16_t i = 0; i < state->upvalues.size(); i++) {
        if (state->upvalues[i].index == index && state->upvalues[i].isLocal == isLocal) {
            return i;
        }
    }
    state->upvalues.push_back({index, isLocal});
    return static_cast<uint16_t>(state->upvalues.size() - 1);
}


int Compiler::resolveUpvalue(CompilerState* state, const string& name) {
    if (state->enclosing == nullptr) {
        return -1;
    }

    // enclosing의 로컬에서 찾기
    int local = resolveLocal(state->enclosing, name);
    if (local != -1) {
        state->enclosing->locals[local].isCaptured = true;
        return addUpvalue(state, static_cast<uint16_t>(local), true);
    }

    // enclosing의 업밸류에서 찾기 (재귀)
    int upvalue = resolveUpvalue(state->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(state, static_cast<uint16_t>(upvalue), false);
    }

    return -1;
}


shared_ptr<CompiledFunction> Compiler::Compile(shared_ptr<Program> program) {
    CompilerState state;
    state.function       = make_shared<CompiledFunction>();
    state.function->name = "";
    current              = &state;

    for (size_t i = 0; i < program->statements.size(); i++) {
        bool isLast = (i == program->statements.size() - 1);
        auto* stmt  = program->statements[i].get();

        // 마지막 문이 ExpressionStatement이면 결과를 스택에 유지 (POP 안 함)
        if (isLast && dynamic_cast<ExpressionStatement*>(stmt)) {
            compileExpression(dynamic_cast<ExpressionStatement*>(stmt)->expression.get());
            chunk().emitOp(OpCode::OP_RETURN, 0);
        } else {
            compileStatement(stmt);
        }
    }

    // 마지막이 ExpressionStatement가 아닌 경우
    if (program->statements.empty() || !dynamic_cast<ExpressionStatement*>(program->statements.back().get())) {
        chunk().emitOp(OpCode::OP_NULL, 0);
        chunk().emitOp(OpCode::OP_RETURN, 0);
    }

    current->function->localCount = static_cast<int>(current->locals.size());
    optimize(*state.function);
    return state.function;
}


void Compiler::optimize(CompiledFunction& fn) {
    peephole::optimizeChunk(fn);
    // 중첩 함수(OP_CLOSURE 상수)와 클래스 생성자/메서드에 재귀 적용
    for (auto& c : fn.constants) {
        if (auto* nested = dynamic_cast<CompiledFunction*>(c.get())) {
            optimize(*nested);
        } else if (auto* ccd = dynamic_cast<CompiledClassDef*>(c.get())) {
            if (ccd->compiledConstructor) {
                optimize(*ccd->compiledConstructor);
            }
            for (auto& [name, method] : ccd->compiledMethods) {
                optimize(*method);
            }
        }
    }
}
