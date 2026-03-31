#ifndef VM_H
#define VM_H

#include "../object/object.h"
#include "../object/built_in.h"
#include "chunk.h"
#include "opcode.h"
#include "vm_value.h"
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// VM 호출 프레임
struct CallFrame {
    CompiledFunction* function;
    size_t ip;                      // 명령 포인터
    size_t slotOffset;              // 스택 내 로컬 시작 위치
    bool hasCallee = true;          // OP_CALL은 true, OP_INVOKE는 false
    Closure* closure = nullptr;     // 클로저인 경우 업밸류 접근용
    bool isGenerator = false;
    std::vector<VMValue> yieldBuffer;
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
    std::vector<std::string> codePoints; // UTF-8 code points for string iteration

    IteratorState() { type = ObjectType::ITERATOR; }
    std::string ToString() override { return "<이터레이터>"; }
};

class VM {
public:
    VM();

    std::shared_ptr<Object> Execute(std::shared_ptr<CompiledFunction> topLevel);

    // REPL 세션용: 전역 상태 접근 (external interface stays shared_ptr based)
    std::map<std::string, std::shared_ptr<Object>> getGlobals() {
        std::map<std::string, std::shared_ptr<Object>> result;
        for (auto& [k, v] : globals) result[k] = v.toObject();
        return result;
    }
    void setGlobals(const std::map<std::string, std::shared_ptr<Object>>& g) {
        globals.clear();
        for (auto& [k, v] : g) globals[k] = VMValue::fromObject(v);
    }

private:
    static constexpr size_t STACK_MAX = 65536;
    static constexpr size_t FRAMES_MAX = 256;

    std::shared_ptr<CompiledFunction> topLevelFn; // 소유권 유지
    std::vector<VMValue> stack;
    std::vector<CallFrame> frames;
    std::map<std::string, VMValue> globals;
    std::map<std::string, std::shared_ptr<Builtin>> builtins;
    std::vector<ExceptionHandler> exceptionHandlers;
    std::set<std::string> importedFiles;
    std::vector<std::shared_ptr<CompiledFunction>> importedModules; // keep compiled imports alive

    // 실행 루프
    std::shared_ptr<Object> run();

    // 스택 연산
    void push(VMValue value);
    VMValue pop();
    VMValue& peek(int distance = 0);

    // 명령 읽기
    uint8_t readByte();
    uint16_t readUint16();
    CallFrame& currentFrame();

    // 이항 연산
    VMValue binaryOp(OpCode op, const VMValue& left, const VMValue& right, long long line);

    // 함수 호출
    bool callValue(VMValue callee, int argCount);

    // 기본 매개변수 채우기
    void fillDefaults(CompiledFunction* fn, int argCount);

    // 에러
    long long currentLine();
};

#endif // VM_H
