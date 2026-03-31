#ifndef VM_H
#define VM_H

#include "../object/object.h"
#include "../object/built_in.h"
#include "chunk.h"
#include "opcode.h"
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
    std::vector<std::shared_ptr<Object>> yieldBuffer;
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

    // REPL 세션용: 전역 상태 접근
    std::map<std::string, std::shared_ptr<Object>>& getGlobals() { return globals; }
    void setGlobals(const std::map<std::string, std::shared_ptr<Object>>& g) { globals = g; }

private:
    static constexpr size_t STACK_MAX = 65536;
    static constexpr size_t FRAMES_MAX = 256;

    std::shared_ptr<CompiledFunction> topLevelFn; // 소유권 유지
    std::vector<std::shared_ptr<Object>> stack;
    std::vector<CallFrame> frames;
    std::map<std::string, std::shared_ptr<Object>> globals;
    std::map<std::string, std::shared_ptr<Builtin>> builtins;
    std::vector<ExceptionHandler> exceptionHandlers;
    std::set<std::string> importedFiles;
    std::vector<std::shared_ptr<CompiledFunction>> importedModules; // keep compiled imports alive

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

    // 기본 매개변수 채우기
    void fillDefaults(CompiledFunction* fn, int argCount);

    // 에러
    long long currentLine();

    // 캐시된 싱글턴
    std::shared_ptr<Object> CACHED_TRUE;
    std::shared_ptr<Object> CACHED_FALSE;
    std::shared_ptr<Object> CACHED_NULL;

    // 정수 풀링
    static constexpr int INT_POOL_MIN = -128;
    static constexpr int INT_POOL_MAX = 127;
    static constexpr int INT_POOL_SIZE = INT_POOL_MAX - INT_POOL_MIN + 1;
    std::shared_ptr<Integer> intPool[INT_POOL_SIZE];
    std::shared_ptr<Object> makeInt(long long val);
};

#endif // VM_H
