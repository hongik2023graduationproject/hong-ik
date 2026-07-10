#include "benchmarks/runner.h"
#include "object/object.h"
#include "vm/peephole.h"
#include <gtest/gtest.h>

namespace {

    long long asInt(const std::shared_ptr<Object>& obj) {
        auto* i = dynamic_cast<Integer*>(obj.get());
        EXPECT_NE(i, nullptr) << (obj ? obj->ToString() : "null");
        return i ? i->value : -1;
    }

    bool containsOp(const CompiledFunction& fn, OpCode op) {
        for (auto& in : peephole::decode(fn.code)) {
            if (in.op == op) {
                return true;
            }
        }
        return false;
    }

    // ---- decode ----
    TEST(PeepholeDecode, CoversAllConstructs) {
        // 함수(클로저)·클래스·try·foreach·match·보간 등 가변 길이 포함 프로그램이 끝까지 디코드되는지
        auto sp     = bench::prepare("함수 f(정수 a) -> 정수:\n    리턴 a + 1\n"
                                         "클래스 점:\n    정수 x\n    생성(정수 a):\n        자기.x = a\n"
                                         "배열 arr = [1, 2]\n"
                                         "각각 정수 원소 arr 에서:\n    출력(원소)\n"
                                         "f(1)\n");
        auto instrs = peephole::decode(sp.bytecode->code);
        ASSERT_FALSE(instrs.empty());
        // 명령 크기 합 == 코드 길이 (경계 정합)
        size_t total = 0;
        for (auto& in : instrs) {
            total += in.size;
        }
        EXPECT_EQ(total, sp.bytecode->code.size());
    }

    // ---- 융합 존재 ----
    TEST(PeepholeFusion, GlobalAssignmentFused) {
        auto sp = bench::prepare("정수 x = 0\nx = 1\nx\n");
        EXPECT_TRUE(containsOp(*sp.bytecode, OpCode::OP_SET_GLOBAL_POP));
        EXPECT_EQ(asInt(bench::runVm(sp)), 1);
    }

    TEST(PeepholeFusion, LocalAssignmentInFunctionFused) {
        auto sp = bench::prepare("함수 f() -> 정수:\n    정수 y = 0\n    y = 5\n    리턴 y\nf()\n");
        // 중첩 함수 상수에 재귀 적용됐는지
        bool found = false;
        for (auto& c : sp.bytecode->constants) {
            if (auto* nested = dynamic_cast<CompiledFunction*>(c.get())) {
                if (containsOp(*nested, OpCode::OP_SET_LOCAL_POP)) {
                    found = true;
                }
            }
        }
        EXPECT_TRUE(found);
        EXPECT_EQ(asInt(bench::runVm(sp)), 5);
    }

    TEST(PeepholeFusion, MemberAssignmentInMethodFused) {
        auto sp    = bench::prepare("클래스 점:\n    정수 x\n    생성(정수 a):\n        자기.x = a\n"
                                       "    함수 셋(정수 v) -> 정수:\n        자기.x = v\n        리턴 자기.x\n"
                                       "점 p = 점(1)\np.셋(9)\n");
        bool found = false;
        for (auto& c : sp.bytecode->constants) {
            if (auto* ccd = dynamic_cast<CompiledClassDef*>(c.get())) {
                if (ccd->compiledConstructor && containsOp(*ccd->compiledConstructor, OpCode::OP_SET_MEMBER_POP)) {
                    found = true;
                }
                for (auto& [name, m] : ccd->compiledMethods) {
                    if (containsOp(*m, OpCode::OP_SET_MEMBER_POP)) {
                        found = true;
                    }
                }
            }
        }
        EXPECT_TRUE(found);
        EXPECT_EQ(asInt(bench::runVm(sp)), 9);
    }

    // ---- 점프 타깃 가드 (수제 청크 — 소스로는 강제하기 어려움) ----
    TEST(PeepholeGuard, PopAtJumpTargetNotFused) {
        CompiledFunction fn;
        // JUMP +3 → SET_LOCAL 0 바로 뒤의 POP을 타깃으로 하는 인위적 청크
        fn.emitOpAndUint16(OpCode::OP_JUMP, 3, 1); // pos 0..2, target = 3+3 = 6 (POP 위치)
        fn.emitOpAndUint16(OpCode::OP_SET_LOCAL, 0, 1); // pos 3..5
        fn.emitOp(OpCode::OP_POP, 1); // pos 6
        fn.emitOp(OpCode::OP_RETURN, 1); // pos 7
        auto before = fn.code;
        peephole::optimizeChunk(fn);
        EXPECT_EQ(fn.code, before); // 가드로 융합 불가 → 무변경
    }

    TEST(PeepholeGuard, PopNotTargetIsFused) {
        CompiledFunction fn;
        fn.emitOpAndUint16(OpCode::OP_JUMP, 4, 1); // target = 3+4 = 7 (RETURN 위치) — POP 아님
        fn.emitOpAndUint16(OpCode::OP_SET_LOCAL, 0, 1); // pos 3..5
        fn.emitOp(OpCode::OP_POP, 1); // pos 6
        fn.emitOp(OpCode::OP_RETURN, 1); // pos 7
        peephole::optimizeChunk(fn);
        // 융합 후: JUMP(3) + SET_LOCAL_POP(3) + RETURN(1) = 7바이트, JUMP 타깃은 새 RETURN 위치(6)로 재매핑
        ASSERT_EQ(fn.code.size(), 7u);
        EXPECT_EQ(static_cast<OpCode>(fn.code[3]), OpCode::OP_SET_LOCAL_POP);
        uint16_t off = static_cast<uint16_t>((fn.code[1] << 8) | fn.code[2]);
        EXPECT_EQ(3 + off, 6u); // 새 타깃 = RETURN 새 위치
        EXPECT_EQ(fn.code.size(), fn.lines.size()); // lines 재매핑 정합
    }

    // ---- 실행 동등성: 융합·재매핑이 제어 흐름을 깨지 않는지 (VM vs Evaluator) ----
    TEST(PeepholeParity, ControlFlowWithAssignments) {
        const char* sources[] = {
            // while + break/continue + 대입
            "정수 s = 0\n정수 i = 0\n반복 i < 10 동안:\n    i = i + 1\n    만약 i == 3 라면:\n        계속\n    만약 i "
            "== 8 라면:\n        중단\n    s = s + i\ns\n",
            // if/else 마지막 문이 대입
            "정수 x = 0\n만약 1 < 2 라면:\n    x = 10\n아니면:\n    x = 20\nx\n",
            // try/catch 안 대입
            "정수 r = 0\n시도:\n    r = 1\n    정수 z = 1 / 0\n실패 e:\n    r = 2\nr\n",
            // foreach 안 대입
            "정수 s = 0\n배열 a = [1, 2, 3]\n각각 정수 v a 에서:\n    s = s + v\ns\n",
        };
        for (const char* src : sources) {
            auto sp = bench::prepare(src);
            EXPECT_EQ(asInt(bench::runVm(sp)), asInt(bench::runEval(sp))) << src;
        }
    }

} // namespace
