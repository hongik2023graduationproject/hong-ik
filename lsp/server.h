#ifndef LSP_SERVER_H
#define LSP_SERVER_H

#include "document_store.h"
#include "jsonrpc.h"
#include <ostream>

namespace lsp {

    class Server {
    public:
        explicit Server(std::ostream& out) : out_(out) {}
        void bind(Dispatcher& dispatcher);
        bool shouldExit() const {
            return exit_;
        }

    private:
        nlohmann::json onInitialize(const nlohmann::json& params);
        void onDidOpen(const nlohmann::json& params);
        void onDidChange(const nlohmann::json& params);
        void onDidClose(const nlohmann::json& params);
        nlohmann::json onHover(const nlohmann::json& params); // Task 8
        nlohmann::json onCompletion(const nlohmann::json& params); // Task 9
        nlohmann::json onDefinition(const nlohmann::json& params); // Task 10
        nlohmann::json onDocumentSymbol(const nlohmann::json& params); // Task 10

        void publish(const AnalyzedDocument& doc);
        void publishEmpty(const std::string& uri);
        const AnalyzedDocument* docFor(const nlohmann::json& params) const;

        std::ostream& out_;
        DocumentStore store_;
        bool exit_ = false;
    };

} // namespace lsp

#endif // LSP_SERVER_H
