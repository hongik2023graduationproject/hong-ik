#include "compiler.h"
#include "../exception/exception.h"
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
    if (state->enclosing == nullptr) return -1;

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
    state.function = make_shared<CompiledFunction>();
    state.function->name = "";
    current = &state;

    for (size_t i = 0; i < program->statements.size(); i++) {
        bool isLast = (i == program->statements.size() - 1);
        auto* stmt = program->statements[i].get();

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
    return state.function;
}

void Compiler::compileStatement(Statement* stmt) {
    if (auto* s = dynamic_cast<ExpressionStatement*>(stmt)) {
        compileExpression(s->expression.get());
        chunk().emitOp(OpCode::OP_POP, 0);
        return;
    }
    if (auto* s = dynamic_cast<InitializationStatement*>(stmt)) {
        compileInitialization(s);
        return;
    }
    if (auto* s = dynamic_cast<AssignmentStatement*>(stmt)) {
        compileAssignment(s);
        return;
    }
    if (auto* s = dynamic_cast<CompoundAssignmentStatement*>(stmt)) {
        compileCompoundAssignment(s);
        return;
    }
    if (auto* s = dynamic_cast<ReturnStatement*>(stmt)) {
        compileReturn(s);
        return;
    }
    if (auto* s = dynamic_cast<IfStatement*>(stmt)) {
        compileIf(s);
        return;
    }
    if (auto* s = dynamic_cast<WhileStatement*>(stmt)) {
        compileWhile(s);
        return;
    }
    if (auto* s = dynamic_cast<ForEachStatement*>(stmt)) {
        compileForEach(s);
        return;
    }
    if (auto* s = dynamic_cast<ForRangeStatement*>(stmt)) {
        compileForRange(s);
        return;
    }
    if (dynamic_cast<BreakStatement*>(stmt)) {
        if (!current->loops.empty()) {
            auto& loop = current->loops.back();
            size_t jumpOffset = chunk().emitJump(OpCode::OP_JUMP, 0);
            loop.breakJumps.push_back(jumpOffset);
        }
        return;
    }
    if (dynamic_cast<ContinueStatement*>(stmt)) {
        if (!current->loops.empty()) {
            auto& loop = current->loops.back();
            if (loop.continueTarget != 0) {
                // For for-range loops, we need a forward jump to the increment section
                size_t jumpOffset = chunk().emitJump(OpCode::OP_JUMP, 0);
                loop.continueJumps.push_back(jumpOffset);
            } else {
                // For while/foreach loops, jump back to loop start
                chunk().emitLoop(loop.loopStart, 0);
            }
        }
        return;
    }
    if (auto* s = dynamic_cast<FunctionStatement*>(stmt)) {
        compileFunction(s);
        return;
    }
    if (auto* s = dynamic_cast<ClassStatement*>(stmt)) {
        compileClass(s);
        return;
    }
    if (auto* s = dynamic_cast<MatchStatement*>(stmt)) {
        compileMatch(s);
        return;
    }
    if (auto* s = dynamic_cast<TryCatchStatement*>(stmt)) {
        compileTryCatch(s);
        return;
    }
    if (auto* s = dynamic_cast<ImportStatement*>(stmt)) {
        compileImport(s);
        return;
    }
    if (auto* s = dynamic_cast<IndexAssignmentStatement*>(stmt)) {
        compileIndexAssignment(s);
        return;
    }
    if (auto* s = dynamic_cast<YieldStatement*>(stmt)) {
        compileYield(s);
        return;
    }
    if (auto* s = dynamic_cast<BlockStatement*>(stmt)) {
        compileBlock(s);
        return;
    }
}

void Compiler::compileExpression(Expression* expr) {
    if (auto* e = dynamic_cast<IntegerLiteral*>(expr)) {
        long long line = e->token ? e->token->line : 0;
        emitConstant(make_shared<Integer>(e->value), line);
        return;
    }
    if (auto* e = dynamic_cast<FloatLiteral*>(expr)) {
        long long line = e->token ? e->token->line : 0;
        emitConstant(make_shared<Float>(e->value), line);
        return;
    }
    if (auto* e = dynamic_cast<BooleanLiteral*>(expr)) {
        long long line = e->token ? e->token->line : 0;
        chunk().emitOp(e->value ? OpCode::OP_TRUE : OpCode::OP_FALSE, line);
        return;
    }
    if (auto* e = dynamic_cast<StringLiteral*>(expr)) {
        long long line = e->token ? e->token->line : 0;
        // 문자열 보간 처리
        const string& val = e->value;
        if (val.find('{') != string::npos) {
            // 보간 세그먼트 파싱
            vector<string> segments;
            vector<string> varNames;
            size_t i = 0;
            string current_seg;
            while (i < val.size()) {
                if (val[i] == '{') {
                    size_t end = val.find('}', i);
                    if (end != string::npos) {
                        segments.push_back(current_seg);
                        current_seg.clear();
                        varNames.push_back(val.substr(i + 1, end - i - 1));
                        i = end + 1;
                        continue;
                    }
                }
                current_seg += val[i];
                i++;
            }
            segments.push_back(current_seg);

            // 세그먼트와 변수를 번갈아 push
            int count = 0;
            for (size_t si = 0; si < segments.size(); si++) {
                if (!segments[si].empty()) {
                    emitConstant(make_shared<String>(segments[si]), line);
                    count++;
                }
                if (si < varNames.size()) {
                    // 변수를 문자열로 변환하기 위해 식별자 로드
                    compileIdentifier(varNames[si], line);
                    count++;
                }
            }
            chunk().emitOpAndUint16(OpCode::OP_INTERPOLATE, static_cast<uint16_t>(count), line);
            return;
        }
        emitConstant(make_shared<String>(val), line);
        return;
    }
    if (dynamic_cast<NullLiteral*>(expr)) {
        chunk().emitOp(OpCode::OP_NULL, 0);
        return;
    }
    if (auto* e = dynamic_cast<ArrayLiteral*>(expr)) {
        long long line = e->token ? e->token->line : 0;
        for (auto& elem : e->elements) {
            compileExpression(elem.get());
        }
        chunk().emitOpAndUint16(OpCode::OP_BUILD_ARRAY, static_cast<uint16_t>(e->elements.size()), line);
        return;
    }
    if (auto* e = dynamic_cast<HashMapLiteral*>(expr)) {
        for (size_t i = 0; i < e->keys.size(); i++) {
            compileExpression(e->keys[i].get());
            compileExpression(e->values[i].get());
        }
        chunk().emitOpAndUint16(OpCode::OP_BUILD_HASHMAP, static_cast<uint16_t>(e->keys.size()), 0);
        return;
    }
    if (auto* e = dynamic_cast<TupleLiteral*>(expr)) {
        for (auto& elem : e->elements) {
            compileExpression(elem.get());
        }
        chunk().emitOpAndUint16(OpCode::OP_BUILD_TUPLE, static_cast<uint16_t>(e->elements.size()), 0);
        return;
    }
    if (auto* e = dynamic_cast<InfixExpression*>(expr)) {
        compileInfix(e);
        return;
    }
    if (auto* e = dynamic_cast<PrefixExpression*>(expr)) {
        compilePrefix(e);
        return;
    }
    if (auto* e = dynamic_cast<IdentifierExpression*>(expr)) {
        compileIdentifier(e);
        return;
    }
    if (auto* e = dynamic_cast<CallExpression*>(expr)) {
        compileCall(e);
        return;
    }
    if (auto* e = dynamic_cast<MemberAccessExpression*>(expr)) {
        compileMemberAccess(e);
        return;
    }
    if (auto* e = dynamic_cast<MethodCallExpression*>(expr)) {
        compileMethodCall(e);
        return;
    }
    if (auto* e = dynamic_cast<IndexExpression*>(expr)) {
        compileIndex(e);
        return;
    }
    if (auto* e = dynamic_cast<SliceExpression*>(expr)) {
        compileSlice(e);
        return;
    }
    if (auto* e = dynamic_cast<PostfixExpression*>(expr)) {
        auto* ident = dynamic_cast<IdentifierExpression*>(e->left.get());
        if (!ident) {
            throw RuntimeException("증감 연산자는 변수에만 사용할 수 있습니다.", 0);
        }
        long long line = e->token ? e->token->line : 0;

        // 현재 값 로드 (반환용)
        compileIdentifier(ident->name, line);

        // 현재 값을 다시 로드하고 1을 더하거나 빼서 저장
        compileIdentifier(ident->name, line);
        emitConstant(make_shared<Integer>(1), line);
        if (e->token->type == TokenType::INCREMENT) {
            chunk().emitOp(OpCode::OP_ADD, line);
        } else {
            chunk().emitOp(OpCode::OP_SUB, line);
        }

        // 결과 저장
        int slot = resolveLocal(current, ident->name);
        if (slot != -1) {
            chunk().emitOpAndUint16(OpCode::OP_SET_LOCAL, static_cast<uint16_t>(slot), line);
        } else {
            int uv = resolveUpvalue(current, ident->name);
            if (uv != -1) {
                chunk().emitOpAndUint16(OpCode::OP_SET_UPVALUE, static_cast<uint16_t>(uv), line);
            } else {
                uint16_t nameIdx = identifierConstant(ident->name);
                chunk().emitOpAndUint16(OpCode::OP_SET_GLOBAL, nameIdx, line);
            }
        }
        chunk().emitOp(OpCode::OP_POP, line); // SET이 남긴 새 값 제거 (이전 값이 스택에 남음)
        return;
    }
    if (auto* e = dynamic_cast<LambdaExpression*>(expr)) {
        CompilerState funcState;
        funcState.function = make_shared<CompiledFunction>();
        funcState.function->name = "<람다>";
        funcState.function->arity = static_cast<int>(e->parameters.size());
        funcState.scopeDepth = 1;
        funcState.enclosing = current;

        CompilerState* enclosing = current;
        current = &funcState;

        for (auto& param : e->parameters) {
            declareLocal(param->name);
        }

        // Compile the body expression and return it
        compileExpression(e->body.get());
        chunk().emitOp(OpCode::OP_RETURN, 0);

        current->function->localCount = static_cast<int>(current->locals.size());
        auto upvalues = current->upvalues;
        current = enclosing;

        uint16_t constIdx = chunk().addConstant(funcState.function);
        if (!upvalues.empty()) {
            chunk().emitOpAndUint16(OpCode::OP_CLOSURE, constIdx, 0);
            chunk().emitByte(static_cast<uint8_t>(upvalues.size()), 0);
            for (auto& uv : upvalues) {
                chunk().emitByte(uv.isLocal ? 1 : 0, 0);
                chunk().emitByte(static_cast<uint8_t>((uv.index >> 8) & 0xff), 0);
                chunk().emitByte(static_cast<uint8_t>(uv.index & 0xff), 0);
            }
        } else {
            chunk().emitOpAndUint16(OpCode::OP_CONSTANT, constIdx, 0);
        }
        return;
    }
}

void Compiler::compileIdentifier(IdentifierExpression* expr) {
    compileIdentifier(expr->name, 0);
}

void Compiler::compileIdentifier(const string& name, long long line) {
    int slot = resolveLocal(current, name);
    if (slot != -1) {
        chunk().emitOpAndUint16(OpCode::OP_GET_LOCAL, static_cast<uint16_t>(slot), line);
        return;
    }
    int upvalue = resolveUpvalue(current, name);
    if (upvalue != -1) {
        chunk().emitOpAndUint16(OpCode::OP_GET_UPVALUE, static_cast<uint16_t>(upvalue), line);
        return;
    }
    uint16_t nameIdx = identifierConstant(name);
    chunk().emitOpAndUint16(OpCode::OP_GET_GLOBAL, nameIdx, line);
}

bool Compiler::tryConstantFold(InfixExpression* expr) {
    long long line = expr->token ? expr->token->line : 0;
    TokenType op = expr->token->type;

    // 산술/비교/모듈로/거듭제곱 연산 폴딩 대상
    if (op != TokenType::PLUS && op != TokenType::MINUS &&
        op != TokenType::ASTERISK && op != TokenType::SLASH &&
        op != TokenType::PERCENT && op != TokenType::POWER &&
        op != TokenType::EQUAL && op != TokenType::NOT_EQUAL &&
        op != TokenType::LESS_THAN && op != TokenType::GREATER_THAN &&
        op != TokenType::LESS_EQUAL && op != TokenType::GREATER_EQUAL) {
        return false;
    }

    // 중첩 상수 표현식 처리: 좌/우가 InfixExpression이면 재귀적으로 폴딩 시도
    // 폴딩 가능 여부만 판단하므로, 실제 emit은 하지 않고 값만 추출

    // 좌항 값 추출
    long long leftInt = 0; double leftFloat = 0; string leftStr;
    enum { T_INT, T_FLOAT, T_STR, T_NONE } leftType = T_NONE;

    if (auto* li = dynamic_cast<IntegerLiteral*>(expr->left.get())) {
        leftInt = li->value; leftType = T_INT;
    } else if (auto* lf = dynamic_cast<FloatLiteral*>(expr->left.get())) {
        leftFloat = lf->value; leftType = T_FLOAT;
    } else if (auto* ls = dynamic_cast<StringLiteral*>(expr->left.get())) {
        if (ls->value.find('{') == string::npos) { leftStr = ls->value; leftType = T_STR; }
    } else {
        return false;
    }

    // 우항 값 추출
    long long rightInt = 0; double rightFloat = 0; string rightStr;
    enum { R_INT, R_FLOAT, R_STR, R_NONE } rightType = R_NONE;

    if (auto* ri = dynamic_cast<IntegerLiteral*>(expr->right.get())) {
        rightInt = ri->value; rightType = R_INT;
    } else if (auto* rf = dynamic_cast<FloatLiteral*>(expr->right.get())) {
        rightFloat = rf->value; rightType = R_FLOAT;
    } else if (auto* rs = dynamic_cast<StringLiteral*>(expr->right.get())) {
        if (rs->value.find('{') == string::npos) { rightStr = rs->value; rightType = R_STR; }
    } else {
        return false;
    }

    // 정수 + 정수
    if (leftType == T_INT && rightType == R_INT) {
        // 비교 연산 폴딩
        if (op == TokenType::EQUAL || op == TokenType::NOT_EQUAL ||
            op == TokenType::LESS_THAN || op == TokenType::GREATER_THAN ||
            op == TokenType::LESS_EQUAL || op == TokenType::GREATER_EQUAL) {
            bool cmpResult;
            switch (op) {
            case TokenType::EQUAL: cmpResult = (leftInt == rightInt); break;
            case TokenType::NOT_EQUAL: cmpResult = (leftInt != rightInt); break;
            case TokenType::LESS_THAN: cmpResult = (leftInt < rightInt); break;
            case TokenType::GREATER_THAN: cmpResult = (leftInt > rightInt); break;
            case TokenType::LESS_EQUAL: cmpResult = (leftInt <= rightInt); break;
            case TokenType::GREATER_EQUAL: cmpResult = (leftInt >= rightInt); break;
            default: return false;
            }
            chunk().emitOp(cmpResult ? OpCode::OP_TRUE : OpCode::OP_FALSE, line);
            return true;
        }

        // 0으로 나누기는 폴딩하지 않음
        if ((op == TokenType::SLASH || op == TokenType::PERCENT) && rightInt == 0) return false;
        long long result;
        switch (op) {
        case TokenType::PLUS:
            if ((rightInt > 0 && leftInt > std::numeric_limits<long long>::max() - rightInt) ||
                (rightInt < 0 && leftInt < std::numeric_limits<long long>::min() - rightInt))
                return false;
            result = leftInt + rightInt;
            break;
        case TokenType::MINUS:
            if ((rightInt < 0 && leftInt > std::numeric_limits<long long>::max() + rightInt) ||
                (rightInt > 0 && leftInt < std::numeric_limits<long long>::min() + rightInt))
                return false;
            result = leftInt - rightInt;
            break;
        case TokenType::ASTERISK:
            if (leftInt != 0 && rightInt != 0) {
                if ((leftInt > 0) == (rightInt > 0)) {
                    if (leftInt > 0 ? leftInt > std::numeric_limits<long long>::max() / rightInt
                                    : leftInt < std::numeric_limits<long long>::max() / rightInt)
                        return false;
                } else {
                    if (leftInt > 0 ? rightInt < std::numeric_limits<long long>::min() / leftInt
                                    : leftInt < std::numeric_limits<long long>::min() / rightInt)
                        return false;
                }
            }
            result = leftInt * rightInt;
            break;
        case TokenType::SLASH: result = leftInt / rightInt; break;
        case TokenType::PERCENT: result = leftInt % rightInt; break;
        case TokenType::POWER: {
            // 간단한 거듭제곱 (음수 지수는 폴딩하지 않음)
            if (rightInt < 0) return false;
            long long base = leftInt;
            long long exp = rightInt;
            result = 1;
            for (long long e = 0; e < exp; e++) {
                // 오버플로우 검사
                if (result != 0 && base != 0) {
                    if (std::abs(result) > std::numeric_limits<long long>::max() / std::abs(base))
                        return false;
                }
                result *= base;
            }
            break;
        }
        default: return false;
        }
        emitConstant(make_shared<Integer>(result), line);
        return true;
    }

    // 실수 + 실수
    if (leftType == T_FLOAT && rightType == R_FLOAT) {
        // 비교 연산 폴딩
        if (op == TokenType::EQUAL || op == TokenType::NOT_EQUAL ||
            op == TokenType::LESS_THAN || op == TokenType::GREATER_THAN ||
            op == TokenType::LESS_EQUAL || op == TokenType::GREATER_EQUAL) {
            bool cmpResult;
            switch (op) {
            case TokenType::EQUAL: cmpResult = (leftFloat == rightFloat); break;
            case TokenType::NOT_EQUAL: cmpResult = (leftFloat != rightFloat); break;
            case TokenType::LESS_THAN: cmpResult = (leftFloat < rightFloat); break;
            case TokenType::GREATER_THAN: cmpResult = (leftFloat > rightFloat); break;
            case TokenType::LESS_EQUAL: cmpResult = (leftFloat <= rightFloat); break;
            case TokenType::GREATER_EQUAL: cmpResult = (leftFloat >= rightFloat); break;
            default: return false;
            }
            chunk().emitOp(cmpResult ? OpCode::OP_TRUE : OpCode::OP_FALSE, line);
            return true;
        }

        if (op == TokenType::SLASH && rightFloat == 0.0) return false;
        double result;
        switch (op) {
        case TokenType::PLUS: result = leftFloat + rightFloat; break;
        case TokenType::MINUS: result = leftFloat - rightFloat; break;
        case TokenType::ASTERISK: result = leftFloat * rightFloat; break;
        case TokenType::SLASH: result = leftFloat / rightFloat; break;
        default: return false;
        }
        emitConstant(make_shared<Float>(result), line);
        return true;
    }

    // 문자열 + 문자열 (연결)
    if (leftType == T_STR && rightType == R_STR && op == TokenType::PLUS) {
        emitConstant(make_shared<String>(leftStr + rightStr), line);
        return true;
    }

    return false;
}

void Compiler::compileInfix(InfixExpression* expr) {
    long long line = expr->token ? expr->token->line : 0;

    // 단축 평가(short-circuit): && 는 좌항이 false면 우항을 평가하지 않음
    if (expr->token->type == TokenType::LOGICAL_AND) {
        compileExpression(expr->left.get());
        // 좌항이 false이면 우항을 건너뛰고 false를 결과로 사용
        size_t jumpToEnd = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, line);
        // 좌항이 true인 경우: 우항을 평가하여 결과로 사용
        compileExpression(expr->right.get());
        size_t skipFalse = chunk().emitJump(OpCode::OP_JUMP, line);
        // 좌항이 false인 경우: false를 push
        chunk().patchJump(jumpToEnd);
        chunk().emitOp(OpCode::OP_FALSE, line);
        chunk().patchJump(skipFalse);
        return;
    }

    // 단축 평가(short-circuit): || 는 좌항이 true면 우항을 평가하지 않음
    if (expr->token->type == TokenType::LOGICAL_OR) {
        compileExpression(expr->left.get());
        // 좌항이 false이면 우항 평가로 진행
        size_t jumpToRight = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, line);
        // 좌항이 true인 경우: true를 결과로 사용
        chunk().emitOp(OpCode::OP_TRUE, line);
        size_t jumpToEnd = chunk().emitJump(OpCode::OP_JUMP, line);
        // 좌항이 false인 경우: 우항을 평가하여 결과로 사용
        chunk().patchJump(jumpToRight);
        compileExpression(expr->right.get());
        chunk().patchJump(jumpToEnd);
        return;
    }

    // 상수 폴딩: 양쪽이 리터럴이면 컴파일 타임에 계산
    if (tryConstantFold(expr)) return;

    compileExpression(expr->left.get());
    compileExpression(expr->right.get());

    switch (expr->token->type) {
    case TokenType::PLUS: chunk().emitOp(OpCode::OP_ADD, line); break;
    case TokenType::MINUS: chunk().emitOp(OpCode::OP_SUB, line); break;
    case TokenType::ASTERISK: chunk().emitOp(OpCode::OP_MUL, line); break;
    case TokenType::SLASH: chunk().emitOp(OpCode::OP_DIV, line); break;
    case TokenType::PERCENT: chunk().emitOp(OpCode::OP_MOD, line); break;
    case TokenType::POWER: chunk().emitOp(OpCode::OP_POW, line); break;
    case TokenType::BITWISE_AND: chunk().emitOp(OpCode::OP_BITWISE_AND, line); break;
    case TokenType::BITWISE_OR: chunk().emitOp(OpCode::OP_BITWISE_OR, line); break;
    case TokenType::EQUAL: chunk().emitOp(OpCode::OP_EQUAL, line); break;
    case TokenType::NOT_EQUAL: chunk().emitOp(OpCode::OP_NOT_EQUAL, line); break;
    case TokenType::LESS_THAN: chunk().emitOp(OpCode::OP_LESS, line); break;
    case TokenType::GREATER_THAN: chunk().emitOp(OpCode::OP_GREATER, line); break;
    case TokenType::LESS_EQUAL: chunk().emitOp(OpCode::OP_LESS_EQUAL, line); break;
    case TokenType::GREATER_EQUAL: chunk().emitOp(OpCode::OP_GREATER_EQUAL, line); break;
    default:
        throw RuntimeException("알 수 없는 중위 연산자입니다.", line);
    }
}

void Compiler::compilePrefix(PrefixExpression* expr) {
    long long line = expr->token ? expr->token->line : 0;

    // 단항 상수 폴딩
    if (expr->token->type == TokenType::MINUS) {
        if (auto* intLit = dynamic_cast<IntegerLiteral*>(expr->right.get())) {
            emitConstant(make_shared<Integer>(-intLit->value), line);
            return;
        }
        if (auto* floatLit = dynamic_cast<FloatLiteral*>(expr->right.get())) {
            emitConstant(make_shared<Float>(-floatLit->value), line);
            return;
        }
    }
    if (expr->token->type == TokenType::BANG) {
        if (auto* boolLit = dynamic_cast<BooleanLiteral*>(expr->right.get())) {
            chunk().emitOp(boolLit->value ? OpCode::OP_FALSE : OpCode::OP_TRUE, line);
            return;
        }
    }

    compileExpression(expr->right.get());
    if (expr->token->type == TokenType::MINUS) {
        chunk().emitOp(OpCode::OP_NEGATE, line);
    } else if (expr->token->type == TokenType::BANG) {
        chunk().emitOp(OpCode::OP_NOT, line);
    }
}

void Compiler::compileInitialization(InitializationStatement* stmt) {
    long long line = stmt->type ? stmt->type->line : 0;
    compileExpression(stmt->value.get());

    if (current->scopeDepth > 0) {
        declareLocal(stmt->name);
    } else {
        uint16_t nameIdx = identifierConstant(stmt->name);
        chunk().emitOpAndUint16(OpCode::OP_DEFINE_GLOBAL, nameIdx, line);
    }
}

void Compiler::compileAssignment(AssignmentStatement* stmt) {
    compileExpression(stmt->value.get());

    // 멤버 대입: 자기.필드
    if (stmt->name.size() > 6 && stmt->name.substr(0, 7) == "자기.") {
        // "자기."는 UTF-8로 9바이트 (자기=6 + .=1... wait)
        // Actually in this codebase, 자기 is Korean UTF-8 chars stored as string
        // The parser stores "자기.필드" as the name
        string fieldName = stmt->name.substr(stmt->name.find('.') + 1);
        // 자기를 스택에 로드
        compileIdentifier("자기", 0);
        uint16_t nameIdx = identifierConstant(fieldName);
        chunk().emitOpAndUint16(OpCode::OP_SET_MEMBER, nameIdx, 0);
        chunk().emitOp(OpCode::OP_POP, 0); // SET_MEMBER이 남긴 값 정리
        return;
    }

    int slot = resolveLocal(current, stmt->name);
    if (slot != -1) {
        chunk().emitOpAndUint16(OpCode::OP_SET_LOCAL, static_cast<uint16_t>(slot), 0);
    } else {
        int uv = resolveUpvalue(current, stmt->name);
        if (uv != -1) {
            chunk().emitOpAndUint16(OpCode::OP_SET_UPVALUE, static_cast<uint16_t>(uv), 0);
        } else {
            uint16_t nameIdx = identifierConstant(stmt->name);
            chunk().emitOpAndUint16(OpCode::OP_SET_GLOBAL, nameIdx, 0);
        }
    }
    // SET_LOCAL/SET_GLOBAL/SET_UPVALUE는 값을 스택에 유지하므로, 문(statement)으로서 pop 필요
    chunk().emitOp(OpCode::OP_POP, 0);
}

void Compiler::compileIndexAssignment(IndexAssignmentStatement* stmt) {
    long long line = 0;
    compileExpression(stmt->collection.get());
    compileExpression(stmt->index.get());
    compileExpression(stmt->value.get());
    chunk().emitOp(OpCode::OP_INDEX_SET, line);
    chunk().emitOp(OpCode::OP_POP, line);
}

void Compiler::compileCompoundAssignment(CompoundAssignmentStatement* stmt) {
    long long line = stmt->op ? stmt->op->line : 0;

    // 현재 값 로드
    compileIdentifier(stmt->name, line);

    // 우항 컴파일
    compileExpression(stmt->value.get());

    // 연산
    switch (stmt->op->type) {
    case TokenType::PLUS_ASSIGN: chunk().emitOp(OpCode::OP_ADD, line); break;
    case TokenType::MINUS_ASSIGN: chunk().emitOp(OpCode::OP_SUB, line); break;
    case TokenType::ASTERISK_ASSIGN: chunk().emitOp(OpCode::OP_MUL, line); break;
    case TokenType::SLASH_ASSIGN: chunk().emitOp(OpCode::OP_DIV, line); break;
    case TokenType::PERCENT_ASSIGN: chunk().emitOp(OpCode::OP_MOD, line); break;
    default: break;
    }

    // 결과 저장
    int slot = resolveLocal(current, stmt->name);
    if (slot != -1) {
        chunk().emitOpAndUint16(OpCode::OP_SET_LOCAL, static_cast<uint16_t>(slot), line);
    } else {
        int uv = resolveUpvalue(current, stmt->name);
        if (uv != -1) {
            chunk().emitOpAndUint16(OpCode::OP_SET_UPVALUE, static_cast<uint16_t>(uv), line);
        } else {
            uint16_t nameIdx = identifierConstant(stmt->name);
            chunk().emitOpAndUint16(OpCode::OP_SET_GLOBAL, nameIdx, line);
        }
    }
    // SET_LOCAL/SET_GLOBAL/SET_UPVALUE는 값을 스택에 유지하므로, 문(statement)으로서 pop 필요
    chunk().emitOp(OpCode::OP_POP, line);
}

void Compiler::compileBlock(BlockStatement* stmt) {
    beginScope();
    for (auto& s : stmt->statements) {
        compileStatement(s.get());
    }
    endScope(0);
}

void Compiler::compileIf(IfStatement* stmt) {
    compileExpression(stmt->condition.get());

    size_t elseJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

    compileBlock(stmt->consequence.get());

    if (stmt->then) {
        size_t endJump = chunk().emitJump(OpCode::OP_JUMP, 0);
        chunk().patchJump(elseJump);
        compileBlock(stmt->then.get());
        chunk().patchJump(endJump);
    } else {
        chunk().patchJump(elseJump);
    }
}

void Compiler::compileWhile(WhileStatement* stmt) {
    size_t loopStart = chunk().code.size();
    current->loops.push_back({loopStart, 0, {}, {}, current->scopeDepth});

    compileExpression(stmt->condition.get());
    size_t exitJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

    beginScope();
    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }
    endScope(0);

    chunk().emitLoop(loopStart, 0);
    chunk().patchJump(exitJump);

    // break 점프 패치
    auto& loop = current->loops.back();
    for (auto offset : loop.breakJumps) {
        chunk().patchJump(offset);
    }
    current->loops.pop_back();
}

void Compiler::compileForEach(ForEachStatement* stmt) {
    // iterable을 스택에 로드
    compileExpression(stmt->iterable.get());
    chunk().emitOp(OpCode::OP_ITER_INIT, 0);

    // 이터레이터를 로컬로 저장
    beginScope();
    uint16_t iterSlot = declareLocal("__iter__");
    (void)iterSlot;

    size_t loopStart = chunk().code.size();
    current->loops.push_back({loopStart, 0, {}, {}, current->scopeDepth});

    size_t exitJump = chunk().emitJump(OpCode::OP_ITER_NEXT, 0);

    chunk().emitOp(OpCode::OP_ITER_VALUE, 0);
    uint16_t elemSlot = declareLocal(stmt->elementName);
    (void)elemSlot;

    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }

    // 원소 로컬 제거
    chunk().emitOp(OpCode::OP_POP, 0);
    current->locals.pop_back();

    chunk().emitLoop(loopStart, 0);
    chunk().patchJump(exitJump);

    auto& loop = current->loops.back();
    for (auto offset : loop.breakJumps) {
        chunk().patchJump(offset);
    }
    current->loops.pop_back();

    endScope(0);
}

void Compiler::compileForRange(ForRangeStatement* stmt) {
    // 반복 정수 i = start 부터 end 까지:
    beginScope();

    // 시작값을 로컬로 저장 (루프 변수)
    compileExpression(stmt->startExpr.get());
    uint16_t varSlot = declareLocal(stmt->varName);

    // 종료값을 로컬로 저장
    compileExpression(stmt->endExpr.get());
    uint16_t endSlot = declareLocal("__end__");
    (void)endSlot;

    size_t loopStart = chunk().code.size();
    // continueTarget = 1 is a sentinel meaning "use continueJumps for forward patching"
    current->loops.push_back({loopStart, 1, {}, {}, current->scopeDepth});

    // 조건: i < end
    chunk().emitOpAndUint16(OpCode::OP_GET_LOCAL, varSlot, 0);
    chunk().emitOpAndUint16(OpCode::OP_GET_LOCAL, static_cast<uint16_t>(varSlot + 1), 0);
    chunk().emitOp(OpCode::OP_LESS, 0);
    size_t exitJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

    // 본문
    beginScope();
    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }
    endScope(0);

    // continue 점프 패치: continue는 여기(증분 단계)로 점프해야 함
    auto& loop = current->loops.back();
    for (auto offset : loop.continueJumps) {
        chunk().patchJump(offset);
    }

    // i += 1
    chunk().emitOpAndUint16(OpCode::OP_GET_LOCAL, varSlot, 0);
    emitConstant(make_shared<Integer>(1), 0);
    chunk().emitOp(OpCode::OP_ADD, 0);
    chunk().emitOpAndUint16(OpCode::OP_SET_LOCAL, varSlot, 0);
    chunk().emitOp(OpCode::OP_POP, 0); // SET_LOCAL이 남긴 값 정리

    chunk().emitLoop(loopStart, 0);
    chunk().patchJump(exitJump);

    // break 점프 패치
    for (auto offset : loop.breakJumps) {
        chunk().patchJump(offset);
    }
    current->loops.pop_back();

    endScope(0);
}

void Compiler::compileReturn(ReturnStatement* stmt) {
    compileExpression(stmt->expression.get());
    chunk().emitOp(OpCode::OP_RETURN, 0);
}

void Compiler::compileFunction(FunctionStatement* stmt) {
    CompilerState funcState;
    funcState.function = make_shared<CompiledFunction>();
    funcState.function->name = stmt->name;
    funcState.function->arity = static_cast<int>(stmt->parameters.size());
    funcState.scopeDepth = 1;
    funcState.enclosing = current;

    CompilerState* enclosing = current;
    current = &funcState;

    for (auto& param : stmt->parameters) {
        declareLocal(param->name);
    }

    // 기본 매개변수 값 사전 평가
    funcState.function->defaultValues.resize(stmt->parameters.size());
    int defaultCount = 0;
    for (size_t i = 0; i < stmt->defaultValues.size(); i++) {
        if (stmt->defaultValues[i]) {
            defaultCount++;
            if (auto* intLit = dynamic_cast<IntegerLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Integer>(intLit->value);
            } else if (auto* floatLit = dynamic_cast<FloatLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Float>(floatLit->value);
            } else if (auto* strLit = dynamic_cast<StringLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<String>(strLit->value);
            } else if (auto* boolLit = dynamic_cast<BooleanLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Boolean>(boolLit->value);
            } else if (dynamic_cast<NullLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Null>();
            } else {
                throw RuntimeException("VM에서는 상수 기본값만 지원합니다.", 0);
            }
        }
    }
    funcState.function->defaultCount = defaultCount;

    funcState.function->hasYield = containsYield(stmt->body.get());

    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }

    chunk().emitOp(OpCode::OP_NULL, 0);
    chunk().emitOp(OpCode::OP_RETURN, 0);

    current->function->localCount = static_cast<int>(current->locals.size());
    auto upvalues = current->upvalues;
    current = enclosing;

    // 업밸류가 있으면 OP_CLOSURE, 없으면 OP_CONSTANT
    uint16_t constIdx = chunk().addConstant(funcState.function);
    if (!upvalues.empty()) {
        chunk().emitOpAndUint16(OpCode::OP_CLOSURE, constIdx, 0);
        // 업밸류 디스크립터
        chunk().emitByte(static_cast<uint8_t>(upvalues.size()), 0);
        for (auto& uv : upvalues) {
            chunk().emitByte(uv.isLocal ? 1 : 0, 0);
            chunk().emitByte(static_cast<uint8_t>((uv.index >> 8) & 0xff), 0);
            chunk().emitByte(static_cast<uint8_t>(uv.index & 0xff), 0);
        }
    } else {
        chunk().emitOpAndUint16(OpCode::OP_CONSTANT, constIdx, 0);
    }

    uint16_t nameIdx = identifierConstant(stmt->name);
    chunk().emitOpAndUint16(OpCode::OP_DEFINE_GLOBAL, nameIdx, 0);
}

void Compiler::compileClass(ClassStatement* stmt) {
    uint16_t nameIdx = identifierConstant(stmt->name);

    // 생성자 컴파일
    shared_ptr<CompiledFunction> constructorFn;
    if (stmt->constructorBody) {
        CompilerState ctorState;
        ctorState.function = make_shared<CompiledFunction>();
        ctorState.function->name = stmt->name + ".생성";
        ctorState.scopeDepth = 1;

        CompilerState* enclosing = current;
        current = &ctorState;

        declareLocal("자기");
        for (auto& param : stmt->constructorParams) {
            declareLocal(param->name);
        }
        current->function->arity = static_cast<int>(stmt->constructorParams.size());

        for (auto& s : stmt->constructorBody->statements) {
            compileStatement(s.get());
        }
        // 생성자는 자기를 반환
        compileIdentifier("자기", 0);
        chunk().emitOp(OpCode::OP_RETURN, 0);
        current->function->localCount = static_cast<int>(current->locals.size());
        constructorFn = ctorState.function;
        current = enclosing;
    }

    // 메서드 컴파일
    map<string, shared_ptr<CompiledFunction>> compiledMethods;
    for (auto& method : stmt->methods) {
        CompilerState methodState;
        methodState.function = make_shared<CompiledFunction>();
        methodState.function->name = stmt->name + "." + method->name;
        methodState.scopeDepth = 1;

        CompilerState* enclosing = current;
        current = &methodState;

        declareLocal("자기");
        for (auto& param : method->parameters) {
            declareLocal(param->name);
        }
        current->function->arity = static_cast<int>(method->parameters.size());

        // 메서드 기본 매개변수 값 사전 평가
        methodState.function->defaultValues.resize(method->parameters.size());
        int methodDefaultCount = 0;
        for (size_t i = 0; i < method->defaultValues.size(); i++) {
            if (method->defaultValues[i]) {
                methodDefaultCount++;
                if (auto* intLit = dynamic_cast<IntegerLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Integer>(intLit->value);
                } else if (auto* floatLit = dynamic_cast<FloatLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Float>(floatLit->value);
                } else if (auto* strLit = dynamic_cast<StringLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<String>(strLit->value);
                } else if (auto* boolLit = dynamic_cast<BooleanLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Boolean>(boolLit->value);
                } else if (dynamic_cast<NullLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Null>();
                } else {
                    throw RuntimeException("VM에서는 상수 기본값만 지원합니다.", 0);
                }
            }
        }
        methodState.function->defaultCount = methodDefaultCount;

        for (auto& s : method->body->statements) {
            compileStatement(s.get());
        }
        chunk().emitOp(OpCode::OP_NULL, 0);
        chunk().emitOp(OpCode::OP_RETURN, 0);
        current->function->localCount = static_cast<int>(current->locals.size());
        compiledMethods[method->name] = methodState.function;
        current = enclosing;
    }

    // CompiledClassDef 생성
    auto classDef = make_shared<CompiledClassDef>();
    classDef->name = stmt->name;
    classDef->fieldNames = stmt->fieldNames;
    classDef->fieldTypes = stmt->fieldTypes;
    classDef->compiledConstructor = constructorFn;
    classDef->compiledMethods = compiledMethods;

    // 상속 처리: 부모 클래스명 저장 (VM에서 런타임에 해결)
    if (!stmt->parentName.empty()) {
        classDef->parentName = stmt->parentName;
    }

    uint16_t classConstIdx = chunk().addConstant(classDef);
    chunk().emitOpAndUint16(OpCode::OP_CONSTANT, classConstIdx, 0);
    chunk().emitOpAndUint16(OpCode::OP_DEFINE_GLOBAL, nameIdx, 0);
}

void Compiler::compileMatch(MatchStatement* stmt) {
    compileExpression(stmt->subject.get());

    vector<size_t> endJumps;

    for (size_t i = 0; i < stmt->caseValues.size(); i++) {
        auto* caseExpr = stmt->caseValues[i].get();

        // 범위 패턴: 경우 1~5:
        if (auto* rangePattern = dynamic_cast<RangePatternExpression*>(caseExpr)) {
            chunk().emitOp(OpCode::OP_DUP, 0); // subject 복사
            compileExpression(rangePattern->start.get());
            compileExpression(rangePattern->end.get());
            chunk().emitOp(OpCode::OP_RANGE_CHECK, 0);
        }
        // 타입 패턴: 경우 정수:
        else if (auto* typePattern = dynamic_cast<TypePatternExpression*>(caseExpr)) {
            chunk().emitOp(OpCode::OP_DUP, 0); // subject 복사
            // 타입 이름을 상수로 저장
            string typeName = typePattern->typeToken->text;
            uint16_t idx = chunk().addConstant(make_shared<String>(typeName));
            chunk().emitOpAndUint16(OpCode::OP_TYPE_CHECK, idx, 0);
        }
        // 일반 값 매칭
        else {
            chunk().emitOp(OpCode::OP_DUP, 0); // subject 복사
            compileExpression(caseExpr);
            chunk().emitOp(OpCode::OP_EQUAL, 0);
        }

        // 조건 가드: 만약 조건
        size_t skipJump;
        if (i < stmt->caseGuards.size() && stmt->caseGuards[i]) {
            // 패턴이 매칭되지 않으면 가드를 건너뜀
            size_t guardSkip = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

            // 가드 조건 평가
            compileExpression(stmt->caseGuards[i].get());
            // 가드 결과로 점프 결정
            skipJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

            // guardSkip은 가드 실패 시와 같은 곳으로 점프해야 함
            // 하지만 OP_JUMP_IF_FALSE가 false를 pop하므로, guardSkip 점프 시 false를 push해야 함
            // 대신 다른 접근: guardSkip을 별도의 위치로 점프시키고, 거기서 false를 push
            size_t jumpToBody = chunk().emitJump(OpCode::OP_JUMP, 0);

            chunk().patchJump(guardSkip);
            // 패턴 불일치 -> 다음 case로
            size_t jumpToNext = chunk().emitJump(OpCode::OP_JUMP, 0);

            chunk().patchJump(skipJump);
            // 가드 불일치 -> 다음 case로
            size_t jumpToNext2 = chunk().emitJump(OpCode::OP_JUMP, 0);

            chunk().patchJump(jumpToBody);
            // case body
            chunk().emitOp(OpCode::OP_POP, 0); // subject 제거
            beginScope();
            for (auto& s : stmt->caseBodies[i]->statements) {
                compileStatement(s.get());
            }
            endScope(0);
            endJumps.push_back(chunk().emitJump(OpCode::OP_JUMP, 0));

            chunk().patchJump(jumpToNext);
            chunk().patchJump(jumpToNext2);
        } else {
            skipJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

            chunk().emitOp(OpCode::OP_POP, 0); // subject 제거
            beginScope();
            for (auto& s : stmt->caseBodies[i]->statements) {
                compileStatement(s.get());
            }
            endScope(0);
            endJumps.push_back(chunk().emitJump(OpCode::OP_JUMP, 0));

            chunk().patchJump(skipJump);
        }
    }

    // default
    chunk().emitOp(OpCode::OP_POP, 0); // subject 제거
    if (stmt->defaultBody) {
        beginScope();
        for (auto& s : stmt->defaultBody->statements) {
            compileStatement(s.get());
        }
        endScope(0);
    }

    for (auto offset : endJumps) {
        chunk().patchJump(offset);
    }
}

void Compiler::compileTryCatch(TryCatchStatement* stmt) {
    size_t tryBeginOffset = chunk().emitJump(OpCode::OP_TRY_BEGIN, 0);

    beginScope();
    for (auto& s : stmt->tryBody->statements) {
        compileStatement(s.get());
    }
    endScope(0);

    chunk().emitOp(OpCode::OP_TRY_END, 0);
    size_t skipCatch = chunk().emitJump(OpCode::OP_JUMP, 0);

    chunk().patchJump(tryBeginOffset);

    // catch 블록: 에러가 스택에 있음
    beginScope();
    declareLocal(stmt->errorName);
    for (auto& s : stmt->catchBody->statements) {
        compileStatement(s.get());
    }
    endScope(0);

    chunk().patchJump(skipCatch);
}

void Compiler::compileImport(ImportStatement* stmt) {
    uint16_t nameIdx = chunk().addConstant(make_shared<String>(stmt->filename));
    chunk().emitOpAndUint16(OpCode::OP_IMPORT, nameIdx, 0);
}

void Compiler::compileCall(CallExpression* expr) {
    compileExpression(expr->function.get());
    for (auto& arg : expr->arguments) {
        compileExpression(arg.get());
    }
    if (expr->arguments.size() > 255) {
        throw RuntimeException("함수 호출의 인자 개수가 255개를 초과했습니다.", 0);
    }
    chunk().emitByte(static_cast<uint8_t>(OpCode::OP_CALL), 0);
    chunk().emitByte(static_cast<uint8_t>(expr->arguments.size()), 0);
}

void Compiler::compileMemberAccess(MemberAccessExpression* expr) {
    compileExpression(expr->object.get());
    uint16_t nameIdx = identifierConstant(expr->member);
    chunk().emitOpAndUint16(OpCode::OP_GET_MEMBER, nameIdx, 0);
}

void Compiler::compileMethodCall(MethodCallExpression* expr) {
    compileExpression(expr->object.get());
    for (auto& arg : expr->arguments) {
        compileExpression(arg.get());
    }
    uint16_t nameIdx = identifierConstant(expr->method);
    chunk().emitOpAndUint16(OpCode::OP_INVOKE, nameIdx, 0);
    chunk().emitByte(static_cast<uint8_t>(expr->arguments.size()), 0);
}

void Compiler::compileIndex(IndexExpression* expr) {
    compileExpression(expr->name.get());
    compileExpression(expr->index.get());
    chunk().emitOp(OpCode::OP_INDEX_GET, 0);
}

void Compiler::compileSlice(SliceExpression* expr) {
    compileExpression(expr->object.get());

    uint8_t flags = 0;
    if (expr->start) {
        compileExpression(expr->start.get());
        flags |= 0x01;
    }
    if (expr->end) {
        compileExpression(expr->end.get());
        flags |= 0x02;
    }

    chunk().emitOp(OpCode::OP_SLICE, 0);
    chunk().emitByte(flags, 0);
}

bool Compiler::containsYield(BlockStatement* block) {
    if (!block) return false;
    for (auto& stmt : block->statements) {
        if (dynamic_cast<YieldStatement*>(stmt.get())) return true;
        if (auto* whileStmt = dynamic_cast<WhileStatement*>(stmt.get())) {
            if (containsYield(whileStmt->body.get())) return true;
        }
        if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
            if (containsYield(ifStmt->consequence.get())) return true;
            if (containsYield(ifStmt->then.get())) return true;
        }
        if (auto* forEachStmt = dynamic_cast<ForEachStatement*>(stmt.get())) {
            if (containsYield(forEachStmt->body.get())) return true;
        }
        if (auto* forRangeStmt = dynamic_cast<ForRangeStatement*>(stmt.get())) {
            if (containsYield(forRangeStmt->body.get())) return true;
        }
    }
    return false;
}

void Compiler::compileYield(YieldStatement* stmt) {
    compileExpression(stmt->expression.get());
    chunk().emitOp(OpCode::OP_YIELD, 0);
}
