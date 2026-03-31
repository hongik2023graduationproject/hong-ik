#ifndef OPCODE_H
#define OPCODE_H

#include <cstdint>
#include <string>

enum class OpCode : uint8_t {
    // 상수 및 리터럴
    OP_CONSTANT,        // [uint16 index] constants[index]를 스택에 push
    OP_NULL,            // Null을 push
    OP_TRUE,            // Boolean(true) push
    OP_FALSE,           // Boolean(false) push

    // 산술 (이항)
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_BITWISE_AND,
    OP_BITWISE_OR,

    // 비교
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_LESS,
    OP_GREATER,
    OP_LESS_EQUAL,
    OP_GREATER_EQUAL,

    // 논리
    OP_AND,
    OP_OR,

    // 단항
    OP_NEGATE,
    OP_NOT,

    // 변수
    OP_GET_LOCAL,       // [uint16 slot]
    OP_SET_LOCAL,       // [uint16 slot]
    OP_GET_GLOBAL,      // [uint16 nameIdx]
    OP_SET_GLOBAL,      // [uint16 nameIdx]
    OP_DEFINE_GLOBAL,   // [uint16 nameIdx]

    // 제어 흐름
    OP_JUMP,            // [uint16 offset]
    OP_JUMP_IF_FALSE,   // [uint16 offset]
    OP_LOOP,            // [uint16 offset] (뒤로 점프)

    // 함수 및 호출
    OP_CALL,            // [uint8 argCount]
    OP_RETURN,
    OP_CLOSURE,         // [uint16 constIdx] [upvalue count * (uint8 isLocal, uint16 index)]
    OP_GET_UPVALUE,     // [uint16 slot]
    OP_SET_UPVALUE,     // [uint16 slot]

    // 내장 함수
    OP_GET_BUILTIN,     // [uint16 nameIdx]

    // 자료 구조
    OP_BUILD_ARRAY,     // [uint16 count]
    OP_BUILD_HASHMAP,   // [uint16 count] (key-value 쌍 수)
    OP_BUILD_TUPLE,     // [uint16 count]
    OP_INDEX_GET,       // pop index, pop collection
    OP_INDEX_SET,       // pop value, pop index, pop collection; set collection[index] = value
    OP_SLICE,           // [uint8 flags] flags: bit0=has_start, bit1=has_end

    // 클래스 및 인스턴스
    OP_GET_MEMBER,      // [uint16 nameIdx]
    OP_SET_MEMBER,      // [uint16 nameIdx]
    OP_INVOKE,          // [uint16 nameIdx] [uint8 argCount]

    // 예외 처리
    OP_TRY_BEGIN,       // [uint16 catchOffset]
    OP_TRY_END,

    // 반복자 (각각)
    OP_ITER_INIT,
    OP_ITER_NEXT,       // [uint16 exitOffset]
    OP_ITER_VALUE,

    // 스택
    OP_POP,
    OP_DUP,

    // 문자열 보간
    OP_INTERPOLATE,     // [uint16 segmentCount]

    // 패턴 매칭
    OP_RANGE_CHECK,     // pop end, pop start, pop subject → push bool (start <= subject <= end)
    OP_TYPE_CHECK,      // [uint16 typeIdx] pop subject → push bool (subject의 타입이 typeIdx와 일치)

    OP_IMPORT,          // [uint16 nameIdx] import and execute file

    OP_YIELD,           // pop value, add to frame's yield buffer
};

inline std::string opcodeName(OpCode op) {
    switch (op) {
    case OpCode::OP_CONSTANT: return "OP_CONSTANT";
    case OpCode::OP_NULL: return "OP_NULL";
    case OpCode::OP_TRUE: return "OP_TRUE";
    case OpCode::OP_FALSE: return "OP_FALSE";
    case OpCode::OP_ADD: return "OP_ADD";
    case OpCode::OP_SUB: return "OP_SUB";
    case OpCode::OP_MUL: return "OP_MUL";
    case OpCode::OP_DIV: return "OP_DIV";
    case OpCode::OP_MOD: return "OP_MOD";
    case OpCode::OP_POW: return "OP_POW";
    case OpCode::OP_BITWISE_AND: return "OP_BITWISE_AND";
    case OpCode::OP_BITWISE_OR: return "OP_BITWISE_OR";
    case OpCode::OP_EQUAL: return "OP_EQUAL";
    case OpCode::OP_NOT_EQUAL: return "OP_NOT_EQUAL";
    case OpCode::OP_LESS: return "OP_LESS";
    case OpCode::OP_GREATER: return "OP_GREATER";
    case OpCode::OP_LESS_EQUAL: return "OP_LESS_EQUAL";
    case OpCode::OP_GREATER_EQUAL: return "OP_GREATER_EQUAL";
    case OpCode::OP_AND: return "OP_AND";
    case OpCode::OP_OR: return "OP_OR";
    case OpCode::OP_NEGATE: return "OP_NEGATE";
    case OpCode::OP_NOT: return "OP_NOT";
    case OpCode::OP_GET_LOCAL: return "OP_GET_LOCAL";
    case OpCode::OP_SET_LOCAL: return "OP_SET_LOCAL";
    case OpCode::OP_GET_GLOBAL: return "OP_GET_GLOBAL";
    case OpCode::OP_SET_GLOBAL: return "OP_SET_GLOBAL";
    case OpCode::OP_DEFINE_GLOBAL: return "OP_DEFINE_GLOBAL";
    case OpCode::OP_JUMP: return "OP_JUMP";
    case OpCode::OP_JUMP_IF_FALSE: return "OP_JUMP_IF_FALSE";
    case OpCode::OP_LOOP: return "OP_LOOP";
    case OpCode::OP_CALL: return "OP_CALL";
    case OpCode::OP_RETURN: return "OP_RETURN";
    case OpCode::OP_CLOSURE: return "OP_CLOSURE";
    case OpCode::OP_GET_UPVALUE: return "OP_GET_UPVALUE";
    case OpCode::OP_SET_UPVALUE: return "OP_SET_UPVALUE";
    case OpCode::OP_GET_BUILTIN: return "OP_GET_BUILTIN";
    case OpCode::OP_BUILD_ARRAY: return "OP_BUILD_ARRAY";
    case OpCode::OP_BUILD_HASHMAP: return "OP_BUILD_HASHMAP";
    case OpCode::OP_BUILD_TUPLE: return "OP_BUILD_TUPLE";
    case OpCode::OP_INDEX_GET: return "OP_INDEX_GET";
    case OpCode::OP_INDEX_SET: return "OP_INDEX_SET";
    case OpCode::OP_SLICE: return "OP_SLICE";
    case OpCode::OP_GET_MEMBER: return "OP_GET_MEMBER";
    case OpCode::OP_SET_MEMBER: return "OP_SET_MEMBER";
    case OpCode::OP_INVOKE: return "OP_INVOKE";
    case OpCode::OP_TRY_BEGIN: return "OP_TRY_BEGIN";
    case OpCode::OP_TRY_END: return "OP_TRY_END";
    case OpCode::OP_ITER_INIT: return "OP_ITER_INIT";
    case OpCode::OP_ITER_NEXT: return "OP_ITER_NEXT";
    case OpCode::OP_ITER_VALUE: return "OP_ITER_VALUE";
    case OpCode::OP_POP: return "OP_POP";
    case OpCode::OP_DUP: return "OP_DUP";
    case OpCode::OP_INTERPOLATE: return "OP_INTERPOLATE";
    case OpCode::OP_RANGE_CHECK: return "OP_RANGE_CHECK";
    case OpCode::OP_TYPE_CHECK: return "OP_TYPE_CHECK";
    case OpCode::OP_IMPORT: return "OP_IMPORT";
    case OpCode::OP_YIELD: return "OP_YIELD";
    default: return "UNKNOWN";
    }
}

#endif // OPCODE_H
