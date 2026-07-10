#include "vm.h"

#include "../environment/environment.h"
#include "../exception/exception.h"
#include "../lexer/lexer.h"
#include "../object/builtin_registry.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include "../util/type_utils.h"
#include "../util/utf8_utils.h"
#include "compiler.h"
#include "vm_internal.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

using namespace std;
using vm_internal::checkedConstant;
using vm_internal::vmValueToObjectType;

namespace {
    // 이식성 있는 signed long long 곱셈 오버플로 검사.
    // __builtin_mul_overflow는 GCC/Clang 전용이라 MSVC에서 컴파일이 깨진다.
    bool mulOverflowsLL(long long a, long long b) {
        if (a == 0 || b == 0) {
            return false;
        }
        constexpr long long kMax = std::numeric_limits<long long>::max();
        constexpr long long kMin = std::numeric_limits<long long>::min();
        // std::abs(kMin)은 UB이므로 별도 처리한다.
        if (a == kMin || b == kMin) {
            return true;
        }
        long long absA = a < 0 ? -a : a;
        long long absB = b < 0 ? -b : b;
        return absA > kMax / absB;
    }

} // namespace

VM::VM(IOContext* ioCtxPtr, ExecutionLimiter* limiterPtr) : ioCtx(ioCtxPtr), limiter(limiterPtr) {
    stack.reserve(STACK_MAX);
    builtins   = BuiltinRegistry::build(ioCtx);
    auto lenIt = builtins.find("길이");
    if (lenIt != builtins.end()) {
        lengthBuiltin_ = lenIt->second;
    }
}

// '길이' fast path (spec D3). 지원 타입이 아니면 false — 호출측이 제네릭 경로로 폴스루해
// 에러 메시지·엣지 시맨틱이 registry 구현과 자동으로 일치한다.
bool VM::tryLengthOf(const VMValue& arg, long long& out) {
    if (!arg.isObject()) {
        return false;
    }
    Object* o = arg.asObject().get();
    if (auto* str = dynamic_cast<String*>(o)) {
        out = static_cast<long long>(utf8::codePointCount(str->value));
        return true;
    }
    if (auto* arr = dynamic_cast<Array*>(o)) {
        out = static_cast<long long>(arr->elements.size());
        return true;
    }
    if (auto* hm = dynamic_cast<HashMap*>(o)) {
        out = static_cast<long long>(hm->pairs.size());
        return true;
    }
    return false;
}

void VM::push(VMValue value) {
    stack.push_back(std::move(value));
}

VMValue VM::pop() {
    if (stack.empty()) {
        throw RuntimeException("VM 스택이 비어있습니다.", 0);
    }
    auto val = std::move(stack.back());
    stack.pop_back();
    return val;
}

VMValue& VM::peek(int distance) {
    if (distance < 0 || static_cast<size_t>(distance) >= stack.size()) {
        throw RuntimeException("VM 스택 범위 밖 접근입니다.", 0);
    }
    return stack[stack.size() - 1 - distance];
}

void VM::checkLimits() {
    // 타임아웃은 Evaluator와 동일하게 RuntimeException으로 throw하여 동작 일관성을 유지한다.
    // (incrementLoopCounter는 std::runtime_error를 throw하므로 사용자 try/catch는 잡지 못한다.)
    if (limiter && limiter->isTimeoutExceeded()) {
        throw RuntimeException("실행 시간 제한을 초과했습니다.", currentLine());
    }
}

CallFrame& VM::currentFrame() {
    // 손상된 바이트코드 등으로 호출 프레임이 모두 비었을 때 빈 vector::back()을 호출하면 UB가 발생한다.
    // 정상 경로(opReturn)에서는 frames.empty() 분기로 안전하게 종료되므로, 이 가드는 비정상 경로 한정 방어다.
    if (frames.empty()) {
        throw RuntimeException("VM 호출 프레임이 비어 있습니다.", 0);
    }
    return frames.back();
}

uint8_t VM::readByte() {
    auto& frame = currentFrame();
    if (frame.ip >= frame.function->code.size()) {
        throw RuntimeException("VM 바이트코드 범위 밖 읽기입니다.", currentLine());
    }
    return frame.function->code[frame.ip++];
}

uint16_t VM::readUint16() {
    uint8_t hi = readByte();
    uint8_t lo = readByte();
    return static_cast<uint16_t>((hi << 8) | lo);
}

long long VM::currentLine() {
    auto& frame = currentFrame();
    if (frame.ip > 0 && frame.ip - 1 < frame.function->lines.size()) {
        return frame.function->lines[frame.ip - 1];
    }
    return 0;
}

shared_ptr<Object> VM::Execute(shared_ptr<CompiledFunction> topLevel) {
    topLevelFn = topLevel;
    CallFrame frame;
    frame.function   = topLevel.get();
    frame.ip         = 0;
    frame.slotOffset = 0;
    frames.push_back(frame);
    return run();
}

VMValue VM::binaryOp(OpCode op, const VMValue& left, const VMValue& right, long long line) {
    // Null checks
    if (left.isNull() || right.isNull()) {
        if (op == OpCode::OP_EQUAL) {
            return VMValue::Bool(left.isNull() && right.isNull());
        }
        if (op == OpCode::OP_NOT_EQUAL) {
            return VMValue::Bool(!(left.isNull() && right.isNull()));
        }
        throw RuntimeException("없음(null) 값에 대해서는 == 와 != 비교만 지원합니다.", line);
    }

    // Int + Int (hot path -- no heap allocation!)
    if (left.isInt() && right.isInt()) {
        long long lv = left.asInt(), rv = right.asInt();
        switch (op) {
        case OpCode::OP_ADD:
            return VMValue::Int(lv + rv);
        case OpCode::OP_SUB:
            return VMValue::Int(lv - rv);
        case OpCode::OP_MUL:
            return VMValue::Int(lv * rv);
        case OpCode::OP_DIV:
            if (rv == 0) {
                throw RuntimeException("0으로 나눌 수 없습니다.", line);
            }
            return VMValue::Int(lv / rv);
        case OpCode::OP_MOD:
            if (rv == 0) {
                throw RuntimeException("0으로 나눌 수 없습니다.", line);
            }
            return VMValue::Int(lv % rv);
        case OpCode::OP_POW:
            {
                if (rv < 0) {
                    return VMValue::Float(std::pow(static_cast<double>(lv), static_cast<double>(rv)));
                }
                // 누적 곱 시 long long 오버플로가 감지되면 즉시 double 경로로 폴백한다.
                // 그대로 두면 signed integer overflow가 일어나 UB가 발생하고 잘못된 정수 결과가 반환된다.
                long long result = 1;
                for (long long i = 0; i < rv; i++) {
                    if (mulOverflowsLL(result, lv)) {
                        return VMValue::Float(std::pow(static_cast<double>(lv), static_cast<double>(rv)));
                    }
                    result *= lv;
                }
                return VMValue::Int(result);
            }
        case OpCode::OP_BITWISE_AND:
            return VMValue::Int(lv & rv);
        case OpCode::OP_BITWISE_OR:
            return VMValue::Int(lv | rv);
        case OpCode::OP_EQUAL:
            return VMValue::Bool(lv == rv);
        case OpCode::OP_NOT_EQUAL:
            return VMValue::Bool(lv != rv);
        case OpCode::OP_LESS:
            return VMValue::Bool(lv < rv);
        case OpCode::OP_GREATER:
            return VMValue::Bool(lv > rv);
        case OpCode::OP_LESS_EQUAL:
            return VMValue::Bool(lv <= rv);
        case OpCode::OP_GREATER_EQUAL:
            return VMValue::Bool(lv >= rv);
        default:
            break;
        }
    }

    // Float path — 양쪽 모두 숫자일 때만 (런타임 일관성 D3).
    // 이전에는 한쪽만 float여도 진입해 비숫자 피연산자를 0으로 취급했다 (1.5 + "a" → 1.5).
    const bool leftNumeric  = left.isFloat() || left.isInt();
    const bool rightNumeric = right.isFloat() || right.isInt();
    double lv = 0, rv = 0;
    if (left.isFloat()) {
        lv = left.asFloat();
    } else if (left.isInt()) {
        lv = static_cast<double>(left.asInt());
    }
    if (right.isFloat()) {
        rv = right.asFloat();
    } else if (right.isInt()) {
        rv = static_cast<double>(right.asInt());
    }

    if (leftNumeric && rightNumeric) {
        switch (op) {
        case OpCode::OP_ADD:
            return VMValue::Float(lv + rv);
        case OpCode::OP_SUB:
            return VMValue::Float(lv - rv);
        case OpCode::OP_MUL:
            return VMValue::Float(lv * rv);
        case OpCode::OP_DIV:
            if (rv == 0.0) {
                throw RuntimeException("0으로 나눌 수 없습니다.", line);
            }
            return VMValue::Float(lv / rv);
        case OpCode::OP_POW:
            return VMValue::Float(std::pow(lv, rv));
        case OpCode::OP_EQUAL:
            return VMValue::Bool(lv == rv);
        case OpCode::OP_NOT_EQUAL:
            return VMValue::Bool(lv != rv);
        case OpCode::OP_LESS:
            return VMValue::Bool(lv < rv);
        case OpCode::OP_GREATER:
            return VMValue::Bool(lv > rv);
        case OpCode::OP_LESS_EQUAL:
            return VMValue::Bool(lv <= rv);
        case OpCode::OP_GREATER_EQUAL:
            return VMValue::Bool(lv >= rv);
        default:
            break;
        }
    }

    // Boolean path
    if (left.isBool() && right.isBool()) {
        switch (op) {
        case OpCode::OP_AND:
            return VMValue::Bool(left.asBool() && right.asBool());
        case OpCode::OP_OR:
            return VMValue::Bool(left.asBool() || right.asBool());
        case OpCode::OP_EQUAL:
            return VMValue::Bool(left.asBool() == right.asBool());
        case OpCode::OP_NOT_EQUAL:
            return VMValue::Bool(left.asBool() != right.asBool());
        default:
            break;
        }
    }

    // String path -- need to convert to Object for complex operations
    if (left.isObject() && right.isObject()) {
        auto* ls = dynamic_cast<String*>(left.asObject().get());
        auto* rs = dynamic_cast<String*>(right.asObject().get());
        if (ls && rs) {
            switch (op) {
            case OpCode::OP_ADD:
                return VMValue::Obj(make_shared<String>(ls->value + rs->value));
            case OpCode::OP_EQUAL:
                return VMValue::Bool(ls->value == rs->value);
            case OpCode::OP_NOT_EQUAL:
                return VMValue::Bool(ls->value != rs->value);
            case OpCode::OP_LESS:
                return VMValue::Bool(ls->value < rs->value);
            case OpCode::OP_GREATER:
                return VMValue::Bool(ls->value > rs->value);
            case OpCode::OP_LESS_EQUAL:
                return VMValue::Bool(ls->value <= rs->value);
            case OpCode::OP_GREATER_EQUAL:
                return VMValue::Bool(ls->value >= rs->value);
            default:
                break;
            }
        }
    }

    throw RuntimeException(typeToKorean(vmValueToObjectType(left)) + " " + opToSymbol(op) + " "
                               + typeToKorean(vmValueToObjectType(right))
                               + ": 좌항과 우항의 타입을 연산할 수 없습니다.",
        line);
}

shared_ptr<Object> VM::run() {
    while (true) {
        auto& frame = currentFrame();
        if (frame.ip >= frame.function->code.size()) {
            break;
        }

        OpCode instruction = static_cast<OpCode>(readByte());

        try {
            switch (instruction) {
            case OpCode::OP_CONSTANT:
                {
                    uint16_t idx = readUint16();
                    push(VMValue::fromObject(checkedConstant(frame.function->constants, idx, currentLine())));
                    break;
                }
            case OpCode::OP_NULL:
                push(VMValue::Null());
                break;
            case OpCode::OP_TRUE:
                push(VMValue::Bool(true));
                break;
            case OpCode::OP_FALSE:
                push(VMValue::Bool(false));
                break;

            case OpCode::OP_ADD:
            case OpCode::OP_SUB:
            case OpCode::OP_MUL:
            case OpCode::OP_DIV:
            case OpCode::OP_MOD:
            case OpCode::OP_POW:
            case OpCode::OP_BITWISE_AND:
            case OpCode::OP_BITWISE_OR:
            case OpCode::OP_EQUAL:
            case OpCode::OP_NOT_EQUAL:
            case OpCode::OP_LESS:
            case OpCode::OP_GREATER:
            case OpCode::OP_LESS_EQUAL:
            case OpCode::OP_GREATER_EQUAL:
            case OpCode::OP_AND:
            case OpCode::OP_OR:
                {
                    auto right = pop();
                    auto left  = pop();
                    push(binaryOp(instruction, left, right, currentLine()));
                    break;
                }

            case OpCode::OP_NEGATE:
                opNegate();
                break;
            case OpCode::OP_NOT:
                opNot();
                break;

            case OpCode::OP_GET_LOCAL:
                {
                    uint16_t slot = readUint16();
                    size_t idx    = frame.slotOffset + slot;
                    if (idx >= stack.size()) {
                        throw RuntimeException("VM 로컬 슬롯이 범위 밖입니다.", currentLine());
                    }
                    push(stack[idx]);
                    break;
                }
            case OpCode::OP_SET_LOCAL:
                {
                    uint16_t slot = readUint16();
                    size_t idx    = frame.slotOffset + slot;
                    if (idx >= stack.size()) {
                        throw RuntimeException("VM 로컬 슬롯이 범위 밖입니다.", currentLine());
                    }
                    stack[idx] = peek(0);
                    break;
                }
            case OpCode::OP_GET_GLOBAL:
                opGetGlobal();
                break;
            case OpCode::OP_SET_GLOBAL:
                opSetGlobal();
                break;
            case OpCode::OP_DEFINE_GLOBAL:
                opDefineGlobal();
                break;

            case OpCode::OP_JUMP:
                opJump();
                break;
            case OpCode::OP_JUMP_IF_FALSE:
                opJumpIfFalse();
                break;
            case OpCode::OP_LOOP:
                opLoop();
                break;

            case OpCode::OP_CALL:
                opCall();
                break;
            case OpCode::OP_RETURN:
                {
                    if (auto result = opReturn()) {
                        return *result;
                    }
                    break;
                }

            case OpCode::OP_BUILD_ARRAY:
                opBuildArray();
                break;
            case OpCode::OP_BUILD_HASHMAP:
                opBuildHashMap();
                break;
            case OpCode::OP_BUILD_TUPLE:
                opBuildTuple();
                break;

            case OpCode::OP_INDEX_GET:
                opIndexGet();
                break;
            case OpCode::OP_INDEX_SET:
                opIndexSet();
                break;
            case OpCode::OP_SLICE:
                opSlice();
                break;

            case OpCode::OP_GET_MEMBER:
                opGetMember();
                break;
            case OpCode::OP_SET_MEMBER:
                opSetMember();
                break;
            case OpCode::OP_INVOKE:
                opInvoke();
                break;

            case OpCode::OP_TRY_BEGIN:
                opTryBegin();
                break;
            case OpCode::OP_TRY_END:
                opTryEnd();
                break;

            case OpCode::OP_ITER_INIT:
                opIterInit();
                break;
            case OpCode::OP_ITER_NEXT:
                opIterNext();
                break;
            case OpCode::OP_ITER_VALUE:
                opIterValue();
                break;

            case OpCode::OP_CLOSURE:
                opClosure();
                break;
            case OpCode::OP_GET_UPVALUE:
                opGetUpvalue();
                break;
            case OpCode::OP_SET_UPVALUE:
                opSetUpvalue();
                break;

            case OpCode::OP_POP:
                pop();
                break;
            case OpCode::OP_DUP:
                push(peek(0));
                break;

            case OpCode::OP_RANGE_CHECK:
                opRangeCheck();
                break;
            case OpCode::OP_TYPE_CHECK:
                opTypeCheck();
                break;
            case OpCode::OP_DECL_CHECK:
                opDeclCheck();
                break;
            case OpCode::OP_ASSERT_BOOL:
                opAssertBool();
                break;
            case OpCode::OP_IMPORT:
                opImport();
                break;
            case OpCode::OP_YIELD:
                opYield();
                break;
            case OpCode::OP_INTERPOLATE:
                opInterpolate();
                break;

            default:
                throw RuntimeException(
                    "알 수 없는 바이트코드 명령입니다: " + to_string(static_cast<int>(instruction)), currentLine());
            }
        } catch (const RuntimeException& e) {
            if (!exceptionHandlers.empty()) {
                auto handler = exceptionHandlers.back();
                exceptionHandlers.pop_back();
                while (stack.size() > handler.stackDepth) {
                    stack.pop_back();
                }
                while (frames.size() > static_cast<size_t>(handler.frameDepth)) {
                    frames.pop_back();
                }
                push(VMValue::Obj(make_shared<String>(string(e.what()))));
                currentFrame().ip = handler.catchIp;
                continue;
            }
            throw;
        } catch (const std::exception& e) {
            if (!exceptionHandlers.empty()) {
                auto handler = exceptionHandlers.back();
                exceptionHandlers.pop_back();
                while (stack.size() > handler.stackDepth) {
                    stack.pop_back();
                }
                while (frames.size() > static_cast<size_t>(handler.frameDepth)) {
                    frames.pop_back();
                }
                push(VMValue::Obj(make_shared<String>(string(e.what()))));
                currentFrame().ip = handler.catchIp;
                continue;
            }
            throw RuntimeException(string(e.what()), currentLine());
        }
    }

    if (!stack.empty()) {
        return pop().toObject();
    }
    return make_shared<Null>();
}

void VM::fillDefaults(CompiledFunction* fn, int argCount) {
    int requiredArgs = fn->arity - fn->defaultCount;
    if (argCount < requiredArgs || argCount > fn->arity) {
        throw RuntimeException("함수가 필요한 인자 개수와 입력된 인자 개수가 다릅니다.", currentLine());
    }
    for (int i = argCount; i < fn->arity; i++) {
        if (i < static_cast<int>(fn->defaultValues.size()) && fn->defaultValues[i]) {
            push(VMValue::fromObject(fn->defaultValues[i]));
        } else {
            push(VMValue::Null());
        }
    }
}

// ============================================================================
// Opcode handlers (Phase 2-1에서 run()에서 추출)
// ============================================================================
