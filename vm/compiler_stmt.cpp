// Compiler — 문장 컴파일 (Phase 3 분할: 코어·표현식은 compiler.cpp / compiler_expr.cpp 참조)
#include "compiler.h"
#include "../exception/exception.h"
#include "../util/type_utils.h"
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std;


void Compiler::compileStatement(Statement* stmt) {
    if (current->isUnreachable) return;

    if (auto* s = dynamic_cast<ExpressionStatement*>(stmt)) {
        compileExpression(s->expression.get());
        chunk().emitOp(OpCode::OP_POP, 0);
        return;
    }
    if (auto* s = dynamic_cast<InitializationStatement*>(stmt)) {
        compileInitialization(s);
        return;
    }
    if (auto* s = dynamic_cast<AssignmentStatement*>(stmt)) {
        compileAssignment(s);
        return;
    }
    if (auto* s = dynamic_cast<CompoundAssignmentStatement*>(stmt)) {
        compileCompoundAssignment(s);
        return;
    }
    if (auto* s = dynamic_cast<ReturnStatement*>(stmt)) {
        compileReturn(s);
        return;
    }
    if (auto* s = dynamic_cast<IfStatement*>(stmt)) {
        compileIf(s);
        return;
    }
    if (auto* s = dynamic_cast<WhileStatement*>(stmt)) {
        compileWhile(s);
        return;
    }
    if (auto* s = dynamic_cast<ForEachStatement*>(stmt)) {
        compileForEach(s);
        return;
    }
    if (auto* s = dynamic_cast<ForRangeStatement*>(stmt)) {
        compileForRange(s);
        return;
    }
    if (dynamic_cast<BreakStatement*>(stmt)) {
        if (!current->loops.empty()) {
            auto& loop = current->loops.back();
            size_t jumpOffset = chunk().emitJump(OpCode::OP_JUMP, 0);
            loop.breakJumps.push_back(jumpOffset);
            current->isUnreachable = true;
        }
        return;
    }
    if (dynamic_cast<ContinueStatement*>(stmt)) {
        if (!current->loops.empty()) {
            auto& loop = current->loops.back();
            if (loop.continueTarget != 0) {
                // For for-range loops, we need a forward jump to the increment section
                size_t jumpOffset = chunk().emitJump(OpCode::OP_JUMP, 0);
                loop.continueJumps.push_back(jumpOffset);
            } else {
                // For while/foreach loops, jump back to loop start
                chunk().emitLoop(loop.loopStart, 0);
            }
            current->isUnreachable = true;
        }
        return;
    }
    if (auto* s = dynamic_cast<FunctionStatement*>(stmt)) {
        compileFunction(s);
        return;
    }
    if (auto* s = dynamic_cast<ClassStatement*>(stmt)) {
        compileClass(s);
        return;
    }
    if (auto* s = dynamic_cast<MatchStatement*>(stmt)) {
        compileMatch(s);
        return;
    }
    if (auto* s = dynamic_cast<TryCatchStatement*>(stmt)) {
        compileTryCatch(s);
        return;
    }
    if (auto* s = dynamic_cast<ImportStatement*>(stmt)) {
        compileImport(s);
        return;
    }
    if (auto* s = dynamic_cast<IndexAssignmentStatement*>(stmt)) {
        compileIndexAssignment(s);
        return;
    }
    if (auto* s = dynamic_cast<YieldStatement*>(stmt)) {
        compileYield(s);
        return;
    }
    if (auto* s = dynamic_cast<BlockStatement*>(stmt)) {
        compileBlock(s);
        return;
    }
}


void Compiler::compileInitialization(InitializationStatement* stmt) {
    long long line = stmt->type ? stmt->type->line : 0;
    compileExpression(stmt->value.get());

    // 선언 타입 검사 (런타임 일관성 D1) — 타입명 상수 + Optional 표기('?')
    std::string typeName = stmt->type ? stmt->type->text : "";
    if (stmt->isOptional) typeName += "?";
    uint16_t typeIdx = identifierConstant(typeName);
    chunk().emitOpAndUint16(OpCode::OP_DECL_CHECK, typeIdx, line);

    if (current->scopeDepth > 0) {
        declareLocal(stmt->name);
    } else {
        uint16_t nameIdx = identifierConstant(stmt->name);
        chunk().emitOpAndUint16(OpCode::OP_DEFINE_GLOBAL, nameIdx, line);
    }
}


void Compiler::compileAssignment(AssignmentStatement* stmt) {
    compileExpression(stmt->value.get());

    // 멤버 대입: 자기.필드
    if (stmt->name.size() > 6 && stmt->name.substr(0, 7) == "자기.") {
        // "자기."는 UTF-8로 9바이트 (자기=6 + .=1... wait)
        // Actually in this codebase, 자기 is Korean UTF-8 chars stored as string
        // The parser stores "자기.필드" as the name
        string fieldName = stmt->name.substr(stmt->name.find('.') + 1);
        // 자기를 스택에 로드
        compileIdentifier("자기", 0);
        uint16_t nameIdx = identifierConstant(fieldName);
        chunk().emitOpAndUint16(OpCode::OP_SET_MEMBER, nameIdx, 0);
        chunk().emitOp(OpCode::OP_POP, 0); // SET_MEMBER이 남긴 값 정리
        return;
    }

    int slot = resolveLocal(current, stmt->name);
    if (slot != -1) {
        chunk().emitOpAndUint16(OpCode::OP_SET_LOCAL, static_cast<uint16_t>(slot), 0);
    } else {
        int uv = resolveUpvalue(current, stmt->name);
        if (uv != -1) {
            chunk().emitOpAndUint16(OpCode::OP_SET_UPVALUE, static_cast<uint16_t>(uv), 0);
        } else {
            uint16_t nameIdx = identifierConstant(stmt->name);
            chunk().emitOpAndUint16(OpCode::OP_SET_GLOBAL, nameIdx, 0);
        }
    }
    // SET_LOCAL/SET_GLOBAL/SET_UPVALUE는 값을 스택에 유지하므로, 문(statement)으로서 pop 필요
    chunk().emitOp(OpCode::OP_POP, 0);
}


void Compiler::compileIndexAssignment(IndexAssignmentStatement* stmt) {
    long long line = 0;
    compileExpression(stmt->collection.get());
    compileExpression(stmt->index.get());
    compileExpression(stmt->value.get());
    chunk().emitOp(OpCode::OP_INDEX_SET, line);
    chunk().emitOp(OpCode::OP_POP, line);
}


void Compiler::compileCompoundAssignment(CompoundAssignmentStatement* stmt) {
    long long line = stmt->op ? stmt->op->line : 0;

    // 현재 값 로드
    compileIdentifier(stmt->name, line);

    // 우항 컴파일
    compileExpression(stmt->value.get());

    // 연산
    switch (stmt->op->type) {
    case TokenType::PLUS_ASSIGN: chunk().emitOp(OpCode::OP_ADD, line); break;
    case TokenType::MINUS_ASSIGN: chunk().emitOp(OpCode::OP_SUB, line); break;
    case TokenType::ASTERISK_ASSIGN: chunk().emitOp(OpCode::OP_MUL, line); break;
    case TokenType::SLASH_ASSIGN: chunk().emitOp(OpCode::OP_DIV, line); break;
    case TokenType::PERCENT_ASSIGN: chunk().emitOp(OpCode::OP_MOD, line); break;
    default: break;
    }

    // 결과 저장
    int slot = resolveLocal(current, stmt->name);
    if (slot != -1) {
        chunk().emitOpAndUint16(OpCode::OP_SET_LOCAL, static_cast<uint16_t>(slot), line);
    } else {
        int uv = resolveUpvalue(current, stmt->name);
        if (uv != -1) {
            chunk().emitOpAndUint16(OpCode::OP_SET_UPVALUE, static_cast<uint16_t>(uv), line);
        } else {
            uint16_t nameIdx = identifierConstant(stmt->name);
            chunk().emitOpAndUint16(OpCode::OP_SET_GLOBAL, nameIdx, line);
        }
    }
    // SET_LOCAL/SET_GLOBAL/SET_UPVALUE는 값을 스택에 유지하므로, 문(statement)으로서 pop 필요
    chunk().emitOp(OpCode::OP_POP, line);
}


void Compiler::compileBlock(BlockStatement* stmt) {
    beginScope();
    current->isUnreachable = false;
    for (auto& s : stmt->statements) {
        compileStatement(s.get());
    }
    endScope(0);
}


void Compiler::compileIf(IfStatement* stmt) {
    compileExpression(stmt->condition.get());

    size_t elseJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

    compileBlock(stmt->consequence.get());

    if (stmt->then) {
        size_t endJump = chunk().emitJump(OpCode::OP_JUMP, 0);
        chunk().patchJump(elseJump);
        compileBlock(stmt->then.get());
        chunk().patchJump(endJump);
    } else {
        chunk().patchJump(elseJump);
    }
    current->isUnreachable = false;
}


void Compiler::compileWhile(WhileStatement* stmt) {
    size_t loopStart = chunk().code.size();
    current->loops.push_back({loopStart, 0, {}, {}, current->scopeDepth});

    compileExpression(stmt->condition.get());
    size_t exitJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

    beginScope();
    current->isUnreachable = false;
    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }
    endScope(0);
    current->isUnreachable = false;

    chunk().emitLoop(loopStart, 0);
    chunk().patchJump(exitJump);

    // break 점프 패치
    auto& loop = current->loops.back();
    for (auto offset : loop.breakJumps) {
        chunk().patchJump(offset);
    }
    current->loops.pop_back();
}


void Compiler::compileForEach(ForEachStatement* stmt) {
    // iterable을 스택에 로드
    compileExpression(stmt->iterable.get());
    chunk().emitOp(OpCode::OP_ITER_INIT, 0);

    // 이터레이터를 로컬로 저장
    beginScope();
    uint16_t iterSlot = declareLocal("__iter__");
    (void)iterSlot;

    size_t loopStart = chunk().code.size();
    current->loops.push_back({loopStart, 0, {}, {}, current->scopeDepth});

    size_t exitJump = chunk().emitJump(OpCode::OP_ITER_NEXT, 0);

    chunk().emitOp(OpCode::OP_ITER_VALUE, 0);
    // 원소 타입 검사 (런타임 일관성 D5 — evaluator의 per-element typeCheck와 합치)
    if (stmt->elementType) {
        chunk().emitOpAndUint16(OpCode::OP_DECL_CHECK, identifierConstant(stmt->elementType->text),
                                stmt->elementType->line);
    }
    uint16_t elemSlot = declareLocal(stmt->elementName);
    (void)elemSlot;

    current->isUnreachable = false;
    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }
    current->isUnreachable = false;

    // 원소 로컬 제거
    chunk().emitOp(OpCode::OP_POP, 0);
    current->locals.pop_back();

    chunk().emitLoop(loopStart, 0);
    chunk().patchJump(exitJump);

    auto& loop = current->loops.back();
    for (auto offset : loop.breakJumps) {
        chunk().patchJump(offset);
    }
    current->loops.pop_back();

    endScope(0);
}


void Compiler::compileForRange(ForRangeStatement* stmt) {
    // 반복 정수 i = start 부터 end 까지:
    beginScope();

    // 시작값을 로컬로 저장 (루프 변수)
    compileExpression(stmt->startExpr.get());
    // 범위 경계는 정수만 (런타임 일관성 — evaluator "반복 범위의 시작과 끝은 정수이어야 합니다"와 합치)
    chunk().emitOpAndUint16(OpCode::OP_DECL_CHECK, identifierConstant("정수"),
                            stmt->varType ? stmt->varType->line : 0);
    uint16_t varSlot = declareLocal(stmt->varName);

    // 종료값을 로컬로 저장
    compileExpression(stmt->endExpr.get());
    chunk().emitOpAndUint16(OpCode::OP_DECL_CHECK, identifierConstant("정수"),
                            stmt->varType ? stmt->varType->line : 0);
    uint16_t endSlot = declareLocal("__end__");
    (void)endSlot;

    size_t loopStart = chunk().code.size();
    // continueTarget = 1 is a sentinel meaning "use continueJumps for forward patching"
    current->loops.push_back({loopStart, 1, {}, {}, current->scopeDepth});

    // 조건: i < end
    chunk().emitOpAndUint16(OpCode::OP_GET_LOCAL, varSlot, 0);
    chunk().emitOpAndUint16(OpCode::OP_GET_LOCAL, static_cast<uint16_t>(varSlot + 1), 0);
    chunk().emitOp(OpCode::OP_LESS, 0);
    size_t exitJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

    // 본문
    beginScope();
    current->isUnreachable = false;
    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }
    endScope(0);
    current->isUnreachable = false;

    // continue 점프 패치: continue는 여기(증분 단계)로 점프해야 함
    auto& loop = current->loops.back();
    for (auto offset : loop.continueJumps) {
        chunk().patchJump(offset);
    }

    // i += 1
    chunk().emitOpAndUint16(OpCode::OP_GET_LOCAL, varSlot, 0);
    emitConstant(make_shared<Integer>(1), 0);
    chunk().emitOp(OpCode::OP_ADD, 0);
    chunk().emitOpAndUint16(OpCode::OP_SET_LOCAL, varSlot, 0);
    chunk().emitOp(OpCode::OP_POP, 0); // SET_LOCAL이 남긴 값 정리

    chunk().emitLoop(loopStart, 0);
    chunk().patchJump(exitJump);

    // break 점프 패치
    for (auto offset : loop.breakJumps) {
        chunk().patchJump(offset);
    }
    current->loops.pop_back();

    endScope(0);
}


void Compiler::compileReturn(ReturnStatement* stmt) {
    compileExpression(stmt->expression.get());
    chunk().emitOp(OpCode::OP_RETURN, 0);
    current->isUnreachable = true;
}


void Compiler::compileFunction(FunctionStatement* stmt) {
    CompilerState funcState;
    funcState.function = make_shared<CompiledFunction>();
    funcState.function->name = stmt->name;
    funcState.function->arity = static_cast<int>(stmt->parameters.size());
    funcState.scopeDepth = 1;
    funcState.enclosing = current;

    CompilerState* enclosing = current;
    current = &funcState;

    for (auto& param : stmt->parameters) {
        declareLocal(param->name);
    }

    // 기본 매개변수 값 사전 평가
    funcState.function->defaultValues.resize(stmt->parameters.size());
    int defaultCount = 0;
    for (size_t i = 0; i < stmt->defaultValues.size(); i++) {
        if (stmt->defaultValues[i]) {
            defaultCount++;
            if (auto* intLit = dynamic_cast<IntegerLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Integer>(intLit->value);
            } else if (auto* floatLit = dynamic_cast<FloatLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Float>(floatLit->value);
            } else if (auto* strLit = dynamic_cast<StringLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<String>(strLit->value);
            } else if (auto* boolLit = dynamic_cast<BooleanLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Boolean>(boolLit->value);
            } else if (dynamic_cast<NullLiteral*>(stmt->defaultValues[i].get())) {
                funcState.function->defaultValues[i] = make_shared<Null>();
            } else {
                throw RuntimeException("VM에서는 상수 기본값만 지원합니다.", 0);
            }
        }
    }
    funcState.function->defaultCount = defaultCount;

    funcState.function->hasYield = containsYield(stmt->body.get());

    // 매개변수 타입 정보 복사
    funcState.function->paramTypeChecks.resize(stmt->parameters.size(), ObjectType::NULL_OBJ);
    funcState.function->paramOptionals.resize(stmt->parameters.size(), false);
    for (size_t i = 0; i < stmt->parameterTypes.size(); i++) {
        if (stmt->parameterTypes[i]) {
            funcState.function->paramTypeChecks[i] = tokenTypeToObjectType(stmt->parameterTypes[i]->type);
        }
        if (i < stmt->parameterOptionals.size()) {
            funcState.function->paramOptionals[i] = stmt->parameterOptionals[i];
        }
    }
    // 반환 타입 정보 복사
    if (stmt->returnType) {
        funcState.function->returnTypeCheck = tokenTypeToObjectType(stmt->returnType->type);
    }
    funcState.function->returnTypeOptional = stmt->returnTypeOptional;

    for (auto& s : stmt->body->statements) {
        compileStatement(s.get());
    }

    chunk().emitOp(OpCode::OP_NULL, 0);
    chunk().emitOp(OpCode::OP_RETURN, 0);

    current->function->localCount = static_cast<int>(current->locals.size());
    auto upvalues = current->upvalues;
    current = enclosing;

    // 업밸류가 있으면 OP_CLOSURE, 없으면 OP_CONSTANT
    uint16_t constIdx = chunk().addConstant(funcState.function);
    if (!upvalues.empty()) {
        chunk().emitOpAndUint16(OpCode::OP_CLOSURE, constIdx, 0);
        // 업밸류 디스크립터
        chunk().emitByte(static_cast<uint8_t>(upvalues.size()), 0);
        for (auto& uv : upvalues) {
            chunk().emitByte(uv.isLocal ? 1 : 0, 0);
            chunk().emitByte(static_cast<uint8_t>((uv.index >> 8) & 0xff), 0);
            chunk().emitByte(static_cast<uint8_t>(uv.index & 0xff), 0);
        }
    } else {
        chunk().emitOpAndUint16(OpCode::OP_CONSTANT, constIdx, 0);
    }

    uint16_t nameIdx = identifierConstant(stmt->name);
    chunk().emitOpAndUint16(OpCode::OP_DEFINE_GLOBAL, nameIdx, 0);
}


void Compiler::compileClass(ClassStatement* stmt) {
    uint16_t nameIdx = identifierConstant(stmt->name);

    // 생성자 컴파일
    shared_ptr<CompiledFunction> constructorFn;
    if (stmt->constructorBody) {
        CompilerState ctorState;
        ctorState.function = make_shared<CompiledFunction>();
        ctorState.function->name = stmt->name + ".생성";
        ctorState.scopeDepth = 1;

        CompilerState* enclosing = current;
        current = &ctorState;

        declareLocal("자기");
        for (auto& param : stmt->constructorParams) {
            declareLocal(param->name);
        }
        current->function->arity = static_cast<int>(stmt->constructorParams.size());

        for (auto& s : stmt->constructorBody->statements) {
            compileStatement(s.get());
        }
        // 생성자는 자기를 반환
        compileIdentifier("자기", 0);
        chunk().emitOp(OpCode::OP_RETURN, 0);
        current->function->localCount = static_cast<int>(current->locals.size());
        constructorFn = ctorState.function;
        current = enclosing;
    }

    // 메서드 컴파일
    map<string, shared_ptr<CompiledFunction>> compiledMethods;
    for (auto& method : stmt->methods) {
        CompilerState methodState;
        methodState.function = make_shared<CompiledFunction>();
        methodState.function->name = stmt->name + "." + method->name;
        methodState.scopeDepth = 1;

        CompilerState* enclosing = current;
        current = &methodState;

        declareLocal("자기");
        for (auto& param : method->parameters) {
            declareLocal(param->name);
        }
        current->function->arity = static_cast<int>(method->parameters.size());

        // 메서드 기본 매개변수 값 사전 평가
        methodState.function->defaultValues.resize(method->parameters.size());
        int methodDefaultCount = 0;
        for (size_t i = 0; i < method->defaultValues.size(); i++) {
            if (method->defaultValues[i]) {
                methodDefaultCount++;
                if (auto* intLit = dynamic_cast<IntegerLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Integer>(intLit->value);
                } else if (auto* floatLit = dynamic_cast<FloatLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Float>(floatLit->value);
                } else if (auto* strLit = dynamic_cast<StringLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<String>(strLit->value);
                } else if (auto* boolLit = dynamic_cast<BooleanLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Boolean>(boolLit->value);
                } else if (dynamic_cast<NullLiteral*>(method->defaultValues[i].get())) {
                    methodState.function->defaultValues[i] = make_shared<Null>();
                } else {
                    throw RuntimeException("VM에서는 상수 기본값만 지원합니다.", 0);
                }
            }
        }
        methodState.function->defaultCount = methodDefaultCount;

        // 메서드 매개변수 타입 정보 복사
        methodState.function->paramTypeChecks.resize(method->parameters.size(), ObjectType::NULL_OBJ);
        methodState.function->paramOptionals.resize(method->parameters.size(), false);
        for (size_t i = 0; i < method->parameterTypes.size(); i++) {
            if (method->parameterTypes[i]) {
                methodState.function->paramTypeChecks[i] = tokenTypeToObjectType(method->parameterTypes[i]->type);
            }
            if (i < method->parameterOptionals.size()) {
                methodState.function->paramOptionals[i] = method->parameterOptionals[i];
            }
        }
        if (method->returnType) {
            methodState.function->returnTypeCheck = tokenTypeToObjectType(method->returnType->type);
        }
        methodState.function->returnTypeOptional = method->returnTypeOptional;

        for (auto& s : method->body->statements) {
            compileStatement(s.get());
        }
        chunk().emitOp(OpCode::OP_NULL, 0);
        chunk().emitOp(OpCode::OP_RETURN, 0);
        current->function->localCount = static_cast<int>(current->locals.size());
        compiledMethods[method->name] = methodState.function;
        current = enclosing;
    }

    // CompiledClassDef 생성
    auto classDef = make_shared<CompiledClassDef>();
    classDef->name = stmt->name;
    classDef->fieldNames = stmt->fieldNames;
    classDef->fieldTypes = stmt->fieldTypes;
    classDef->compiledConstructor = constructorFn;
    classDef->compiledMethods = compiledMethods;

    // 상속 처리: 부모 클래스명 저장 (VM에서 런타임에 해결)
    if (!stmt->parentName.empty()) {
        classDef->parentName = stmt->parentName;
    }

    uint16_t classConstIdx = chunk().addConstant(classDef);
    chunk().emitOpAndUint16(OpCode::OP_CONSTANT, classConstIdx, 0);
    chunk().emitOpAndUint16(OpCode::OP_DEFINE_GLOBAL, nameIdx, 0);
}


void Compiler::compileMatch(MatchStatement* stmt) {
    compileExpression(stmt->subject.get());

    vector<size_t> endJumps;

    for (size_t i = 0; i < stmt->caseValues.size(); i++) {
        auto* caseExpr = stmt->caseValues[i].get();

        // 범위 패턴: 경우 1~5:
        if (auto* rangePattern = dynamic_cast<RangePatternExpression*>(caseExpr)) {
            chunk().emitOp(OpCode::OP_DUP, 0); // subject 복사
            compileExpression(rangePattern->start.get());
            compileExpression(rangePattern->end.get());
            chunk().emitOp(OpCode::OP_RANGE_CHECK, 0);
        }
        // 타입 패턴: 경우 정수:
        else if (auto* typePattern = dynamic_cast<TypePatternExpression*>(caseExpr)) {
            chunk().emitOp(OpCode::OP_DUP, 0); // subject 복사
            // 타입 이름을 상수로 저장
            string typeName = typePattern->typeToken->text;
            uint16_t idx = chunk().addConstant(make_shared<String>(typeName));
            chunk().emitOpAndUint16(OpCode::OP_TYPE_CHECK, idx, 0);
        }
        // 일반 값 매칭
        else {
            chunk().emitOp(OpCode::OP_DUP, 0); // subject 복사
            compileExpression(caseExpr);
            chunk().emitOp(OpCode::OP_EQUAL, 0);
        }

        // 조건 가드: 만약 조건
        size_t skipJump;
        if (i < stmt->caseGuards.size() && stmt->caseGuards[i]) {
            // 패턴이 매칭되지 않으면 가드를 건너뜀
            size_t guardSkip = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

            // 가드 조건 평가
            compileExpression(stmt->caseGuards[i].get());
            // 가드 결과로 점프 결정
            skipJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

            // guardSkip은 가드 실패 시와 같은 곳으로 점프해야 함
            // 하지만 OP_JUMP_IF_FALSE가 false를 pop하므로, guardSkip 점프 시 false를 push해야 함
            // 대신 다른 접근: guardSkip을 별도의 위치로 점프시키고, 거기서 false를 push
            size_t jumpToBody = chunk().emitJump(OpCode::OP_JUMP, 0);

            chunk().patchJump(guardSkip);
            // 패턴 불일치 -> 다음 case로
            size_t jumpToNext = chunk().emitJump(OpCode::OP_JUMP, 0);

            chunk().patchJump(skipJump);
            // 가드 불일치 -> 다음 case로
            size_t jumpToNext2 = chunk().emitJump(OpCode::OP_JUMP, 0);

            chunk().patchJump(jumpToBody);
            // case body
            chunk().emitOp(OpCode::OP_POP, 0); // subject 제거
            beginScope();
            for (auto& s : stmt->caseBodies[i]->statements) {
                compileStatement(s.get());
            }
            endScope(0);
            endJumps.push_back(chunk().emitJump(OpCode::OP_JUMP, 0));

            chunk().patchJump(jumpToNext);
            chunk().patchJump(jumpToNext2);
        } else {
            skipJump = chunk().emitJump(OpCode::OP_JUMP_IF_FALSE, 0);

            chunk().emitOp(OpCode::OP_POP, 0); // subject 제거
            beginScope();
            for (auto& s : stmt->caseBodies[i]->statements) {
                compileStatement(s.get());
            }
            endScope(0);
            endJumps.push_back(chunk().emitJump(OpCode::OP_JUMP, 0));

            chunk().patchJump(skipJump);
        }
    }

    // default
    chunk().emitOp(OpCode::OP_POP, 0); // subject 제거
    if (stmt->defaultBody) {
        beginScope();
        for (auto& s : stmt->defaultBody->statements) {
            compileStatement(s.get());
        }
        endScope(0);
    }

    for (auto offset : endJumps) {
        chunk().patchJump(offset);
    }
}


void Compiler::compileTryCatch(TryCatchStatement* stmt) {
    size_t tryBeginOffset = chunk().emitJump(OpCode::OP_TRY_BEGIN, 0);

    beginScope();
    for (auto& s : stmt->tryBody->statements) {
        compileStatement(s.get());
    }
    endScope(0);

    chunk().emitOp(OpCode::OP_TRY_END, 0);
    size_t skipCatch = chunk().emitJump(OpCode::OP_JUMP, 0);

    chunk().patchJump(tryBeginOffset);

    // catch 블록: 에러가 스택에 있음
    beginScope();
    declareLocal(stmt->errorName);
    for (auto& s : stmt->catchBody->statements) {
        compileStatement(s.get());
    }
    endScope(0);

    chunk().patchJump(skipCatch);
}


void Compiler::compileImport(ImportStatement* stmt) {
    uint16_t nameIdx = chunk().addConstant(make_shared<String>(stmt->filename));
    chunk().emitOpAndUint16(OpCode::OP_IMPORT, nameIdx, 0);
}


bool Compiler::containsYield(BlockStatement* block) {
    if (!block) return false;
    for (auto& stmt : block->statements) {
        if (dynamic_cast<YieldStatement*>(stmt.get())) return true;
        if (auto* whileStmt = dynamic_cast<WhileStatement*>(stmt.get())) {
            if (containsYield(whileStmt->body.get())) return true;
        }
        if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
            if (containsYield(ifStmt->consequence.get())) return true;
            if (containsYield(ifStmt->then.get())) return true;
        }
        if (auto* forEachStmt = dynamic_cast<ForEachStatement*>(stmt.get())) {
            if (containsYield(forEachStmt->body.get())) return true;
        }
        if (auto* forRangeStmt = dynamic_cast<ForRangeStatement*>(stmt.get())) {
            if (containsYield(forRangeStmt->body.get())) return true;
        }
    }
    return false;
}


void Compiler::compileYield(YieldStatement* stmt) {
    compileExpression(stmt->expression.get());
    chunk().emitOp(OpCode::OP_YIELD, 0);
}

