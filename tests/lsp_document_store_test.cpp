#include "../lsp/document_store.h"
#include <gtest/gtest.h>

TEST(DocumentStoreTest, AnalyzesValidDocument) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///a.hik", "정수 x = 10\n출력(x)\n", 1);
    ASSERT_NE(doc, nullptr);
    EXPECT_TRUE(doc->parseDiagnostics.empty());
    EXPECT_FALSE(doc->tokens.empty());
    EXPECT_FALSE(doc->symbols.empty());
    EXPECT_EQ(doc->symbols[0].name, "x");
}

TEST(DocumentStoreTest, PartialAstOnParseError) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///b.hik", "정수 x = ;\n정수 y = 2\n", 1);
    ASSERT_NE(doc, nullptr);
    EXPECT_FALSE(doc->parseDiagnostics.empty());
    bool foundY = false;
    for (const auto& s : doc->symbols) {
        if (s.name == "y") {
            foundY = true;
        }
    }
    EXPECT_TRUE(foundY); // 부분 AST에서도 심볼 유지
}

TEST(DocumentStoreTest, LexerFailureYieldsSingleDiagnostic) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///c.hik", "정수 x = @\n", 1);
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->parseDiagnostics.size(), 1u);
    EXPECT_EQ(doc->parseDiagnostics[0].line, 1);
    EXPECT_EQ(doc->program, nullptr);
}

TEST(DocumentStoreTest, TypeDiagnosticsReported) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///d.hik", "정수 x = 1\n정수 y = 모름\n", 1);
    ASSERT_NE(doc, nullptr);
    EXPECT_FALSE(doc->typeDiagnostics.empty()); // TC001 미선언 식별자 계열
}

TEST(DocumentStoreTest, UpdateReplacesAndCloseRemoves) {
    lsp::DocumentStore store;
    store.open("file:///e.hik", "정수 x = 1\n", 1);
    const auto* doc = store.update("file:///e.hik", "정수 바뀜 = 2\n", 2);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->symbols[0].name, "바뀜");
    store.close("file:///e.hik");
    EXPECT_EQ(store.get("file:///e.hik"), nullptr);
}

TEST(DocumentStoreTest, AnalyzesDocumentWithoutTrailingNewline) {
    lsp::DocumentStore store;
    const auto* doc = store.open("file:///f.hik", "함수 f() -> 정수:\n    리턴 1", 1);
    ASSERT_NE(doc, nullptr);
    bool foundF = false;
    for (const auto& s : doc->symbols) {
        if (s.name == "f") {
            foundF = true;
        }
    }
    EXPECT_TRUE(foundF);
}
