#include "../lsp/jsonrpc.h"
#include "../lsp/server.h"
#include <gtest/gtest.h>
#include <sstream>

using nlohmann::json;

namespace {
    // out 스트림에 쌓인 프레임들을 순서대로 파싱
    std::vector<json> drain(std::ostringstream& out) {
        std::istringstream in(out.str());
        std::vector<json> msgs;
        while (auto body = lsp::readMessage(in)) {
            msgs.push_back(json::parse(*body));
        }
        out.str("");
        return msgs;
    }
    json didOpen(const std::string& uri, const std::string& text) {
        return {{"jsonrpc", "2.0"}, {"method", "textDocument/didOpen"},
            {"params", {{"textDocument", {{"uri", uri}, {"languageId", "hongik"}, {"version", 1}, {"text", text}}}}}};
    }
} // namespace

class LspServerTest : public ::testing::Test {
protected:
    std::ostringstream out;
    lsp::Dispatcher d;
    lsp::Server server{out};
    void SetUp() override {
        server.bind(d);
    }
};

TEST_F(LspServerTest, InitializeAdvertisesCapabilities) {
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}, {"params", json::object()}}, out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    const auto& caps = msgs[0]["result"]["capabilities"];
    EXPECT_EQ(caps["textDocumentSync"], 1);
    EXPECT_TRUE(caps["hoverProvider"].get<bool>());
    EXPECT_TRUE(caps["definitionProvider"].get<bool>());
    EXPECT_TRUE(caps["documentSymbolProvider"].get<bool>());
    EXPECT_TRUE(caps.contains("completionProvider"));
}

TEST_F(LspServerTest, DidOpenPublishesParseAndTypeDiagnostics) {
    d.handle(didOpen("file:///t.hik", "정수 x = ;\n"), out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0]["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(msgs[0]["params"]["uri"], "file:///t.hik");
    ASSERT_FALSE(msgs[0]["params"]["diagnostics"].empty());
    const auto& diag = msgs[0]["params"]["diagnostics"][0];
    EXPECT_EQ(diag["severity"], 1);
    EXPECT_EQ(diag["range"]["start"]["line"], 0); // 내부 1-based → LSP 0-based
    EXPECT_EQ(diag["range"]["start"]["character"], 7); // ';' cp 7 == utf16 7 (한글 BMP)
}

TEST_F(LspServerTest, DidChangeRepublishesAndDidCloseClears) {
    d.handle(didOpen("file:///t.hik", "정수 x = ;\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"method", "textDocument/didChange"},
                 {"params", {{"textDocument", {{"uri", "file:///t.hik"}, {"version", 2}}},
                                {"contentChanges", json::array({{{"text", "정수 x = 1\n"}}})}}}},
        out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_TRUE(msgs[0]["params"]["diagnostics"].empty()); // 고쳐졌으니 빈 배열

    d.handle(json{{"jsonrpc", "2.0"}, {"method", "textDocument/didClose"},
                 {"params", {{"textDocument", {{"uri", "file:///t.hik"}}}}}},
        out);
    msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_TRUE(msgs[0]["params"]["diagnostics"].empty()); // 닫으면 진단 클리어
}

TEST_F(LspServerTest, ShutdownThenExitSetsShouldExit) {
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 9}, {"method", "shutdown"}}, out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_TRUE(msgs[0]["result"].is_null());
    EXPECT_FALSE(server.shouldExit());
    d.handle(json{{"jsonrpc", "2.0"}, {"method", "exit"}}, out);
    EXPECT_TRUE(server.shouldExit());
}

TEST_F(LspServerTest, HoverOnVariableShowsDeclaredType) {
    d.handle(didOpen("file:///h.hik", "정수 개수 = 3\n출력(개수)\n"), out);
    drain(out);
    // 2행 "출력(개수)" — 개수: 출(0)력(1)((2)개(3)수(4) → cp 3
    d.handle(
        json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "textDocument/hover"},
            {"params", {{"textDocument", {{"uri", "file:///h.hik"}}}, {"position", {{"line", 1}, {"character", 3}}}}}},
        out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    const std::string value = msgs[0]["result"]["contents"]["value"];
    EXPECT_NE(value.find("정수 개수"), std::string::npos);
}

TEST_F(LspServerTest, HoverOnBuiltinShowsArity) {
    d.handle(didOpen("file:///h2.hik", "출력(1)\n"), out);
    drain(out);
    d.handle(
        json{{"jsonrpc", "2.0"}, {"id", 3}, {"method", "textDocument/hover"},
            {"params", {{"textDocument", {{"uri", "file:///h2.hik"}}}, {"position", {{"line", 0}, {"character", 0}}}}}},
        out);
    auto msgs               = drain(out);
    const std::string value = msgs[0]["result"]["contents"]["value"];
    EXPECT_NE(value.find("내장 함수"), std::string::npos);
    EXPECT_NE(value.find("출력"), std::string::npos);
}

TEST_F(LspServerTest, HoverOnNothingReturnsNull) {
    d.handle(didOpen("file:///h3.hik", "정수 x = 1\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 4}, {"method", "textDocument/hover"},
                 {"params", {{"textDocument", {{"uri", "file:///h3.hik"}}},
                                {"position", {{"line", 0}, {"character", 7}}}}}}, // '=' 위
        out);
    auto msgs = drain(out);
    EXPECT_TRUE(msgs[0]["result"].is_null());
}

TEST_F(LspServerTest, CompletionListsKeywordsBuiltinsAndSymbols) {
    d.handle(didOpen("file:///c.hik", "정수 내변수 = 1\n"), out);
    drain(out);
    d.handle(
        json{{"jsonrpc", "2.0"}, {"id", 5}, {"method", "textDocument/completion"},
            {"params", {{"textDocument", {{"uri", "file:///c.hik"}}}, {"position", {{"line", 0}, {"character", 0}}}}}},
        out);
    auto msgs = drain(out);
    ASSERT_EQ(msgs.size(), 1u);
    const auto& items = msgs[0]["result"];
    ASSERT_TRUE(items.is_array());
    auto has = [&](const std::string& label, int kind) {
        for (const auto& item : items) {
            if (item["label"] == label && item["kind"] == kind) {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(has("만약", 14)); // Keyword
    EXPECT_TRUE(has("출력", 3)); // 내장 Function
    EXPECT_TRUE(has("내변수", 6)); // Variable
}

TEST_F(LspServerTest, DefinitionJumpsToDeclaration) {
    d.handle(didOpen("file:///g.hik", "정수 개수 = 3\n출력(개수)\n"), out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 6}, {"method", "textDocument/definition"},
                 {"params", {{"textDocument", {{"uri", "file:///g.hik"}}},
                                {"position", {{"line", 1}, {"character", 3}}}}}}, // 2행의 '개수'
        out);
    auto msgs       = drain(out);
    const auto& loc = msgs[0]["result"];
    ASSERT_FALSE(loc.is_null());
    EXPECT_EQ(loc["uri"], "file:///g.hik");
    EXPECT_EQ(loc["range"]["start"]["line"], 0); // 1행 선언부
    EXPECT_EQ(loc["range"]["start"]["character"], 3); // 정수␣ 다음
}

TEST_F(LspServerTest, DocumentSymbolBuildsClassTree) {
    d.handle(didOpen("file:///s.hik", "정수 전역 = 1\n"
                                      "함수 도우미() -> 정수:\n"
                                      "    리턴 1\n"
                                      "클래스 점:\n"
                                      "    정수 x\n"
                                      "    생성(정수 값):\n"
                                      "        자기.x = 값\n"
                                      "    함수 값얻기() -> 정수:\n"
                                      "        리턴 자기.x\n"),
        out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 7}, {"method", "textDocument/documentSymbol"},
                 {"params", {{"textDocument", {{"uri", "file:///s.hik"}}}}}},
        out);
    auto msgs        = drain(out);
    const auto& syms = msgs[0]["result"];
    ASSERT_TRUE(syms.is_array());
    // 톱레벨: 전역(Variable 13), 도우미(Function 12), 점(Class 5) — 지역/파라미터 제외
    ASSERT_EQ(syms.size(), 3u);
    json cls;
    for (const auto& s : syms) {
        if (s["name"] == "점") {
            cls = s;
        }
    }
    ASSERT_FALSE(cls.is_null());
    EXPECT_EQ(cls["kind"], 5);
    // children: x(Field 8), 값얻기(Method 6) — 생성자 파라미터 제외
    ASSERT_EQ(cls["children"].size(), 2u);
}

TEST_F(LspServerTest, DocumentSymbolDoesNotMixSameNamedClasses) {
    d.handle(didOpen("file:///dup.hik", "클래스 점:\n"
                                        "    정수 x\n"
                                        "클래스 점:\n"
                                        "    정수 y\n"),
        out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 8}, {"method", "textDocument/documentSymbol"},
                 {"params", {{"textDocument", {{"uri", "file:///dup.hik"}}}}}},
        out);
    auto msgs        = drain(out);
    const auto& syms = msgs[0]["result"];
    ASSERT_TRUE(syms.is_array());
    ASSERT_EQ(syms.size(), 2u); // 톱레벨 클래스 2개, 둘 다 이름 "점"
    EXPECT_EQ(syms[0]["name"], "점");
    EXPECT_EQ(syms[1]["name"], "점");
    ASSERT_EQ(syms[0]["children"].size(), 1u);
    EXPECT_EQ(syms[0]["children"][0]["name"], "x"); // 첫 번째 클래스는 자신의 필드만
    ASSERT_EQ(syms[1]["children"].size(), 1u);
    EXPECT_EQ(syms[1]["children"][0]["name"], "y"); // 두 번째 클래스는 자신의 필드만
}

TEST_F(LspServerTest, DocumentSymbolNestedSameNameClassDoesNotContaminate) {
    d.handle(didOpen("file:///nested.hik", "클래스 점:\n"
                                           "    정수 x\n"
                                           "    함수 값얻기() -> 정수:\n"
                                           "        클래스 점:\n"
                                           "            정수 z\n"
                                           "        리턴 자기.x\n"),
        out);
    drain(out);
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 9}, {"method", "textDocument/documentSymbol"},
                 {"params", {{"textDocument", {{"uri", "file:///nested.hik"}}}}}},
        out);
    auto msgs        = drain(out);
    const auto& syms = msgs[0]["result"];
    ASSERT_TRUE(syms.is_array());
    ASSERT_EQ(syms.size(), 1u); // 톱레벨 "점" 1개 (중첩 클래스는 함수 본문 지역이라 톱레벨엔 없음)
    const auto& cls = syms[0];
    EXPECT_EQ(cls["name"], "점");
    // children: x(Field), 값얻기(Method) 정확히 2개 — 중첩 클래스의 z는 섞이지 않는다
    ASSERT_EQ(cls["children"].size(), 2u);
    bool hasZ = false;
    for (const auto& c : cls["children"]) {
        if (c["name"] == "z") {
            hasZ = true;
        }
    }
    EXPECT_FALSE(hasZ);
}
