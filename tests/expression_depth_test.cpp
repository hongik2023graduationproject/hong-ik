// 식 AST 깊이 제한: 너무 깊은 식은 스택 오버플로로 죽지 않고 파스 오류로 거절된다.
#include "benchmarks/runner.h"
#include "lsp/document_store.h"
#include "object/object.h"
#include "parser/parser.h"
#include <gtest/gtest.h>
#include <string>

namespace {
    constexpr int kMax = Parser::kMaxExpressionDepth;
    const std::string kTooDeep = "식이 너무 깊게 중첩되었습니다";

    std::string nestedParens(int depth) {
        return std::string(depth, '(') + "1" + std::string(depth, ')');
    }

    // terms개 항의 왼쪽 결합 덧셈. AST 깊이는 terms (중위 노드 terms-1개 + 리프).
    std::string plusChain(int terms) {
        std::string source = "1";
        for (int i = 1; i < terms; i++) {
            source += "+1";
        }
        return source;
    }

    void expectRejectedAsTooDeep(const std::string& source) {
        try {
            bench::parseOnly(source);
            FAIL() << "깊이 제한을 넘는 식이 거절되지 않음 (길이 " << source.size() << ")";
        } catch (const std::exception& e) {
            EXPECT_NE(std::string(e.what()).find(kTooDeep), std::string::npos) << e.what();
        }
    }

    long long runOnVm(const std::string& source) {
        auto result = bench::runVm(bench::prepare(source));
        auto* integer = dynamic_cast<Integer*>(result.get());
        return integer ? integer->value : -1;
    }
} // namespace

TEST(ExpressionDepthTest, VeryDeepParenthesesAreAParseErrorNotACrash) {
    expectRejectedAsTooDeep(nestedParens(100000));
}

TEST(ExpressionDepthTest, LongOperatorChainIsAParseErrorNotACrash) {
    expectRejectedAsTooDeep(plusChain(100000));
}

TEST(ExpressionDepthTest, LimitIsExactForChains) {
    EXPECT_EQ(runOnVm(plusChain(kMax)), kMax);
    expectRejectedAsTooDeep(plusChain(kMax + 1));
}

TEST(ExpressionDepthTest, DepthAddsUpAcrossGroupAndOuterChain) {
    // 각 부분은 한도 아래지만, 괄호 그룹이 바깥 체인 왼쪽 맨 아래에 붙어 전체 깊이는 한도를 넘는다.
    const int part = kMax * 2 / 3;
    expectRejectedAsTooDeep("1+(" + plusChain(part) + ")" + plusChain(part).substr(1));
    EXPECT_EQ(runOnVm("1+(" + plusChain(part) + ")"), part + 1);
}

TEST(ExpressionDepthTest, LspDocumentWithTooDeepExpressionReportsDiagnostic) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///deep.hik", "정수 x = " + nestedParens(100000) + "\n정수 y = 2\n", 1);
    ASSERT_NE(doc, nullptr);
    ASSERT_FALSE(doc->parseDiagnostics.empty());
    EXPECT_NE(doc->parseDiagnostics[0].message.find(kTooDeep), std::string::npos);
}
