// Compiler — 표현식 컴파일 (Phase 3 분할: 코어·문장은 compiler.cpp / compiler_stmt.cpp 참조)
#include "compiler.h"
#include "../exception/exception.h"
#include "../util/type_utils.h"
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std;


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
        chunk().emitOp(OpCode::OP_ASSERT_BOOL, line);  // 논리 전용 (런타임 일관성 D4)
        // 좌항이 false이면 우항을 건너뛰고 false를 결과로 사용
        size_t jumpToEnd = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, line);
        // 좌항이 true인 경우: 우항을 평가하여 결과로 사용
        compileExpression(expr->right.get());
        chunk().emitOp(OpCode::OP_ASSERT_BOOL, line);  // 평가된 우항도 논리 요구 (D4)
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
        chunk().emitOp(OpCode::OP_ASSERT_BOOL, line);  // 논리 전용 (런타임 일관성 D4)
        // 좌항이 false이면 우항 평가로 진행
        size_t jumpToRight = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, line);
        // 좌항이 true인 경우: true를 결과로 사용
        chunk().emitOp(OpCode::OP_TRUE, line);
        size_t jumpToEnd = chunk().emitJump(OpCode::OP_JUMP, line);
        // 좌항이 false인 경우: 우항을 평가하여 결과로 사용
        chunk().patchJump(jumpToRight);
        compileExpression(expr->right.get());
        chunk().emitOp(OpCode::OP_ASSERT_BOOL, line);  // 평가된 우항도 논리 요구 (D4)
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


