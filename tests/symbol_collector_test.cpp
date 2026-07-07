#include "../analyzer/symbol_collector.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>

namespace {
    std::vector<SymbolInfo> collectFrom(const std::string& src) {
        Lexer lexer;
        Parser parser;
        auto program = parser.Parsing(lexer.Tokenize(Utf8Converter::Convert(src)));
        SymbolCollector collector;
        return collector.collect(program);
    }
    const SymbolInfo* find(const std::vector<SymbolInfo>& v, const std::string& name) {
        for (const auto& s : v) {
            if (s.name == name) {
                return &s;
            }
        }
        return nullptr;
    }
} // namespace

TEST(SymbolCollectorTest, CollectsGlobalsFunctionsClassesAndLocals) {
    auto symbols = collectFrom("정수 개수 = 3\n"
                               "실수? 비율 = 없음\n"
                               "함수 더하기(정수 a, 정수 b) -> 정수:\n"
                               "    정수 합 = a + b\n"
                               "    리턴 합\n"
                               "클래스 점:\n"
                               "    정수 x\n"
                               "    생성(정수 값):\n"
                               "        자기.x = 값\n"
                               "    함수 값얻기() -> 정수:\n"
                               "        리턴 자기.x\n"
                               "\n");

    auto* cnt = find(symbols, "개수");
    ASSERT_NE(cnt, nullptr);
    EXPECT_EQ(cnt->kind, DocSymbolKind::Variable);
    EXPECT_EQ(cnt->typeText, "정수");
    EXPECT_EQ(cnt->container, "");
    EXPECT_EQ(cnt->line, 1);
    EXPECT_EQ(cnt->column, 3);

    auto* ratio = find(symbols, "비율");
    ASSERT_NE(ratio, nullptr);
    EXPECT_EQ(ratio->typeText, "실수?");

    auto* add = find(symbols, "더하기");
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->kind, DocSymbolKind::Function);
    EXPECT_EQ(add->typeText, "함수 더하기(정수 a, 정수 b) -> 정수");

    auto* sum = find(symbols, "합");
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->container, "더하기");

    auto* paramA = find(symbols, "a");
    ASSERT_NE(paramA, nullptr);
    EXPECT_EQ(paramA->kind, DocSymbolKind::Parameter);
    EXPECT_EQ(paramA->container, "더하기");

    auto* cls = find(symbols, "점");
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->kind, DocSymbolKind::Class);

    auto* fieldX = find(symbols, "x");
    ASSERT_NE(fieldX, nullptr);
    EXPECT_EQ(fieldX->kind, DocSymbolKind::Field);
    EXPECT_EQ(fieldX->container, "점");

    auto* method = find(symbols, "값얻기");
    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->kind, DocSymbolKind::Method);
    EXPECT_EQ(method->container, "점");
}

TEST(SymbolCollectorTest, CollectsLoopVariables) {
    auto symbols = collectFrom("반복 정수 i = 0 부터 10 까지:\n"
                               "    출력(i)\n"
                               "각각 정수 요소 [1, 2] 에서:\n"
                               "    출력(요소)\n"
                               "\n");
    EXPECT_NE(find(symbols, "i"), nullptr);
    EXPECT_NE(find(symbols, "요소"), nullptr);
}

TEST(SymbolCollectorTest, SetsOwnerClassIndexForDirectMembersOnly) {
    auto symbols = collectFrom("정수 전역 = 1\n"
                               "클래스 점:\n"
                               "    정수 x\n"
                               "    함수 값얻기() -> 정수:\n"
                               "        리턴 자기.x\n"
                               "\n");

    long long classIdx = -1;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (symbols[i].name == "점" && symbols[i].kind == DocSymbolKind::Class) {
            classIdx = static_cast<long long>(i);
        }
    }
    ASSERT_NE(classIdx, -1);

    auto* global = find(symbols, "전역");
    ASSERT_NE(global, nullptr);
    EXPECT_EQ(global->ownerClassIndex, -1); // 전역 변수는 클래스 멤버가 아님

    auto* fieldX = find(symbols, "x");
    ASSERT_NE(fieldX, nullptr);
    EXPECT_EQ(fieldX->ownerClassIndex, classIdx);

    auto* method = find(symbols, "값얻기");
    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->ownerClassIndex, classIdx);
}

TEST(SymbolCollectorTest, SurvivesPartialAst) {
    auto symbols = collectFrom("정수 x = ;\n정수 y = 2\n");
    EXPECT_NE(find(symbols, "y"), nullptr); // 1행이 깨져도 2행 심볼은 수집
}
