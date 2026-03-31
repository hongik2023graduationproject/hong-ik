#include "vm.h"
#include "compiler.h"
#include "../environment/environment.h"
#include "../exception/exception.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include "../util/type_utils.h"
#include "../util/utf8_utils.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

VM::VM() {
    stack.reserve(STACK_MAX);
    builtins = {
        {"길이", make_shared<Length>()},
        {"출력", make_shared<Print>()},
        {"추가", make_shared<Push>()},
        {"타입", make_shared<TypeOf>()},
        {"정수변환", make_shared<ToInteger>()},
        {"실수변환", make_shared<ToFloat>()},
        {"문자변환", make_shared<ToString_>()},
        {"입력", make_shared<Input>()},
        {"키목록", make_shared<Keys>()},
        {"포함", make_shared<Contains>()},
        {"설정", make_shared<MapSet>()},
        {"삭제", make_shared<Remove>()},
        {"파일읽기", make_shared<FileRead>()},
        {"파일쓰기", make_shared<FileWrite>()},
        {"절대값", make_shared<Abs>()},
        {"제곱근", make_shared<Sqrt>()},
        {"최대", make_shared<Max>()},
        {"최소", make_shared<Min>()},
        {"난수", make_shared<Random>()},
        {"사인", make_shared<Sin>()},
        {"코사인", make_shared<Cos>()},
        {"탄젠트", make_shared<Tan>()},
        {"로그", make_shared<Log>()},
        {"자연로그", make_shared<Ln>()},
        {"거듭제곱", make_shared<Power>()},
        {"파이", make_shared<Pi>()},
        {"자연수e", make_shared<EulerE>()},
        {"반올림", make_shared<Round>()},
        {"올림", make_shared<Ceil>()},
        {"내림", make_shared<Floor>()},
        {"분리", make_shared<Split>()},
        {"대문자", make_shared<ToUpper>()},
        {"소문자", make_shared<ToLower>()},
        {"치환", make_shared<Replace>()},
        {"자르기", make_shared<Trim>()},
        {"시작확인", make_shared<StartsWith>()},
        {"끝확인", make_shared<EndsWith>()},
        {"반복", make_shared<Repeat>()},
        {"채우기", make_shared<Pad>()},
        {"부분문자", make_shared<Substring>()},
        {"정렬", make_shared<Sort>()},
        {"뒤집기", make_shared<Reverse>()},
        {"찾기", make_shared<Find>()},
        {"조각", make_shared<Slice>()},
        {"JSON_파싱", make_shared<JsonParse>()},
        {"JSON_직렬화", make_shared<JsonSerialize>()},
    };
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

CallFrame& VM::currentFrame() {
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
    frame.function = topLevel.get();
    frame.ip = 0;
    frame.slotOffset = 0;
    frames.push_back(frame);
    return run();
}

static ObjectType vmValueToObjectType(const VMValue& val) {
    switch (val.tag) {
    case ValueTag::INT: return ObjectType::INTEGER;
    case ValueTag::FLOAT: return ObjectType::FLOAT;
    case ValueTag::BOOL: return ObjectType::BOOLEAN;
    case ValueTag::NULL_V: return ObjectType::NULL_OBJ;
    case ValueTag::OBJECT:
        if (val.objVal) return val.objVal->type;
        return ObjectType::NULL_OBJ;
    }
    return ObjectType::NULL_OBJ;
}

VMValue VM::binaryOp(OpCode op, const VMValue& left, const VMValue& right, long long line) {
    // Null checks
    if (left.tag == ValueTag::NULL_V || right.tag == ValueTag::NULL_V) {
        if (op == OpCode::OP_EQUAL) return VMValue::Bool(left.tag == ValueTag::NULL_V && right.tag == ValueTag::NULL_V);
        if (op == OpCode::OP_NOT_EQUAL) return VMValue::Bool(!(left.tag == ValueTag::NULL_V && right.tag == ValueTag::NULL_V));
        throw RuntimeException("없음(null) 값에 대해서는 == 와 != 비교만 지원합니다.", line);
    }

    // Int + Int (hot path -- no heap allocation!)
    if (left.tag == ValueTag::INT && right.tag == ValueTag::INT) {
        long long lv = left.intVal, rv = right.intVal;
        switch (op) {
        case OpCode::OP_ADD: return VMValue::Int(lv + rv);
        case OpCode::OP_SUB: return VMValue::Int(lv - rv);
        case OpCode::OP_MUL: return VMValue::Int(lv * rv);
        case OpCode::OP_DIV:
            if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
            return VMValue::Int(lv / rv);
        case OpCode::OP_MOD:
            if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
            return VMValue::Int(lv % rv);
        case OpCode::OP_POW: {
            if (rv < 0) return VMValue::Float(std::pow(static_cast<double>(lv), static_cast<double>(rv)));
            long long result = 1;
            for (long long i = 0; i < rv; i++) result *= lv;
            return VMValue::Int(result);
        }
        case OpCode::OP_BITWISE_AND: return VMValue::Int(lv & rv);
        case OpCode::OP_BITWISE_OR: return VMValue::Int(lv | rv);
        case OpCode::OP_EQUAL: return VMValue::Bool(lv == rv);
        case OpCode::OP_NOT_EQUAL: return VMValue::Bool(lv != rv);
        case OpCode::OP_LESS: return VMValue::Bool(lv < rv);
        case OpCode::OP_GREATER: return VMValue::Bool(lv > rv);
        case OpCode::OP_LESS_EQUAL: return VMValue::Bool(lv <= rv);
        case OpCode::OP_GREATER_EQUAL: return VMValue::Bool(lv >= rv);
        default: break;
        }
    }

    // Float path (one or both are float, or int+float mix)
    double lv = 0, rv = 0;
    bool isFloat = false;
    if (left.tag == ValueTag::FLOAT) { lv = left.floatVal; isFloat = true; }
    else if (left.tag == ValueTag::INT) { lv = static_cast<double>(left.intVal); }
    if (right.tag == ValueTag::FLOAT) { rv = right.floatVal; isFloat = true; }
    else if (right.tag == ValueTag::INT) { rv = static_cast<double>(right.intVal); }

    if (isFloat) {
        switch (op) {
        case OpCode::OP_ADD: return VMValue::Float(lv + rv);
        case OpCode::OP_SUB: return VMValue::Float(lv - rv);
        case OpCode::OP_MUL: return VMValue::Float(lv * rv);
        case OpCode::OP_DIV:
            if (rv == 0.0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
            return VMValue::Float(lv / rv);
        case OpCode::OP_POW: return VMValue::Float(std::pow(lv, rv));
        case OpCode::OP_EQUAL: return VMValue::Bool(lv == rv);
        case OpCode::OP_NOT_EQUAL: return VMValue::Bool(lv != rv);
        case OpCode::OP_LESS: return VMValue::Bool(lv < rv);
        case OpCode::OP_GREATER: return VMValue::Bool(lv > rv);
        case OpCode::OP_LESS_EQUAL: return VMValue::Bool(lv <= rv);
        case OpCode::OP_GREATER_EQUAL: return VMValue::Bool(lv >= rv);
        default: break;
        }
    }

    // Boolean path
    if (left.tag == ValueTag::BOOL && right.tag == ValueTag::BOOL) {
        switch (op) {
        case OpCode::OP_AND: return VMValue::Bool(left.boolVal && right.boolVal);
        case OpCode::OP_OR: return VMValue::Bool(left.boolVal || right.boolVal);
        case OpCode::OP_EQUAL: return VMValue::Bool(left.boolVal == right.boolVal);
        case OpCode::OP_NOT_EQUAL: return VMValue::Bool(left.boolVal != right.boolVal);
        default: break;
        }
    }

    // String path -- need to convert to Object for complex operations
    if (left.tag == ValueTag::OBJECT && right.tag == ValueTag::OBJECT) {
        auto* ls = dynamic_cast<String*>(left.objVal.get());
        auto* rs = dynamic_cast<String*>(right.objVal.get());
        if (ls && rs) {
            switch (op) {
            case OpCode::OP_ADD: return VMValue::Obj(make_shared<String>(ls->value + rs->value));
            case OpCode::OP_EQUAL: return VMValue::Bool(ls->value == rs->value);
            case OpCode::OP_NOT_EQUAL: return VMValue::Bool(ls->value != rs->value);
            default: break;
            }
        }
    }

    throw RuntimeException(
        typeToKorean(vmValueToObjectType(left)) + " " + opToSymbol(op) + " " +
        typeToKorean(vmValueToObjectType(right)) + ": 좌항과 우항의 타입을 연산할 수 없습니다.", line);
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
            case OpCode::OP_CONSTANT: {
                uint16_t idx = readUint16();
                push(VMValue::fromObject(frame.function->constants[idx]));
                break;
            }
            case OpCode::OP_NULL: push(VMValue::Null()); break;
            case OpCode::OP_TRUE: push(VMValue::Bool(true)); break;
            case OpCode::OP_FALSE: push(VMValue::Bool(false)); break;

            case OpCode::OP_ADD: case OpCode::OP_SUB: case OpCode::OP_MUL:
            case OpCode::OP_DIV: case OpCode::OP_MOD: case OpCode::OP_POW:
            case OpCode::OP_BITWISE_AND: case OpCode::OP_BITWISE_OR:
            case OpCode::OP_EQUAL: case OpCode::OP_NOT_EQUAL:
            case OpCode::OP_LESS: case OpCode::OP_GREATER:
            case OpCode::OP_LESS_EQUAL: case OpCode::OP_GREATER_EQUAL:
            case OpCode::OP_AND: case OpCode::OP_OR: {
                auto right = pop();
                auto left = pop();
                push(binaryOp(instruction, left, right, currentLine()));
                break;
            }

            case OpCode::OP_NEGATE: {
                auto operand = pop();
                if (operand.tag == ValueTag::INT)
                    push(VMValue::Int(-operand.intVal));
                else if (operand.tag == ValueTag::FLOAT)
                    push(VMValue::Float(-operand.floatVal));
                else
                    throw RuntimeException("'-' 전위 연산자가 지원되지 않는 타입입니다.", currentLine());
                break;
            }
            case OpCode::OP_NOT: {
                auto operand = pop();
                if (operand.tag == ValueTag::BOOL)
                    push(VMValue::Bool(!operand.boolVal));
                else
                    throw RuntimeException("'!' 전위 연산자가 지원되지 않는 타입입니다.", currentLine());
                break;
            }

            case OpCode::OP_GET_LOCAL: {
                uint16_t slot = readUint16();
                push(stack[frame.slotOffset + slot]);
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint16_t slot = readUint16();
                stack[frame.slotOffset + slot] = peek(0);
                break;
            }
            case OpCode::OP_GET_GLOBAL: {
                uint16_t nameIdx = readUint16();
                auto* name = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                if (!name) throw RuntimeException("잘못된 전역 변수명입니다.", currentLine());
                auto it = globals.find(name->value);
                if (it != globals.end()) { push(it->second); break; }
                auto bit = builtins.find(name->value);
                if (bit != builtins.end()) { push(VMValue::Obj(bit->second)); break; }
                throw RuntimeException("'" + name->value + "' 존재하지 않는 식별자입니다.", currentLine());
            }
            case OpCode::OP_SET_GLOBAL: {
                uint16_t nameIdx = readUint16();
                auto* name = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                globals[name->value] = peek(0);
                break;
            }
            case OpCode::OP_DEFINE_GLOBAL: {
                uint16_t nameIdx = readUint16();
                auto* name = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                auto val = pop();
                // 상속 해결: CompiledClassDef의 부모 참조 설정
                if (val.tag == ValueTag::OBJECT) {
                    if (auto* ccd = dynamic_cast<CompiledClassDef*>(val.objVal.get())) {
                        if (!ccd->parentName.empty() && !ccd->parent) {
                            auto parentIt = globals.find(ccd->parentName);
                            if (parentIt != globals.end() && parentIt->second.tag == ValueTag::OBJECT) {
                                auto parentCcd = dynamic_pointer_cast<CompiledClassDef>(parentIt->second.objVal);
                                if (parentCcd) {
                                    ccd->parent = parentCcd;
                                    // 부모 필드 병합
                                    vector<string> mergedFieldNames = parentCcd->fieldNames;
                                    vector<shared_ptr<Token>> mergedFieldTypes = parentCcd->fieldTypes;
                                    for (size_t i = 0; i < ccd->fieldNames.size(); i++) {
                                        bool found = false;
                                        for (size_t j = 0; j < mergedFieldNames.size(); j++) {
                                            if (mergedFieldNames[j] == ccd->fieldNames[i]) {
                                                mergedFieldTypes[j] = ccd->fieldTypes[i];
                                                found = true;
                                                break;
                                            }
                                        }
                                        if (!found) {
                                            mergedFieldNames.push_back(ccd->fieldNames[i]);
                                            mergedFieldTypes.push_back(ccd->fieldTypes[i]);
                                        }
                                    }
                                    ccd->fieldNames = mergedFieldNames;
                                    ccd->fieldTypes = mergedFieldTypes;
                                    // 부모 생성자 상속 (자식에 없으면)
                                    if (!ccd->compiledConstructor && parentCcd->compiledConstructor) {
                                        ccd->compiledConstructor = parentCcd->compiledConstructor;
                                    }
                                    // 부모 메서드 병합
                                    for (auto& [mname, mfn] : parentCcd->compiledMethods) {
                                        if (ccd->compiledMethods.find(mname) == ccd->compiledMethods.end()) {
                                            ccd->compiledMethods[mname] = mfn;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                globals[name->value] = val;
                break;
            }

            case OpCode::OP_JUMP: {
                uint16_t offset = readUint16();
                currentFrame().ip += offset;
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = readUint16();
                auto condition = pop();
                if (condition.tag != ValueTag::BOOL) {
                    throw RuntimeException("조건식의 결과는 논리(참/거짓) 값이어야 합니다.", currentLine());
                }
                if (!condition.boolVal) {
                    currentFrame().ip += offset;
                }
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = readUint16();
                currentFrame().ip -= offset;
                break;
            }

            case OpCode::OP_CALL: {
                uint8_t argCount = readByte();
                auto& callee = peek(argCount);

                if (callee.tag == ValueTag::OBJECT && dynamic_cast<Builtin*>(callee.objVal.get())) {
                    auto* builtin = dynamic_cast<Builtin*>(callee.objVal.get());
                    vector<shared_ptr<Object>> args(argCount);
                    for (int i = 0; i < argCount; i++) {
                        args[i] = peek(argCount - 1 - i).toObject();
                    }
                    for (int i = 0; i < argCount; i++) pop();
                    pop(); // callee
                    auto result = builtin->function(args);
                    push(result ? VMValue::fromObject(result) : VMValue::Null());
                } else if (callee.tag == ValueTag::OBJECT && dynamic_cast<Closure*>(callee.objVal.get())) {
                    auto* cls = dynamic_cast<Closure*>(callee.objVal.get());
                    auto* fn = cls->function.get();
                    fillDefaults(fn, argCount);
                    // 매개변수 타입 체크
                    if (!fn->paramTypeChecks.empty()) {
                        for (size_t i = 0; i < fn->paramTypeChecks.size() && i < static_cast<size_t>(fn->arity); i++) {
                            if (fn->paramTypeChecks[i] == ObjectType::NULL_OBJ) continue;
                            auto& arg = stack[stack.size() - fn->arity + i];
                            ObjectType argType = vmValueToObjectType(arg);
                            if (argType == ObjectType::NULL_OBJ) {
                                if (i < fn->paramOptionals.size() && fn->paramOptionals[i]) continue;
                                throw RuntimeException("함수 인자 타입 오류: " + to_string(i + 1) + "번째 인자 (선택적 타입이 아닌 매개변수에 '없음' 전달)", currentLine());
                            }
                            if (argType != fn->paramTypeChecks[i]) {
                                throw RuntimeException("함수 인자 타입 오류: " + to_string(i + 1) + "번째 인자 ("
                                    + typeToKorean(fn->paramTypeChecks[i]) + " 필요, "
                                    + typeToKorean(argType) + " 전달)", currentLine());
                            }
                        }
                    }
                    if (frames.size() >= FRAMES_MAX) {
                        throw RuntimeException("최대 호출 프레임 수(" + to_string(FRAMES_MAX) + ")를 초과했습니다.", currentLine());
                    }
                    CallFrame newFrame;
                    newFrame.function = fn;
                    newFrame.ip = 0;
                    newFrame.slotOffset = stack.size() - fn->arity;
                    newFrame.closure = cls;
                    newFrame.isGenerator = fn->hasYield;
                    frames.push_back(newFrame);
                } else if (callee.tag == ValueTag::OBJECT && dynamic_cast<CompiledFunction*>(callee.objVal.get())) {
                    auto* fn = dynamic_cast<CompiledFunction*>(callee.objVal.get());
                    fillDefaults(fn, argCount);
                    // 매개변수 타입 체크
                    if (!fn->paramTypeChecks.empty()) {
                        for (size_t i = 0; i < fn->paramTypeChecks.size() && i < static_cast<size_t>(fn->arity); i++) {
                            if (fn->paramTypeChecks[i] == ObjectType::NULL_OBJ) continue;
                            auto& arg = stack[stack.size() - fn->arity + i];
                            ObjectType argType = vmValueToObjectType(arg);
                            if (argType == ObjectType::NULL_OBJ) {
                                if (i < fn->paramOptionals.size() && fn->paramOptionals[i]) continue;
                                throw RuntimeException("함수 인자 타입 오류: " + to_string(i + 1) + "번째 인자 (선택적 타입이 아닌 매개변수에 '없음' 전달)", currentLine());
                            }
                            if (argType != fn->paramTypeChecks[i]) {
                                throw RuntimeException("함수 인자 타입 오류: " + to_string(i + 1) + "번째 인자 ("
                                    + typeToKorean(fn->paramTypeChecks[i]) + " 필요, "
                                    + typeToKorean(argType) + " 전달)", currentLine());
                            }
                        }
                    }
                    if (frames.size() >= FRAMES_MAX) {
                        throw RuntimeException("최대 호출 프레임 수(" + to_string(FRAMES_MAX) + ")를 초과했습니다.", currentLine());
                    }
                    CallFrame newFrame;
                    newFrame.function = fn;
                    newFrame.ip = 0;
                    newFrame.slotOffset = stack.size() - fn->arity;
                    newFrame.isGenerator = fn->hasYield;
                    frames.push_back(newFrame);
                } else if (callee.tag == ValueTag::OBJECT && dynamic_cast<CompiledClassDef*>(callee.objVal.get())) {
                    auto* classDef = dynamic_cast<CompiledClassDef*>(callee.objVal.get());
                    auto instance = make_shared<Instance>();
                    instance->classDef = dynamic_pointer_cast<ClassDef>(callee.objVal);
                    instance->fields = make_shared<Environment>();
                    for (auto& fieldName : classDef->fieldNames) {
                        instance->fields->Set(fieldName, make_shared<Null>());
                    }

                    if (classDef->compiledConstructor) {
                        auto* ctorFn = classDef->compiledConstructor.get();
                        fillDefaults(ctorFn, argCount);
                        // 스택: [classDef, arg1, arg2, ..., defaults...]
                        // 생성자 로컬: [자기(slot0), param1, param2, ...]
                        stack[stack.size() - ctorFn->arity - 1] = VMValue::Obj(instance);

                        CallFrame ctorFrame;
                        ctorFrame.function = ctorFn;
                        ctorFrame.ip = 0;
                        ctorFrame.slotOffset = stack.size() - ctorFn->arity - 1;
                        frames.push_back(ctorFrame);
                    } else {
                        for (int i = 0; i < argCount; i++) pop();
                        pop();
                        push(VMValue::Obj(instance));
                    }
                } else {
                    throw RuntimeException("호출할 수 없는 객체입니다.", currentLine());
                }
                break;
            }

            case OpCode::OP_RETURN: {
                auto result = pop();
                // 반환 타입 체크 (제너레이터 함수는 제외)
                auto& retFrame = currentFrame();
                if (retFrame.function->returnTypeCheck != ObjectType::NULL_OBJ && !retFrame.isGenerator) {
                    ObjectType resultType = vmValueToObjectType(result);
                    if (resultType == ObjectType::NULL_OBJ) {
                        if (!retFrame.function->returnTypeOptional) {
                            throw RuntimeException("함수 반환 타입과 실제 반환의 타입이 일치하지 않습니다. ("
                                + typeToKorean(retFrame.function->returnTypeCheck) + " 필요, 없음 반환)", currentLine());
                        }
                    } else if (resultType != retFrame.function->returnTypeCheck) {
                        throw RuntimeException("함수 반환 타입과 실제 반환의 타입이 일치하지 않습니다. ("
                            + typeToKorean(retFrame.function->returnTypeCheck) + " 필요, "
                            + typeToKorean(resultType) + " 반환)", currentLine());
                    }
                }
                size_t slotOffset = currentFrame().slotOffset;
                bool hasCallee = currentFrame().hasCallee;
                bool wasGenerator = currentFrame().isGenerator;
                auto yieldValues = std::move(currentFrame().yieldBuffer);
                frames.pop_back();

                // 현재 프레임의 로컬 스택 정리
                while (stack.size() > slotOffset) {
                    stack.pop_back();
                }

                if (frames.empty()) {
                    if (wasGenerator) {
                        auto arr = make_shared<Array>();
                        for (auto& yv : yieldValues) {
                            arr->elements.push_back(yv.toObject());
                        }
                        return arr;
                    }
                    return result.toObject();
                }

                // OP_CALL로 호출된 경우 callee가 slotOffset-1에 있으므로 제거
                if (hasCallee && !stack.empty() && stack.size() > currentFrame().slotOffset) {
                    stack.pop_back(); // callee 제거
                }

                if (wasGenerator) {
                    auto arr = make_shared<Array>();
                    for (auto& yv : yieldValues) {
                        arr->elements.push_back(yv.toObject());
                    }
                    push(VMValue::Obj(arr));
                } else {
                    push(result);
                }
                break;
            }

            case OpCode::OP_BUILD_ARRAY: {
                uint16_t count = readUint16();
                auto array = make_shared<Array>();
                array->elements.reserve(count);
                for (uint16_t i = 0; i < count; i++) {
                    array->elements.push_back(pop().toObject());
                }
                std::reverse(array->elements.begin(), array->elements.end());
                push(VMValue::Obj(array));
                break;
            }
            case OpCode::OP_BUILD_HASHMAP: {
                uint16_t count = readUint16();
                auto hashmap = make_shared<HashMap>();
                vector<pair<shared_ptr<Object>, shared_ptr<Object>>> pairs;
                for (uint16_t i = 0; i < count; i++) {
                    auto value = pop().toObject();
                    auto key = pop().toObject();
                    pairs.emplace_back(std::move(key), std::move(value));
                }
                for (auto it = pairs.rbegin(); it != pairs.rend(); ++it) {
                    auto* strKey = dynamic_cast<String*>(it->first.get());
                    if (!strKey) throw RuntimeException("사전의 키는 문자열이어야 합니다.", currentLine());
                    hashmap->pairs[strKey->value] = it->second;
                }
                push(VMValue::Obj(hashmap));
                break;
            }
            case OpCode::OP_BUILD_TUPLE: {
                uint16_t count = readUint16();
                auto tuple = make_shared<Tuple>();
                for (uint16_t i = 0; i < count; i++) {
                    tuple->elements.insert(tuple->elements.begin(), pop().toObject());
                }
                push(VMValue::Obj(tuple));
                break;
            }

            case OpCode::OP_INDEX_GET: {
                auto index = pop();
                auto collection = pop();
                // Convert to objects for complex operations
                auto indexObj = index.toObject();
                auto collectionObj = collection.toObject();
                if (auto* arr = dynamic_cast<Array*>(collectionObj.get())) {
                    auto* idx = dynamic_cast<Integer*>(indexObj.get());
                    if (!idx) throw RuntimeException("배열 인덱스는 정수여야 합니다.", currentLine());
                    long long actualIdx = idx->value;
                    if (actualIdx < 0) actualIdx = static_cast<long long>(arr->elements.size()) + actualIdx;
                    if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
                        throw RuntimeException("배열의 범위 밖 인덱스입니다.", currentLine());
                    push(VMValue::fromObject(arr->elements[actualIdx]));
                } else if (auto* str = dynamic_cast<String*>(collectionObj.get())) {
                    auto* idx = dynamic_cast<Integer*>(indexObj.get());
                    if (!idx) throw RuntimeException("문자열 인덱스는 정수여야 합니다.", currentLine());
                    const auto& cps = str->codePoints();
                    long long len = static_cast<long long>(cps.size());
                    long long actualIdx = idx->value;
                    if (actualIdx < 0) actualIdx = len + actualIdx;
                    if (actualIdx < 0 || actualIdx >= len)
                        throw RuntimeException("문자열의 범위 밖 인덱스입니다.", currentLine());
                    push(VMValue::Obj(make_shared<String>(cps[actualIdx])));
                } else if (auto* hm = dynamic_cast<HashMap*>(collectionObj.get())) {
                    auto* key = dynamic_cast<String*>(indexObj.get());
                    if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", currentLine());
                    auto it = hm->pairs.find(key->value);
                    if (it == hm->pairs.end())
                        throw RuntimeException("사전에 키가 존재하지 않습니다.", currentLine());
                    push(VMValue::fromObject(it->second));
                } else if (auto* tup = dynamic_cast<Tuple*>(collectionObj.get())) {
                    auto* idx = dynamic_cast<Integer*>(indexObj.get());
                    if (!idx || idx->value < 0 || idx->value >= static_cast<long long>(tup->elements.size()))
                        throw RuntimeException("튜플의 범위 밖 인덱스입니다.", currentLine());
                    push(VMValue::fromObject(tup->elements[idx->value]));
                } else {
                    throw RuntimeException("인덱스 연산이 지원되지 않는 형식입니다.", currentLine());
                }
                break;
            }

            case OpCode::OP_INDEX_SET: {
                auto value = pop();
                auto index = pop();
                auto collection = pop();
                auto valueObj = value.toObject();
                auto indexObj = index.toObject();
                auto collectionObj = collection.toObject();
                if (auto* arr = dynamic_cast<Array*>(collectionObj.get())) {
                    auto* idx = dynamic_cast<Integer*>(indexObj.get());
                    if (!idx) throw RuntimeException("배열 인덱스는 정수여야 합니다.", currentLine());
                    long long actualIdx = idx->value;
                    if (actualIdx < 0) actualIdx += static_cast<long long>(arr->elements.size());
                    if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
                        throw RuntimeException("배열의 범위 밖 인덱스입니다.", currentLine());
                    arr->elements[actualIdx] = valueObj;
                } else if (auto* hm = dynamic_cast<HashMap*>(collectionObj.get())) {
                    auto* key = dynamic_cast<String*>(indexObj.get());
                    if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", currentLine());
                    hm->pairs[key->value] = valueObj;
                } else {
                    throw RuntimeException("인덱스 대입이 지원되지 않는 형식입니다.", currentLine());
                }
                push(value);
                break;
            }

            case OpCode::OP_SLICE: {
                uint8_t flags = readByte();
                bool hasStart = flags & 0x01;
                bool hasEnd = flags & 0x02;

                shared_ptr<Object> endObj = hasEnd ? pop().toObject() : nullptr;
                shared_ptr<Object> startObj = hasStart ? pop().toObject() : nullptr;
                auto collectionObj = pop().toObject();

                if (auto* arr = dynamic_cast<Array*>(collectionObj.get())) {
                    long long len = static_cast<long long>(arr->elements.size());
                    long long s = 0, e = len;

                    if (startObj) {
                        auto* si = dynamic_cast<Integer*>(startObj.get());
                        if (!si) throw RuntimeException("슬라이스 시작 인덱스는 정수여야 합니다.", currentLine());
                        s = si->value;
                        if (s < 0) s = len + s;
                        if (s < 0) s = 0;
                    }
                    if (endObj) {
                        auto* ei = dynamic_cast<Integer*>(endObj.get());
                        if (!ei) throw RuntimeException("슬라이스 끝 인덱스는 정수여야 합니다.", currentLine());
                        e = ei->value;
                        if (e < 0) e = len + e;
                        if (e > len) e = len;
                    }
                    auto result = make_shared<Array>();
                    for (long long i = s; i < e; i++) {
                        result->elements.push_back(arr->elements[i]);
                    }
                    push(VMValue::Obj(result));
                } else if (auto* str = dynamic_cast<String*>(collectionObj.get())) {
                    long long len = static_cast<long long>(utf8::codePointCount(str->value));
                    long long s = 0, e = len;

                    if (startObj) {
                        auto* si = dynamic_cast<Integer*>(startObj.get());
                        if (!si) throw RuntimeException("슬라이스 시작 인덱스는 정수여야 합니다.", currentLine());
                        s = si->value;
                        if (s < 0) s = len + s;
                        if (s < 0) s = 0;
                    }
                    if (endObj) {
                        auto* ei = dynamic_cast<Integer*>(endObj.get());
                        if (!ei) throw RuntimeException("슬라이스 끝 인덱스는 정수여야 합니다.", currentLine());
                        e = ei->value;
                        if (e < 0) e = len + e;
                        if (e > len) e = len;
                    }
                    if (s >= e) {
                        push(VMValue::Obj(make_shared<String>("")));
                    } else {
                        push(VMValue::Obj(make_shared<String>(utf8::substringCodePoints(str->value, static_cast<size_t>(s), static_cast<size_t>(e)))));
                    }
                } else {
                    throw RuntimeException("슬라이스 연산이 지원되지 않는 형식입니다.", currentLine());
                }
                break;
            }

            case OpCode::OP_GET_MEMBER: {
                uint16_t nameIdx = readUint16();
                auto* memberName = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                auto obj = pop();
                if (obj.tag == ValueTag::OBJECT) {
                    if (auto* inst = dynamic_cast<Instance*>(obj.objVal.get())) {
                        auto val = inst->fields->Get(memberName->value);
                        if (val) { push(VMValue::fromObject(val)); break; }
                        throw RuntimeException("인스턴스에 '" + memberName->value + "' 필드가 존재하지 않습니다.", currentLine());
                    }
                }
                throw RuntimeException("멤버 접근은 인스턴스에서만 사용할 수 있습니다.", currentLine());
            }
            case OpCode::OP_SET_MEMBER: {
                uint16_t nameIdx = readUint16();
                auto* memberName = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                auto instance = pop();
                auto& value = peek(0);
                if (instance.tag == ValueTag::OBJECT) {
                    if (auto* inst = dynamic_cast<Instance*>(instance.objVal.get())) {
                        inst->fields->Set(memberName->value, value.toObject());
                        break;
                    }
                }
                throw RuntimeException("멤버 설정은 인스턴스에서만 사용할 수 있습니다.", currentLine());
            }
            case OpCode::OP_INVOKE: {
                uint16_t nameIdx = readUint16();
                uint8_t argCount = readByte();
                auto* methodName = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                auto& obj = peek(argCount);

                // 내장 타입에 대한 메서드 호출 지원
                if (obj.tag == ValueTag::OBJECT &&
                    (dynamic_cast<String*>(obj.objVal.get()) || dynamic_cast<Array*>(obj.objVal.get()) || dynamic_cast<HashMap*>(obj.objVal.get()))) {
                    auto bit = builtins.find(methodName->value);
                    if (bit != builtins.end()) {
                        vector<shared_ptr<Object>> args(argCount + 1);
                        for (int i = 0; i < argCount; i++) {
                            args[i + 1] = peek(argCount - 1 - i).toObject();
                        }
                        for (int i = 0; i < argCount; i++) pop();
                        auto target = pop(); // the object itself
                        args[0] = target.toObject();
                        auto result = bit->second->function(args);
                        push(result ? VMValue::fromObject(result) : VMValue::Null());
                        break;
                    }
                }

                if (obj.tag == ValueTag::OBJECT) {
                    if (auto* inst = dynamic_cast<Instance*>(obj.objVal.get())) {
                        auto* ccd = dynamic_cast<CompiledClassDef*>(inst->classDef.get());
                        if (ccd) {
                            auto it = ccd->compiledMethods.find(methodName->value);
                            if (it != ccd->compiledMethods.end()) {
                                auto* methodFn = it->second.get();
                                fillDefaults(methodFn, static_cast<int>(argCount));
                                CallFrame methodFrame;
                                methodFrame.function = methodFn;
                                methodFrame.ip = 0;
                                methodFrame.slotOffset = stack.size() - methodFn->arity - 1;
                                methodFrame.hasCallee = false;
                                frames.push_back(methodFrame);
                                break;
                            }
                            throw RuntimeException("인스턴스에 '" + methodName->value + "' 메서드가 존재하지 않습니다.", currentLine());
                        }
                        throw RuntimeException("VM에서 지원하지 않는 클래스 형식입니다.", currentLine());
                    }
                }
                throw RuntimeException("메서드 호출은 인스턴스에서만 사용할 수 있습니다.", currentLine());
            }

            case OpCode::OP_TRY_BEGIN: {
                uint16_t catchOffset = readUint16();
                ExceptionHandler handler;
                handler.catchIp = currentFrame().ip + catchOffset;
                handler.frameDepth = static_cast<int>(frames.size());
                handler.stackDepth = stack.size();
                exceptionHandlers.push_back(handler);
                break;
            }
            case OpCode::OP_TRY_END: {
                if (!exceptionHandlers.empty()) exceptionHandlers.pop_back();
                break;
            }

            case OpCode::OP_ITER_INIT: {
                auto iterable = pop().toObject();
                auto iter = make_shared<IteratorState>();
                iter->iterable = iterable;
                iter->index = 0;
                if (auto* str = dynamic_cast<String*>(iterable.get())) {
                    iter->codePoints = str->codePoints();
                }
                push(VMValue::Obj(iter));
                break;
            }
            case OpCode::OP_ITER_NEXT: {
                uint16_t exitOffset = readUint16();
                auto& iterVal = peek(0);
                if (iterVal.tag != ValueTag::OBJECT) throw RuntimeException("유효하지 않은 이터레이터입니다.", currentLine());
                auto* iter = dynamic_cast<IteratorState*>(iterVal.objVal.get());
                if (!iter) throw RuntimeException("유효하지 않은 이터레이터입니다.", currentLine());

                bool exhausted = false;
                if (auto* arr = dynamic_cast<Array*>(iter->iterable.get())) {
                    exhausted = iter->index >= arr->elements.size();
                } else if (auto* str = dynamic_cast<String*>(iter->iterable.get())) {
                    exhausted = iter->index >= iter->codePoints.size();
                } else {
                    exhausted = true;
                }
                if (exhausted) currentFrame().ip += exitOffset;
                break;
            }
            case OpCode::OP_ITER_VALUE: {
                auto& iterVal = peek(0);
                auto* iter = dynamic_cast<IteratorState*>(iterVal.objVal.get());
                if (auto* arr = dynamic_cast<Array*>(iter->iterable.get())) {
                    push(VMValue::fromObject(arr->elements[iter->index]));
                } else if (auto* str = dynamic_cast<String*>(iter->iterable.get())) {
                    push(VMValue::Obj(make_shared<String>(iter->codePoints[iter->index])));
                }
                iter->index++;
                break;
            }

            case OpCode::OP_CLOSURE: {
                uint16_t constIdx = readUint16();
                auto fn = dynamic_pointer_cast<CompiledFunction>(frame.function->constants[constIdx]);
                auto closure = make_shared<Closure>(fn);

                uint8_t upvalueCount = readByte();
                closure->upvalues.resize(upvalueCount);

                for (uint8_t i = 0; i < upvalueCount; i++) {
                    uint8_t isLocal = readByte();
                    uint16_t index = readUint16();
                    if (isLocal) {
                        // enclosing 함수의 로컬 변수를 캡처
                        auto uv = make_shared<Upvalue>();
                        uv->value = stack[frame.slotOffset + index].toObject();
                        closure->upvalues[i] = uv;
                    } else {
                        // enclosing 함수의 업밸류를 체이닝
                        if (frame.closure) {
                            closure->upvalues[i] = frame.closure->upvalues[index];
                        }
                    }
                }
                push(VMValue::Obj(closure));
                break;
            }
            case OpCode::OP_GET_UPVALUE: {
                uint16_t slot = readUint16();
                if (frame.closure && slot < frame.closure->upvalues.size()) {
                    push(VMValue::fromObject(frame.closure->upvalues[slot]->value));
                } else {
                    push(VMValue::Null());
                }
                break;
            }
            case OpCode::OP_SET_UPVALUE: {
                uint16_t slot = readUint16();
                if (frame.closure && slot < frame.closure->upvalues.size()) {
                    frame.closure->upvalues[slot]->value = peek(0).toObject();
                }
                break;
            }

            case OpCode::OP_POP: pop(); break;
            case OpCode::OP_DUP: push(peek(0)); break;

            case OpCode::OP_RANGE_CHECK: {
                auto endVal = pop();
                auto startVal = pop();
                auto subject = pop();
                bool result = false;
                if (subject.tag == ValueTag::INT &&
                    startVal.tag == ValueTag::INT &&
                    endVal.tag == ValueTag::INT) {
                    auto sv = subject.intVal;
                    auto sv_start = startVal.intVal;
                    auto sv_end = endVal.intVal;
                    result = (sv >= sv_start && sv <= sv_end);
                } else {
                    // float/mixed numeric range
                    auto toDouble = [](const VMValue& v) -> double {
                        if (v.tag == ValueTag::FLOAT) return v.floatVal;
                        if (v.tag == ValueTag::INT) return static_cast<double>(v.intVal);
                        return 0.0;
                    };
                    if ((subject.tag == ValueTag::INT || subject.tag == ValueTag::FLOAT) &&
                        (startVal.tag == ValueTag::INT || startVal.tag == ValueTag::FLOAT) &&
                        (endVal.tag == ValueTag::INT || endVal.tag == ValueTag::FLOAT)) {
                        double sv = toDouble(subject);
                        double sv_start = toDouble(startVal);
                        double sv_end = toDouble(endVal);
                        result = (sv >= sv_start && sv <= sv_end);
                    }
                }
                push(VMValue::Bool(result));
                break;
            }

            case OpCode::OP_TYPE_CHECK: {
                uint16_t typeIdx = readUint16();
                auto subject = pop();
                auto typeNameObj = currentFrame().function->constants[typeIdx];
                string typeName = typeNameObj->ToString();
                bool result = false;
                // Map VMValue tag to ObjectType for comparison
                ObjectType subjectType;
                switch (subject.tag) {
                case ValueTag::INT: subjectType = ObjectType::INTEGER; break;
                case ValueTag::FLOAT: subjectType = ObjectType::FLOAT; break;
                case ValueTag::BOOL: subjectType = ObjectType::BOOLEAN; break;
                case ValueTag::NULL_V: subjectType = ObjectType::NULL_OBJ; break;
                case ValueTag::OBJECT: subjectType = subject.objVal->type; break;
                }
                static const map<string, ObjectType> typeMap = {
                    {"\xEC\xA0\x95\xEC\x88\x98", ObjectType::INTEGER},       // 정수
                    {"\xEC\x8B\xA4\xEC\x88\x98", ObjectType::FLOAT},        // 실수
                    {"\xEB\xAC\xB8\xEC\x9E\x90", ObjectType::STRING},       // 문자
                    {"\xEB\x85\xBC\xEB\xA6\xAC", ObjectType::BOOLEAN},      // 논리
                    {"\xEB\xB0\xB0\xEC\x97\xB4", ObjectType::ARRAY},        // 배열
                    {"\xEC\x82\xAC\xEC\xA0\x84", ObjectType::HASH_MAP},     // 사전
                };
                auto it = typeMap.find(typeName);
                if (it != typeMap.end()) {
                    result = (subjectType == it->second);
                }
                push(VMValue::Bool(result));
                break;
            }

            case OpCode::OP_IMPORT: {
                uint16_t nameIdx = readUint16();
                auto* filename = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                if (!filename) throw RuntimeException("잘못된 파일명입니다.", currentLine());

                // Canonicalize path for dedup
                string canonicalPath;
                try {
                    canonicalPath = std::filesystem::weakly_canonical(filename->value).string();
                } catch (...) {
                    canonicalPath = filename->value;
                }

                // Skip if already imported
                if (importedFiles.count(canonicalPath)) break;
                importedFiles.insert(canonicalPath);

                // Read file
                ifstream importFile(filename->value);
                if (!importFile.is_open()) {
                    throw RuntimeException("파일을 열 수 없습니다: " + filename->value, currentLine());
                }
                stringstream importBuffer;
                importBuffer << importFile.rdbuf();
                string source = importBuffer.str();

                // Lexer -> Parser -> Compiler pipeline
                Lexer importLexer;
                vector<shared_ptr<Token>> allTokens;
                istringstream importStream(source);
                string srcLine;
                int indent = 0;

                while (getline(importStream, srcLine)) {
                    srcLine += "\n";
                    auto utf8chars = Utf8Converter::Convert(srcLine);
                    auto tokens = importLexer.Tokenize(utf8chars);
                    allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());
                    for (auto& t : tokens) {
                        if (t->type == TokenType::START_BLOCK) indent++;
                        if (t->type == TokenType::END_BLOCK) indent--;
                    }
                }
                for (int i = 0; i < indent; i++) {
                    allTokens.push_back(make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
                }

                Parser importParser;
                auto program = importParser.Parsing(allTokens);

                Compiler importCompiler;
                auto importedFn = importCompiler.Compile(program);

                // Store ownership
                importedModules.push_back(importedFn);

                // Execute imported code in current VM context
                CallFrame importFrame;
                importFrame.function = importedFn.get();
                importFrame.ip = 0;
                importFrame.slotOffset = stack.size();
                importFrame.hasCallee = false;
                frames.push_back(importFrame);
                break;
            }

            case OpCode::OP_YIELD: {
                auto value = pop();
                currentFrame().yieldBuffer.push_back(value);
                break;
            }

            case OpCode::OP_INTERPOLATE: {
                uint16_t count = readUint16();
                vector<VMValue> segments;
                for (uint16_t i = 0; i < count; i++) {
                    segments.insert(segments.begin(), pop());
                }
                string result;
                for (auto& seg : segments) result += seg.toString();
                push(VMValue::Obj(make_shared<String>(result)));
                break;
            }

            default:
                throw RuntimeException("알 수 없는 바이트코드 명령입니다: "
                                       + to_string(static_cast<int>(instruction)), currentLine());
            }
        } catch (const RuntimeException& e) {
            if (!exceptionHandlers.empty()) {
                auto handler = exceptionHandlers.back();
                exceptionHandlers.pop_back();
                while (stack.size() > handler.stackDepth) stack.pop_back();
                while (frames.size() > static_cast<size_t>(handler.frameDepth)) frames.pop_back();
                push(VMValue::Obj(make_shared<String>(string(e.what()))));
                currentFrame().ip = handler.catchIp;
                continue;
            }
            throw;
        } catch (const std::exception& e) {
            if (!exceptionHandlers.empty()) {
                auto handler = exceptionHandlers.back();
                exceptionHandlers.pop_back();
                while (stack.size() > handler.stackDepth) stack.pop_back();
                while (frames.size() > static_cast<size_t>(handler.frameDepth)) frames.pop_back();
                push(VMValue::Obj(make_shared<String>(string(e.what()))));
                currentFrame().ip = handler.catchIp;
                continue;
            }
            throw RuntimeException(string(e.what()), currentLine());
        }
    }

    if (!stack.empty()) return pop().toObject();
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
