#include "peephole.h"

#include <unordered_map>
#include <unordered_set>

namespace {

    // 피연산자 바이트 수. -1 = 가변(OP_CLOSURE), -2 = 알 수 없음.
    int operandBytes(OpCode op) {
        switch (op) {
        case OpCode::OP_CONSTANT:
        case OpCode::OP_GET_LOCAL:
        case OpCode::OP_SET_LOCAL:
        case OpCode::OP_GET_GLOBAL:
        case OpCode::OP_SET_GLOBAL:
        case OpCode::OP_DEFINE_GLOBAL:
        case OpCode::OP_JUMP:
        case OpCode::OP_JUMP_IF_FALSE:
        case OpCode::OP_LOOP:
        case OpCode::OP_GET_UPVALUE:
        case OpCode::OP_SET_UPVALUE:
        case OpCode::OP_BUILD_ARRAY:
        case OpCode::OP_BUILD_HASHMAP:
        case OpCode::OP_BUILD_TUPLE:
        case OpCode::OP_GET_MEMBER:
        case OpCode::OP_SET_MEMBER:
        case OpCode::OP_TRY_BEGIN:
        case OpCode::OP_ITER_NEXT:
        case OpCode::OP_INTERPOLATE:
        case OpCode::OP_TYPE_CHECK:
        case OpCode::OP_DECL_CHECK:
        case OpCode::OP_IMPORT:
        case OpCode::OP_SET_LOCAL_POP:
        case OpCode::OP_SET_GLOBAL_POP:
        case OpCode::OP_SET_UPVALUE_POP:
        case OpCode::OP_SET_MEMBER_POP:
            return 2;
        case OpCode::OP_CALL:
        case OpCode::OP_SLICE:
            return 1;
        case OpCode::OP_INVOKE:
            return 3; // uint16 nameIdx + uint8 argCount
        case OpCode::OP_CLOSURE:
            return -1; // uint16 constIdx + uint8 count + count * (uint8 + uint16)
        case OpCode::OP_NULL:
        case OpCode::OP_TRUE:
        case OpCode::OP_FALSE:
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
        case OpCode::OP_NEGATE:
        case OpCode::OP_NOT:
        case OpCode::OP_RETURN:
        case OpCode::OP_INDEX_GET:
        case OpCode::OP_INDEX_SET:
        case OpCode::OP_TRY_END:
        case OpCode::OP_ITER_INIT:
        case OpCode::OP_ITER_VALUE:
        case OpCode::OP_POP:
        case OpCode::OP_DUP:
        case OpCode::OP_RANGE_CHECK: // opRangeCheck는 read* 호출 없음 — 피연산자 0
        case OpCode::OP_ASSERT_BOOL:
        case OpCode::OP_YIELD:
            return 0;
        }
        return -2;
    }

    bool isJump(OpCode op) {
        return op == OpCode::OP_JUMP || op == OpCode::OP_JUMP_IF_FALSE || op == OpCode::OP_ITER_NEXT
            || op == OpCode::OP_TRY_BEGIN || op == OpCode::OP_LOOP;
    }

    uint16_t readU16(const std::vector<uint8_t>& code, size_t pos) {
        return static_cast<uint16_t>((static_cast<uint16_t>(code[pos]) << 8) | code[pos + 1]);
    }

    // 점프 명령의 절대 타깃. 모든 점프는 피연산자 뒤 ip 기준 상대 (OP_LOOP만 후방).
    size_t jumpTarget(const std::vector<uint8_t>& code, const peephole::DecodedInstr& in) {
        size_t after = in.pos + 3;
        uint16_t off = readU16(code, in.pos + 1);
        return in.op == OpCode::OP_LOOP ? after - off : after + off;
    }

    bool fusedOpFor(OpCode op, OpCode& fused) {
        switch (op) {
        case OpCode::OP_SET_LOCAL:
            fused = OpCode::OP_SET_LOCAL_POP;
            return true;
        case OpCode::OP_SET_GLOBAL:
            fused = OpCode::OP_SET_GLOBAL_POP;
            return true;
        case OpCode::OP_SET_UPVALUE:
            fused = OpCode::OP_SET_UPVALUE_POP;
            return true;
        case OpCode::OP_SET_MEMBER:
            fused = OpCode::OP_SET_MEMBER_POP;
            return true;
        default:
            return false;
        }
    }

} // namespace

namespace peephole {

    std::vector<DecodedInstr> decode(const std::vector<uint8_t>& code) {
        std::vector<DecodedInstr> out;
        size_t pos = 0;
        while (pos < code.size()) {
            OpCode op = static_cast<OpCode>(code[pos]);
            int ob    = operandBytes(op);
            size_t size;
            if (ob == -1) { // OP_CLOSURE 가변 길이
                if (pos + 4 > code.size()) {
                    return {};
                }
                uint8_t count = code[pos + 3];
                size          = 4 + static_cast<size_t>(count) * 3;
            } else if (ob >= 0) {
                size = 1 + static_cast<size_t>(ob);
            } else {
                return {};
            }
            if (pos + size > code.size()) {
                return {};
            }
            out.push_back({op, pos, size});
            pos += size;
        }
        return out;
    }

    void optimizeChunk(CompiledFunction& fn) {
        auto instrs = decode(fn.code);
        if (instrs.empty()) {
            return;
        }

        // 1) 점프 타깃 절대주소 수집
        std::unordered_set<size_t> targets;
        for (const auto& in : instrs) {
            if (isJump(in.op)) {
                targets.insert(jumpTarget(fn.code, in));
            }
        }

        // 2) 융합 계획: SET_* 직후 OP_POP, 단 POP이 점프 타깃이면 제외
        std::vector<bool> drop(instrs.size(), false);
        std::vector<OpCode> newOp(instrs.size());
        bool changed = false;
        for (size_t i = 0; i < instrs.size(); i++) {
            newOp[i] = instrs[i].op;
        }
        for (size_t i = 0; i + 1 < instrs.size(); i++) {
            OpCode fused;
            if (fusedOpFor(instrs[i].op, fused) && instrs[i + 1].op == OpCode::OP_POP
                && targets.count(instrs[i + 1].pos) == 0) {
                newOp[i]    = fused;
                drop[i + 1] = true;
                changed     = true;
            }
        }
        if (!changed) {
            return;
        }

        // 3) 새 위치 계산 (융합은 길이 불변, POP 1바이트만 탈락)
        std::unordered_map<size_t, size_t> posMap; // 명령 시작 oldPos → newPos
        size_t newLen = 0;
        for (size_t i = 0; i < instrs.size(); i++) {
            if (drop[i]) {
                continue;
            }
            posMap[instrs[i].pos] = newLen;
            newLen += instrs[i].size;
        }
        posMap[fn.code.size()] = newLen; // 코드 끝을 가리키는 전방 점프 대응

        // 4) 재조립: 바이트 복사 + 점프 오프셋·lines 재매핑
        std::vector<uint8_t> newCode;
        std::vector<long long> newLines;
        newCode.reserve(newLen);
        newLines.reserve(newLen);
        for (size_t i = 0; i < instrs.size(); i++) {
            if (drop[i]) {
                continue;
            }
            const auto& in = instrs[i];
            size_t start   = newCode.size();
            newCode.push_back(static_cast<uint8_t>(newOp[i]));
            newLines.push_back(fn.lines[in.pos]);
            for (size_t b = 1; b < in.size; b++) {
                newCode.push_back(fn.code[in.pos + b]);
                newLines.push_back(fn.lines[in.pos + b]);
            }
            if (isJump(in.op)) {
                auto it = posMap.find(jumpTarget(fn.code, in));
                if (it == posMap.end()) {
                    return; // 타깃이 명령 경계가 아님 — fn 미변경 상태로 포기
                }
                size_t after       = start + 3;
                size_t off         = in.op == OpCode::OP_LOOP ? after - it->second : it->second - after;
                newCode[start + 1] = static_cast<uint8_t>((off >> 8) & 0xff);
                newCode[start + 2] = static_cast<uint8_t>(off & 0xff);
            }
        }
        fn.code  = std::move(newCode);
        fn.lines = std::move(newLines);
    }

} // namespace peephole
