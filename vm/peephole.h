#ifndef PEEPHOLE_H
#define PEEPHOLE_H

#include "chunk.h"
#include <cstddef>
#include <vector>

namespace peephole {

    // 디코드된 명령 하나. size = opcode 1바이트 + 피연산자 바이트.
    struct DecodedInstr {
        OpCode op;
        size_t pos; // 원본 code에서의 시작 오프셋
        size_t size; // 총 바이트 수
    };

    // 바이트코드를 명령 단위로 디코드한다.
    // 알 수 없는 opcode·경계 초과를 만나면 빈 벡터를 반환한다 (호출측은 최적화를 포기 — 안전 우선).
    std::vector<DecodedInstr> decode(const std::vector<uint8_t>& code);

    // SET_{LOCAL,GLOBAL,UPVALUE,MEMBER} + OP_POP → *_POP 융합 후 점프 오프셋·lines를 재조립한다.
    // 이 함수는 fn.code만 다룬다 — 중첩 함수/클래스 재귀는 호출측(Compiler::optimize) 책임.
    void optimizeChunk(CompiledFunction& fn);

} // namespace peephole

#endif // PEEPHOLE_H
