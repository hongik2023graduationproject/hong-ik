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
