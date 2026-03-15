#ifndef VM_H
#define VM_H

#include "../object/object.h"
#include "../object/built_in.h"
#include "chunk.h"
#include "opcode.h"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// VM 호출 프레임
struct CallFrame {
    CompiledFunction* function;
    size_t ip;                      // 명령 포인터
    size_t slotOffset;              // 스택 내 로컬 시작 위치
    bool hasCallee = true;          // OP_CALL은 true, OP_INVOKE는 false (callee가 instance로 대체됨)
};

// 예외 핸들러
struct ExceptionHandler {
    size_t catchIp;
    int frameDepth;
    size_t stackDepth;
};

// 이터레이터 상태
struct IteratorState : public Object {
    std::shared_ptr<Object> iterable;
    size_t index = 0;

    IteratorState() { type = ObjectType::NULL_OBJ; }
    std::string ToString() override { return "<이터레이터>"; }
};

class VM {
public:
    VM();

    std::shared_ptr<Object> Execute(std::shared_ptr<CompiledFunction> topLevel);

private:
    static constexpr size_t STACK_MAX = 65536;
    static constexpr size_t FRAMES_MAX = 256;

    std::shared_ptr<CompiledFunction> topLevelFn; // 소유권 유지
    std::vector<std::shared_ptr<Object>> stack;
    std::vector<CallFrame> frames;
    std::map<std::string, std::shared_ptr<Object>> globals;
    std::map<std::string, std::shared_ptr<Builtin>> builtins;
    std::vector<ExceptionHandler> exceptionHandlers;

    // 실행 루프
    std::shared_ptr<Object> run();

    // 스택 연산
    void push(std::shared_ptr<Object> value);
    std::shared_ptr<Object> pop();
    std::shared_ptr<Object>& peek(int distance = 0);

    // 명령 읽기
    uint8_t readByte();
    uint16_t readUint16();
    CallFrame& currentFrame();

    // 이항 연산
    std::shared_ptr<Object> binaryOp(OpCode op, std::shared_ptr<Object> left,
                                      std::shared_ptr<Object> right, long long line);

    // 함수 호출
    bool callValue(std::shared_ptr<Object> callee, int argCount);

    // 에러
    long long currentLine();
};

#endif // VM_H
