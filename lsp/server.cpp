#include "server.h"

#include "features/diagnostics.h"

namespace lsp {

    void Server::bind(Dispatcher& d) {
        d.onRequest("initialize", [this](const nlohmann::json& p) { return onInitialize(p); });
        d.onNotification("initialized", [](const nlohmann::json&) {});
        d.onRequest("shutdown", [](const nlohmann::json&) { return nlohmann::json(nullptr); });
        d.onNotification("exit", [this](const nlohmann::json&) { exit_ = true; });
        d.onNotification("textDocument/didOpen", [this](const nlohmann::json& p) { onDidOpen(p); });
        d.onNotification("textDocument/didChange", [this](const nlohmann::json& p) { onDidChange(p); });
        d.onNotification("textDocument/didClose", [this](const nlohmann::json& p) { onDidClose(p); });
    }

    nlohmann::json Server::onInitialize(const nlohmann::json&) {
        return {{"capabilities", {{"textDocumentSync", 1}, // Full
                                     {"hoverProvider", true}, {"completionProvider", nlohmann::json::object()},
                                     {"definitionProvider", true}, {"documentSymbolProvider", true}}},
            {"serverInfo", {{"name", "hongik-lsp"}, {"version", "0.1.0"}}}};
    }

    void Server::onDidOpen(const nlohmann::json& params) {
        const auto& td  = params.at("textDocument");
        const auto* doc = store_.open(td.at("uri"), td.at("text"), td.value("version", 0LL));
        publish(*doc);
    }

    void Server::onDidChange(const nlohmann::json& params) {
        const auto& td      = params.at("textDocument");
        const auto& changes = params.at("contentChanges");
        if (changes.empty()) {
            return;
        }
        // Full sync — 마지막 변경분의 text가 전체 문서
        const auto* doc = store_.update(td.at("uri"), changes.back().at("text"), td.value("version", 0LL));
        publish(*doc);
    }

    void Server::onDidClose(const nlohmann::json& params) {
        const std::string uri = params.at("textDocument").at("uri");
        store_.close(uri);
        publishEmpty(uri);
    }

    void Server::publish(const AnalyzedDocument& doc) {
        writeMessage(out_, {{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"},
                               {"params", {{"uri", doc.uri}, {"version", doc.version},
                                              {"diagnostics", features::buildDiagnostics(doc)}}}});
    }

    void Server::publishEmpty(const std::string& uri) {
        writeMessage(out_, {{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"},
                               {"params", {{"uri", uri}, {"diagnostics", nlohmann::json::array()}}}});
    }

    const AnalyzedDocument* Server::docFor(const nlohmann::json& params) const {
        return store_.get(params.at("textDocument").at("uri"));
    }

} // namespace lsp
