#ifndef TYPE_UTILS_H
#define TYPE_UTILS_H

#include "../object/object_type.h"
#include "../token/token_type.h"
#include "../vm/opcode.h"
#include <string>

// TokenType(한글 키워드) → ObjectType 변환
inline ObjectType tokenTypeToObjectType(TokenType tt) {
    switch (tt) {
    case TokenType::정수: return ObjectType::INTEGER;
    case TokenType::실수: return ObjectType::FLOAT;
    case TokenType::문자: return ObjectType::STRING;
    case TokenType::논리: return ObjectType::BOOLEAN;
    case TokenType::배열: return ObjectType::ARRAY;
    case TokenType::사전: return ObjectType::HASH_MAP;
    case TokenType::함수: return ObjectType::FUNCTION;
    default: return ObjectType::NULL_OBJ; // 클래스 등 알 수 없는 타입
    }
}

// ObjectType → 한글 타입명
inline std::string typeToKorean(ObjectType ot) {
    switch (ot) {
    case ObjectType::INTEGER: return "정수";
    case ObjectType::FLOAT: return "실수";
    case ObjectType::STRING: return "문자";
    case ObjectType::BOOLEAN: return "논리";
    case ObjectType::ARRAY: return "배열";
    case ObjectType::HASH_MAP: return "사전";
    case ObjectType::FUNCTION: return "함수";
    case ObjectType::NULL_OBJ: return "없음";
    case ObjectType::INSTANCE: return "인스턴스";
    case ObjectType::CLASS_DEF: return "클래스";
    case ObjectType::RETURN_VALUE: return "반환값";
    case ObjectType::BUILTIN_FUNCTION: return "내장함수";
    case ObjectType::TUPLE: return "튜플";
    case ObjectType::ITERATOR: return "이터레이터";
    case ObjectType::GENERATOR: return "제너레이터";
    case ObjectType::BREAK_SIGNAL: return "중단";
    case ObjectType::CONTINUE_SIGNAL: return "계속";
    }
    return "알 수 없음";
}

// OpCode → 연산자 기호
inline std::string opToSymbol(OpCode op) {
    switch (op) {
    case OpCode::OP_ADD: return "+";
    case OpCode::OP_SUB: return "-";
    case OpCode::OP_MUL: return "*";
    case OpCode::OP_DIV: return "/";
    case OpCode::OP_MOD: return "%";
    case OpCode::OP_POW: return "**";
    case OpCode::OP_BITWISE_AND: return "&";
    case OpCode::OP_BITWISE_OR: return "|";
    case OpCode::OP_EQUAL: return "==";
    case OpCode::OP_NOT_EQUAL: return "!=";
    case OpCode::OP_LESS: return "<";
    case OpCode::OP_GREATER: return ">";
    case OpCode::OP_LESS_EQUAL: return "<=";
    case OpCode::OP_GREATER_EQUAL: return ">=";
    case OpCode::OP_AND: return "&&";
    case OpCode::OP_OR: return "||";
    default: return "?";
    }
}

// ValueTag → 한글 타입명 (VM용)
inline std::string valueTagToKorean(uint8_t tag) {
    // ValueTag enum values: INT=0, FLOAT=1, BOOL=2, NULL_V=3, OBJECT=4
    switch (tag) {
    case 0: return "정수";
    case 1: return "실수";
    case 2: return "논리";
    case 3: return "없음";
    default: return "객체";
    }
}

#endif // TYPE_UTILS_H
