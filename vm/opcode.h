#ifndef OPCODE_H
#define OPCODE_H

#include <cstdint>
#include <string>

enum class OpCode : uint8_t {
    // 상수 및 리터럴
    OP_CONSTANT, // [uint16 index] constants[index]를 스택에 push
    OP_NULL, // Null을 push
    OP_TRUE, // Boolean(true) push
    OP_FALSE, // Boolean(false) push

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
    OP_GET_LOCAL, // [uint16 slot]
    OP_SET_LOCAL, // [uint16 slot]
    OP_GET_GLOBAL, // [uint16 nameIdx]
    OP_SET_GLOBAL, // [uint16 nameIdx]
    OP_DEFINE_GLOBAL, // [uint16 nameIdx]

    // 제어 흐름
    OP_JUMP, // [uint16 offset]
    OP_JUMP_IF_FALSE, // [uint16 offset]
    OP_LOOP, // [uint16 offset] (뒤로 점프)

    // 함수 및 호출
    OP_CALL, // [uint8 argCount]
    OP_RETURN,
    OP_CLOSURE, // [uint16 constIdx] [upvalue count * (uint8 isLocal, uint16 index)]
    OP_GET_UPVALUE, // [uint16 slot]
    OP_SET_UPVALUE, // [uint16 slot]

    // 자료 구조
    OP_BUILD_ARRAY, // [uint16 count]
    OP_BUILD_HASHMAP, // [uint16 count] (key-value 쌍 수)
    OP_BUILD_TUPLE, // [uint16 count]
    OP_INDEX_GET, // pop index, pop collection
    OP_INDEX_SET, // pop value, pop index, pop collection; set collection[index] = value
    OP_SLICE, // [uint8 flags] flags: bit0=has_start, bit1=has_end

    // 클래스 및 인스턴스
    OP_GET_MEMBER, // [uint16 nameIdx]
    OP_SET_MEMBER, // [uint16 nameIdx]
    OP_INVOKE, // [uint16 nameIdx] [uint8 argCount]

    // 예외 처리
    OP_TRY_BEGIN, // [uint16 catchOffset]
    OP_TRY_END,

    // 반복자 (각각)
    OP_ITER_INIT,
    OP_ITER_NEXT, // [uint16 exitOffset]
    OP_ITER_VALUE,

    // 스택
    OP_POP,
    OP_DUP,

    // 문자열 보간
    OP_INTERPOLATE, // [uint16 segmentCount]

    // 패턴 매칭
    OP_RANGE_CHECK, // pop end, pop start, pop subject → push bool (start <= subject <= end)
    OP_TYPE_CHECK, // [uint16 typeIdx] pop subject → push bool (subject의 타입이 typeIdx와 일치)
    OP_DECL_CHECK, // [uint16 typeIdx] peek 값이 선언 타입과 호환되는지 검사, 불일치 시 예외 (런타임 일관성 D1)
    OP_ASSERT_BOOL, // peek 값이 논리가 아니면 예외 — &&/|| 피연산자 검사 (런타임 일관성 D4)

    OP_IMPORT, // [uint16 nameIdx] import and execute file

    OP_YIELD, // pop value, add to frame's yield buffer

    // 융합 명령 (vm/peephole.cpp 산출 — 컴파일러가 직접 emit하지 않는다)
    OP_SET_LOCAL_POP, // [uint16 slot] SET_LOCAL 후 즉시 POP
    OP_SET_GLOBAL_POP, // [uint16 nameIdx]
    OP_SET_UPVALUE_POP, // [uint16 slot]
    OP_SET_MEMBER_POP, // [uint16 nameIdx]
};

#endif // OPCODE_H
