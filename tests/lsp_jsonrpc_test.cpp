#include "../lsp/jsonrpc.h"
#include <gtest/gtest.h>
#include <sstream>

using nlohmann::json;

TEST(JsonRpcFramingTest, WriteThenReadRoundTrips) {
    std::stringstream ss;
    lsp::writeMessage(ss, json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}});
    auto body = lsp::readMessage(ss);
    ASSERT_TRUE(body.has_value());
    auto parsed = json::parse(*body);
    EXPECT_EQ(parsed["method"], "initialize");
}

TEST(JsonRpcFramingTest, ReadHandlesMultipleMessagesAndHangul) {
    std::stringstream ss;
    lsp::writeMessage(ss, json{{"v", "첫번째"}});
    lsp::writeMessage(ss, json{{"v", "두번째"}});
    EXPECT_EQ(json::parse(*lsp::readMessage(ss))["v"], "첫번째");
    EXPECT_EQ(json::parse(*lsp::readMessage(ss))["v"], "두번째");
    EXPECT_FALSE(lsp::readMessage(ss).has_value()); // EOF
}

TEST(JsonRpcFramingTest, GarbageHeaderReturnsNullopt) {
    std::stringstream ss("Content-Length: abc\r\n\r\n{}");
    EXPECT_FALSE(lsp::readMessage(ss).has_value());
}

TEST(DispatcherTest, RoutesRequestAndWritesResponse) {
    lsp::Dispatcher d;
    d.onRequest("echo", [](const json& p) { return p; });
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 7}, {"method", "echo"}, {"params", {{"x", 1}}}}, out);
    std::istringstream in(out.str());
    auto resp = json::parse(*lsp::readMessage(in));
    EXPECT_EQ(resp["id"], 7);
    EXPECT_EQ(resp["result"]["x"], 1);
}

TEST(DispatcherTest, UnknownRequestReturnsMethodNotFound) {
    lsp::Dispatcher d;
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "nope"}}, out);
    std::istringstream in(out.str());
    auto resp = json::parse(*lsp::readMessage(in));
    EXPECT_EQ(resp["error"]["code"], -32601);
}

TEST(DispatcherTest, HandlerExceptionBecomesInternalError) {
    lsp::Dispatcher d;
    d.onRequest("boom", [](const json&) -> json { throw std::runtime_error("bad"); });
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "boom"}}, out);
    std::istringstream in(out.str());
    auto resp = json::parse(*lsp::readMessage(in));
    EXPECT_EQ(resp["error"]["code"], -32603);
}

TEST(DispatcherTest, NotificationProducesNoOutput) {
    lsp::Dispatcher d;
    bool called = false;
    d.onNotification("note", [&](const json&) { called = true; });
    std::ostringstream out;
    d.handle(json{{"jsonrpc", "2.0"}, {"method", "note"}}, out);
    EXPECT_TRUE(called);
    EXPECT_TRUE(out.str().empty());
    d.handle(json{{"jsonrpc", "2.0"}, {"method", "unknown-note"}}, out); // 무시, 크래시 없음
    EXPECT_TRUE(out.str().empty());
}
