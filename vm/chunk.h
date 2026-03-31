#ifndef CHUNK_H
#define CHUNK_H

#include "../object/object.h"
#include "opcode.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct CompiledFunction : public Object {
    std::vector<uint8_t> code;
    std::vector<std::shared_ptr<Object>> constants;
    std::vector<long long> lines;   // lines[i] = 소스 줄 번호 for code[i]

    std::string name;
    int arity = 0;
    int localCount = 0;
    int defaultCount = 0;
    std::vector<std::shared_ptr<Object>> defaultValues; // pre-evaluated constant defaults, nullptr for required params
    bool hasYield = false;

    CompiledFunction() { type = ObjectType::FUNCTION; }

    std::string ToString() override {
        return name.empty() ? "<스크립트>" : "함수: " + name;
    }

    size_t emitByte(uint8_t byte, long long line) {
        code.push_back(byte);
        lines.push_back(line);
        return code.size() - 1;
    }

    size_t emitOp(OpCode op, long long line) {
        return emitByte(static_cast<uint8_t>(op), line);
    }

    size_t emitOpAndUint16(OpCode op, uint16_t operand, long long line) {
        emitByte(static_cast<uint8_t>(op), line);
        emitByte(static_cast<uint8_t>((operand >> 8) & 0xff), line);
        emitByte(static_cast<uint8_t>(operand & 0xff), line);
        return code.size() - 3;
    }

    uint16_t addConstant(std::shared_ptr<Object> value) {
        if (constants.size() >= 65535) {
            throw std::runtime_error("상수 개수가 최대치(65535)를 초과했습니다.");
        }
        constants.push_back(std::move(value));
        return static_cast<uint16_t>(constants.size() - 1);
    }

    // 점프 명령 emit (placeholder 오프셋)
    size_t emitJump(OpCode op, long long line) {
        emitByte(static_cast<uint8_t>(op), line);
        emitByte(0xff, line); // placeholder
        emitByte(0xff, line);
        return code.size() - 2; // 오프셋 위치 반환
    }

    // 점프 오프셋을 현재 위치로 패치
    void patchJump(size_t offset) {
        size_t jump = code.size() - offset - 2;
        code[offset]     = static_cast<uint8_t>((jump >> 8) & 0xff);
        code[offset + 1] = static_cast<uint8_t>(jump & 0xff);
    }

    // 루프 점프 emit (뒤로)
    void emitLoop(size_t loopStart, long long line) {
        emitByte(static_cast<uint8_t>(OpCode::OP_LOOP), line);
        size_t offset = code.size() - loopStart + 2;
        emitByte(static_cast<uint8_t>((offset >> 8) & 0xff), line);
        emitByte(static_cast<uint8_t>(offset & 0xff), line);
    }
};

// 업밸류: 클로저가 캡처한 변수
struct Upvalue {
    std::shared_ptr<Object> value;    // 캡처된 값
};

// 클로저: 함수 + 캡처된 업밸류
// NOTE: Closure와 CompiledFunction이 동일한 ObjectType::FUNCTION 타입을 사용함.
// 이는 트리워킹 인터프리터의 Function과의 호환성을 위한 의도적 설계임.
struct Closure : public Object {
    std::shared_ptr<CompiledFunction> function;
    std::vector<std::shared_ptr<Upvalue>> upvalues;

    Closure(std::shared_ptr<CompiledFunction> fn) : function(std::move(fn)) {
        type = ObjectType::FUNCTION;
    }
    std::string ToString() override {
        return function->name.empty() ? "<클로저>" : "함수: " + function->name;
    }
};

// VM용 클래스 정의 - 컴파일된 생성자/메서드 포함
struct CompiledClassDef : public ClassDef {
    std::string parentName;
    std::shared_ptr<CompiledFunction> compiledConstructor;
    std::map<std::string, std::shared_ptr<CompiledFunction>> compiledMethods;

    CompiledClassDef() { type = ObjectType::CLASS_DEF; }

    std::string ToString() override {
        return "클래스 " + name;
    }
};

#endif // CHUNK_H
