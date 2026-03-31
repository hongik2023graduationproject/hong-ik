#include "vm.h"
#include "compiler.h"
#include "../environment/environment.h"
#include "../exception/exception.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
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
    CACHED_TRUE = make_shared<Boolean>(true);
    CACHED_FALSE = make_shared<Boolean>(false);
    CACHED_NULL = make_shared<Null>();
    for (int i = 0; i < INT_POOL_SIZE; i++) {
        intPool[i] = make_shared<Integer>(INT_POOL_MIN + i);
    }
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

shared_ptr<Object> VM::makeInt(long long val) {
    if (val >= INT_POOL_MIN && val <= INT_POOL_MAX) {
        return intPool[val - INT_POOL_MIN];
    }
    return make_shared<Integer>(val);
}

void VM::push(shared_ptr<Object> value) {
    stack.push_back(std::move(value));
}

shared_ptr<Object> VM::pop() {
    if (stack.empty()) {
        throw RuntimeException("VM 스택이 비어있습니다.", 0);
    }
    auto val = std::move(stack.back());
    stack.pop_back();
    return val;
}

shared_ptr<Object>& VM::peek(int distance) {
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
    // 최상위 함수를 globals에도 저장 (소유권 유지)
    topLevelFn = topLevel;
    CallFrame frame;
    frame.function = topLevel.get();
    frame.ip = 0;
    frame.slotOffset = 0;
    frames.push_back(frame);
    return run();
}

shared_ptr<Object> VM::binaryOp(OpCode op, shared_ptr<Object> left, shared_ptr<Object> right, long long line) {
    bool ln = dynamic_cast<Null*>(left.get()) != nullptr;
    bool rn = dynamic_cast<Null*>(right.get()) != nullptr;
    if (ln || rn) {
        if (op == OpCode::OP_EQUAL) return (ln && rn) ? CACHED_TRUE : CACHED_FALSE;
        if (op == OpCode::OP_NOT_EQUAL) return !(ln && rn) ? CACHED_TRUE : CACHED_FALSE;
        throw RuntimeException("없음(null) 값에 대해서는 == 와 != 비교만 지원합니다.", line);
    }

    auto* li = dynamic_cast<Integer*>(left.get());
    auto* ri = dynamic_cast<Integer*>(right.get());
    if (li && ri) {
        long long lv = li->value, rv = ri->value;
        switch (op) {
        case OpCode::OP_ADD: return makeInt(lv + rv);
        case OpCode::OP_SUB: return makeInt(lv - rv);
        case OpCode::OP_MUL: return makeInt(lv * rv);
        case OpCode::OP_DIV:
            if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
            return makeInt(lv / rv);
        case OpCode::OP_MOD:
            if (rv == 0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
            return makeInt(lv % rv);
        case OpCode::OP_POW: {
            if (rv < 0) return make_shared<Float>(std::pow(static_cast<double>(lv), static_cast<double>(rv)));
            long long result = 1;
            for (long long i = 0; i < rv; i++) result *= lv;
            return makeInt(result);
        }
        case OpCode::OP_BITWISE_AND: return makeInt(lv & rv);
        case OpCode::OP_BITWISE_OR: return makeInt(lv | rv);
        case OpCode::OP_EQUAL: return (lv == rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_NOT_EQUAL: return (lv != rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_LESS: return (lv < rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_GREATER: return (lv > rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_LESS_EQUAL: return (lv <= rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_GREATER_EQUAL: return (lv >= rv) ? CACHED_TRUE : CACHED_FALSE;
        default: break;
        }
    }

    double lv = 0, rv = 0;
    bool isFloat = false;
    if (auto* f = dynamic_cast<Float*>(left.get())) { lv = f->value; isFloat = true; }
    else if (li) { lv = static_cast<double>(li->value); }
    if (auto* f = dynamic_cast<Float*>(right.get())) { rv = f->value; isFloat = true; }
    else if (ri) { rv = static_cast<double>(ri->value); }

    if (isFloat) {
        switch (op) {
        case OpCode::OP_ADD: return make_shared<Float>(lv + rv);
        case OpCode::OP_SUB: return make_shared<Float>(lv - rv);
        case OpCode::OP_MUL: return make_shared<Float>(lv * rv);
        case OpCode::OP_DIV:
            if (rv == 0.0) throw RuntimeException("0으로 나눌 수 없습니다.", line);
            return make_shared<Float>(lv / rv);
        case OpCode::OP_POW: return make_shared<Float>(std::pow(lv, rv));
        case OpCode::OP_EQUAL: return (lv == rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_NOT_EQUAL: return (lv != rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_LESS: return (lv < rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_GREATER: return (lv > rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_LESS_EQUAL: return (lv <= rv) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_GREATER_EQUAL: return (lv >= rv) ? CACHED_TRUE : CACHED_FALSE;
        default: break;
        }
    }

    auto* lb = dynamic_cast<Boolean*>(left.get());
    auto* rb = dynamic_cast<Boolean*>(right.get());
    if (lb && rb) {
        switch (op) {
        case OpCode::OP_AND: return (lb->value && rb->value) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_OR: return (lb->value || rb->value) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_EQUAL: return (lb->value == rb->value) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_NOT_EQUAL: return (lb->value != rb->value) ? CACHED_TRUE : CACHED_FALSE;
        default: break;
        }
    }

    auto* ls = dynamic_cast<String*>(left.get());
    auto* rs = dynamic_cast<String*>(right.get());
    if (ls && rs) {
        switch (op) {
        case OpCode::OP_ADD: return make_shared<String>(ls->value + rs->value);
        case OpCode::OP_EQUAL: return (ls->value == rs->value) ? CACHED_TRUE : CACHED_FALSE;
        case OpCode::OP_NOT_EQUAL: return (ls->value != rs->value) ? CACHED_TRUE : CACHED_FALSE;
        default: break;
        }
    }

    throw RuntimeException("좌항과 우항의 타입을 연산할 수 없습니다.", line);
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
                push(frame.function->constants[idx]);
                break;
            }
            case OpCode::OP_NULL: push(CACHED_NULL); break;
            case OpCode::OP_TRUE: push(CACHED_TRUE); break;
            case OpCode::OP_FALSE: push(CACHED_FALSE); break;

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
                if (auto* i = dynamic_cast<Integer*>(operand.get()))
                    push(makeInt(-i->value));
                else if (auto* f = dynamic_cast<Float*>(operand.get()))
                    push(make_shared<Float>(-f->value));
                else
                    throw RuntimeException("'-' 전위 연산자가 지원되지 않는 타입입니다.", currentLine());
                break;
            }
            case OpCode::OP_NOT: {
                auto operand = pop();
                if (auto* b = dynamic_cast<Boolean*>(operand.get()))
                    push(b->value ? CACHED_FALSE : CACHED_TRUE);
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
                if (bit != builtins.end()) { push(bit->second); break; }
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
                if (auto* ccd = dynamic_cast<CompiledClassDef*>(val.get())) {
                    if (!ccd->parentName.empty() && !ccd->parent) {
                        auto parentIt = globals.find(ccd->parentName);
                        if (parentIt != globals.end()) {
                            auto parentCcd = dynamic_pointer_cast<CompiledClassDef>(parentIt->second);
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
                auto* b = dynamic_cast<Boolean*>(condition.get());
                if (!b) {
                    throw RuntimeException("조건식의 결과는 논리(참/거짓) 값이어야 합니다.", currentLine());
                }
                if (!b->value) {
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
                auto callee = peek(argCount);

                if (dynamic_cast<Builtin*>(callee.get())) {
                    vector<shared_ptr<Object>> args(argCount);
                    for (int i = 0; i < argCount; i++) {
                        args[i] = peek(argCount - 1 - i);
                    }
                    for (int i = 0; i < argCount; i++) pop();
                    pop(); // callee
                    auto result = dynamic_cast<Builtin*>(callee.get())->function(args);
                    push(result ? result : CACHED_NULL);
                } else if (auto* cls = dynamic_cast<Closure*>(callee.get())) {
                    auto* fn = cls->function.get();
                    fillDefaults(fn, argCount);
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
                } else if (auto* fn = dynamic_cast<CompiledFunction*>(callee.get())) {
                    fillDefaults(fn, argCount);
                    if (frames.size() >= FRAMES_MAX) {
                        throw RuntimeException("최대 호출 프레임 수(" + to_string(FRAMES_MAX) + ")를 초과했습니다.", currentLine());
                    }
                    CallFrame newFrame;
                    newFrame.function = fn;
                    newFrame.ip = 0;
                    newFrame.slotOffset = stack.size() - fn->arity;
                    newFrame.isGenerator = fn->hasYield;
                    frames.push_back(newFrame);
                } else if (auto* classDef = dynamic_cast<CompiledClassDef*>(callee.get())) {
                    auto instance = make_shared<Instance>();
                    instance->classDef = dynamic_pointer_cast<ClassDef>(callee);
                    instance->fields = make_shared<Environment>();
                    for (auto& fieldName : classDef->fieldNames) {
                        instance->fields->Set(fieldName, make_shared<Null>());
                    }

                    if (classDef->compiledConstructor) {
                        auto* ctorFn = classDef->compiledConstructor.get();
                        fillDefaults(ctorFn, argCount);
                        // 스택: [classDef, arg1, arg2, ..., defaults...]
                        // 생성자 로컬: [자기(slot0), param1, param2, ...]
                        stack[stack.size() - ctorFn->arity - 1] = instance;

                        CallFrame ctorFrame;
                        ctorFrame.function = ctorFn;
                        ctorFrame.ip = 0;
                        ctorFrame.slotOffset = stack.size() - ctorFn->arity - 1;
                        frames.push_back(ctorFrame);
                    } else {
                        for (int i = 0; i < argCount; i++) pop();
                        pop();
                        push(instance);
                    }
                } else {
                    throw RuntimeException("호출할 수 없는 객체입니다.", currentLine());
                }
                break;
            }

            case OpCode::OP_RETURN: {
                auto result = pop();
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
                        arr->elements = std::move(yieldValues);
                        return arr;
                    }
                    return result;
                }

                // OP_CALL로 호출된 경우 callee가 slotOffset-1에 있으므로 제거
                // OP_INVOKE(메서드)는 instance가 slot0으로 이미 정리됨
                if (hasCallee && !stack.empty() && stack.size() > currentFrame().slotOffset) {
                    stack.pop_back(); // callee 제거
                }

                if (wasGenerator) {
                    auto arr = make_shared<Array>();
                    arr->elements = std::move(yieldValues);
                    push(arr);
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
                    array->elements.push_back(pop());
                }
                std::reverse(array->elements.begin(), array->elements.end());
                push(array);
                break;
            }
            case OpCode::OP_BUILD_HASHMAP: {
                uint16_t count = readUint16();
                auto hashmap = make_shared<HashMap>();
                // 스택에서 역순으로 pop하므로, 먼저 벡터에 모은 뒤
                // 역순으로 삽입하여 소스 코드 순서대로 마지막 키가 우선하도록 함
                vector<pair<shared_ptr<Object>, shared_ptr<Object>>> pairs;
                for (uint16_t i = 0; i < count; i++) {
                    auto value = pop();
                    auto key = pop();
                    pairs.emplace_back(std::move(key), std::move(value));
                }
                for (auto it = pairs.rbegin(); it != pairs.rend(); ++it) {
                    auto* strKey = dynamic_cast<String*>(it->first.get());
                    if (!strKey) throw RuntimeException("사전의 키는 문자열이어야 합니다.", currentLine());
                    hashmap->pairs[strKey->value] = it->second;
                }
                push(hashmap);
                break;
            }
            case OpCode::OP_BUILD_TUPLE: {
                uint16_t count = readUint16();
                auto tuple = make_shared<Tuple>();
                for (uint16_t i = 0; i < count; i++) {
                    tuple->elements.insert(tuple->elements.begin(), pop());
                }
                push(tuple);
                break;
            }

            case OpCode::OP_INDEX_GET: {
                auto index = pop();
                auto collection = pop();
                if (auto* arr = dynamic_cast<Array*>(collection.get())) {
                    auto* idx = dynamic_cast<Integer*>(index.get());
                    if (!idx) throw RuntimeException("배열 인덱스는 정수여야 합니다.", currentLine());
                    long long actualIdx = idx->value;
                    if (actualIdx < 0) actualIdx = static_cast<long long>(arr->elements.size()) + actualIdx;
                    if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
                        throw RuntimeException("배열의 범위 밖 인덱스입니다.", currentLine());
                    push(arr->elements[actualIdx]);
                } else if (auto* str = dynamic_cast<String*>(collection.get())) {
                    auto* idx = dynamic_cast<Integer*>(index.get());
                    if (!idx) throw RuntimeException("문자열 인덱스는 정수여야 합니다.", currentLine());
                    const auto& cps = str->codePoints();
                    long long len = static_cast<long long>(cps.size());
                    long long actualIdx = idx->value;
                    if (actualIdx < 0) actualIdx = len + actualIdx;
                    if (actualIdx < 0 || actualIdx >= len)
                        throw RuntimeException("문자열의 범위 밖 인덱스입니다.", currentLine());
                    push(make_shared<String>(cps[actualIdx]));
                } else if (auto* hm = dynamic_cast<HashMap*>(collection.get())) {
                    auto* key = dynamic_cast<String*>(index.get());
                    if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", currentLine());
                    auto it = hm->pairs.find(key->value);
                    if (it == hm->pairs.end())
                        throw RuntimeException("사전에 키가 존재하지 않습니다.", currentLine());
                    push(it->second);
                } else if (auto* tup = dynamic_cast<Tuple*>(collection.get())) {
                    auto* idx = dynamic_cast<Integer*>(index.get());
                    if (!idx || idx->value < 0 || idx->value >= static_cast<long long>(tup->elements.size()))
                        throw RuntimeException("튜플의 범위 밖 인덱스입니다.", currentLine());
                    push(tup->elements[idx->value]);
                } else {
                    throw RuntimeException("인덱스 연산이 지원되지 않는 형식입니다.", currentLine());
                }
                break;
            }

            case OpCode::OP_INDEX_SET: {
                auto value = pop();
                auto index = pop();
                auto collection = pop();
                if (auto* arr = dynamic_cast<Array*>(collection.get())) {
                    auto* idx = dynamic_cast<Integer*>(index.get());
                    if (!idx) throw RuntimeException("배열 인덱스는 정수여야 합니다.", currentLine());
                    long long actualIdx = idx->value;
                    if (actualIdx < 0) actualIdx += static_cast<long long>(arr->elements.size());
                    if (actualIdx < 0 || actualIdx >= static_cast<long long>(arr->elements.size()))
                        throw RuntimeException("배열의 범위 밖 인덱스입니다.", currentLine());
                    arr->elements[actualIdx] = value;
                } else if (auto* hm = dynamic_cast<HashMap*>(collection.get())) {
                    auto* key = dynamic_cast<String*>(index.get());
                    if (!key) throw RuntimeException("사전 키는 문자열이어야 합니다.", currentLine());
                    hm->pairs[key->value] = value;
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

                shared_ptr<Object> endObj = hasEnd ? pop() : nullptr;
                shared_ptr<Object> startObj = hasStart ? pop() : nullptr;
                auto collection = pop();

                if (auto* arr = dynamic_cast<Array*>(collection.get())) {
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
                    push(result);
                } else if (auto* str = dynamic_cast<String*>(collection.get())) {
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
                        push(make_shared<String>(""));
                    } else {
                        push(make_shared<String>(utf8::substringCodePoints(str->value, static_cast<size_t>(s), static_cast<size_t>(e))));
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
                if (auto* inst = dynamic_cast<Instance*>(obj.get())) {
                    auto val = inst->fields->Get(memberName->value);
                    if (val) { push(val); break; }
                    throw RuntimeException("인스턴스에 '" + memberName->value + "' 필드가 존재하지 않습니다.", currentLine());
                }
                throw RuntimeException("멤버 접근은 인스턴스에서만 사용할 수 있습니다.", currentLine());
            }
            case OpCode::OP_SET_MEMBER: {
                uint16_t nameIdx = readUint16();
                auto* memberName = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                auto instance = pop();
                auto value = peek(0);
                if (auto* inst = dynamic_cast<Instance*>(instance.get())) {
                    inst->fields->Set(memberName->value, value);
                    break;
                }
                throw RuntimeException("멤버 설정은 인스턴스에서만 사용할 수 있습니다.", currentLine());
            }
            case OpCode::OP_INVOKE: {
                uint16_t nameIdx = readUint16();
                uint8_t argCount = readByte();
                auto* methodName = dynamic_cast<String*>(frame.function->constants[nameIdx].get());
                auto obj = peek(argCount);

                // 내장 타입에 대한 메서드 호출 지원
                if (dynamic_cast<String*>(obj.get()) || dynamic_cast<Array*>(obj.get()) || dynamic_cast<HashMap*>(obj.get())) {
                    auto bit = builtins.find(methodName->value);
                    if (bit != builtins.end()) {
                        vector<shared_ptr<Object>> args(argCount + 1);
                        for (int i = 0; i < argCount; i++) {
                            args[i + 1] = peek(argCount - 1 - i);
                        }
                        for (int i = 0; i < argCount; i++) pop();
                        auto target = pop(); // the object itself
                        args[0] = target;
                        auto result = bit->second->function(args);
                        push(result ? result : CACHED_NULL);
                        break;
                    }
                }

                if (auto* inst = dynamic_cast<Instance*>(obj.get())) {
                    auto* ccd = dynamic_cast<CompiledClassDef*>(inst->classDef.get());
                    if (ccd) {
                        auto it = ccd->compiledMethods.find(methodName->value);
                        if (it != ccd->compiledMethods.end()) {
                            auto* methodFn = it->second.get();
                            fillDefaults(methodFn, static_cast<int>(argCount));
                            // 스택: [instance, arg1, arg2, ..., defaults...]
                            // 메서드 로컬: [자기, param1, param2, ...]
                            // instance가 자기(slot 0), 별도 callee 슬롯 없음
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
                    // 트리워킹 ClassDef (이전 호환)
                    throw RuntimeException("VM에서 지원하지 않는 클래스 형식입니다.", currentLine());
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
                auto iterable = pop();
                auto iter = make_shared<IteratorState>();
                iter->iterable = iterable;
                iter->index = 0;
                if (auto* str = dynamic_cast<String*>(iterable.get())) {
                    iter->codePoints = str->codePoints();
                }
                push(iter);
                break;
            }
            case OpCode::OP_ITER_NEXT: {
                uint16_t exitOffset = readUint16();
                auto& iterObj = peek(0);
                auto* iter = dynamic_cast<IteratorState*>(iterObj.get());
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
                auto& iterObj = peek(0);
                auto* iter = dynamic_cast<IteratorState*>(iterObj.get());
                if (auto* arr = dynamic_cast<Array*>(iter->iterable.get())) {
                    push(arr->elements[iter->index]);
                } else if (auto* str = dynamic_cast<String*>(iter->iterable.get())) {
                    push(make_shared<String>(iter->codePoints[iter->index]));
                }
                iter->index++;
                break;
            }

            case OpCode::OP_CLOSURE: {
                // Upvalue 객체를 통한 간접 참조로 캡처된 변수의 변경이 클로저 간에 공유됨.
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
                        uv->value = stack[frame.slotOffset + index];
                        closure->upvalues[i] = uv;
                    } else {
                        // enclosing 함수의 업밸류를 체이닝
                        if (frame.closure) {
                            closure->upvalues[i] = frame.closure->upvalues[index];
                        }
                    }
                }
                push(closure);
                break;
            }
            case OpCode::OP_GET_UPVALUE: {
                uint16_t slot = readUint16();
                if (frame.closure && slot < frame.closure->upvalues.size()) {
                    push(frame.closure->upvalues[slot]->value);
                } else {
                    push(make_shared<Null>());
                }
                break;
            }
            case OpCode::OP_SET_UPVALUE: {
                uint16_t slot = readUint16();
                if (frame.closure && slot < frame.closure->upvalues.size()) {
                    frame.closure->upvalues[slot]->value = peek(0);
                }
                break;
            }

            case OpCode::OP_POP: pop(); break;
            case OpCode::OP_DUP: push(peek(0)); break;

            case OpCode::OP_RANGE_CHECK: {
                // stack: ... subject subject start end
                auto endVal = pop();
                auto startVal = pop();
                auto subject = pop(); // the DUP'd subject
                bool result = false;
                if (subject->type == ObjectType::INTEGER &&
                    startVal->type == ObjectType::INTEGER &&
                    endVal->type == ObjectType::INTEGER) {
                    auto sv = dynamic_cast<Integer*>(subject.get())->value;
                    auto sv_start = dynamic_cast<Integer*>(startVal.get())->value;
                    auto sv_end = dynamic_cast<Integer*>(endVal.get())->value;
                    result = (sv >= sv_start && sv <= sv_end);
                } else {
                    // float/mixed numeric range
                    auto toDouble = [](Object* o) -> double {
                        if (o->type == ObjectType::FLOAT) return dynamic_cast<Float*>(o)->value;
                        if (o->type == ObjectType::INTEGER) return static_cast<double>(dynamic_cast<Integer*>(o)->value);
                        return 0.0;
                    };
                    if ((subject->type == ObjectType::INTEGER || subject->type == ObjectType::FLOAT) &&
                        (startVal->type == ObjectType::INTEGER || startVal->type == ObjectType::FLOAT) &&
                        (endVal->type == ObjectType::INTEGER || endVal->type == ObjectType::FLOAT)) {
                        double sv = toDouble(subject.get());
                        double sv_start = toDouble(startVal.get());
                        double sv_end = toDouble(endVal.get());
                        result = (sv >= sv_start && sv <= sv_end);
                    }
                }
                push(make_shared<Boolean>(result));
                break;
            }

            case OpCode::OP_TYPE_CHECK: {
                uint16_t typeIdx = readUint16();
                auto subject = pop(); // the DUP'd subject
                auto typeNameObj = currentFrame().function->constants[typeIdx];
                string typeName = typeNameObj->ToString();
                bool result = false;
                // 타입 이름과 ObjectType 매핑
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
                    result = (subject->type == it->second);
                }
                push(make_shared<Boolean>(result));
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
                vector<shared_ptr<Object>> segments;
                for (uint16_t i = 0; i < count; i++) {
                    segments.insert(segments.begin(), pop());
                }
                string result;
                for (auto& seg : segments) result += seg->ToString();
                push(make_shared<String>(result));
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
                push(make_shared<String>(string(e.what())));
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
                push(make_shared<String>(string(e.what())));
                currentFrame().ip = handler.catchIp;
                continue;
            }
            throw RuntimeException(string(e.what()), currentLine());
        }
    }

    if (!stack.empty()) return pop();
    return make_shared<Null>();
}

void VM::fillDefaults(CompiledFunction* fn, int argCount) {
    int requiredArgs = fn->arity - fn->defaultCount;
    if (argCount < requiredArgs || argCount > fn->arity) {
        throw RuntimeException("함수가 필요한 인자 개수와 입력된 인자 개수가 다릅니다.", currentLine());
    }
    for (int i = argCount; i < fn->arity; i++) {
        if (i < static_cast<int>(fn->defaultValues.size()) && fn->defaultValues[i]) {
            push(fn->defaultValues[i]);
        } else {
            push(make_shared<Null>());
        }
    }
}
