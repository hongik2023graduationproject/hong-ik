#ifndef LSP_DOCUMENT_STORE_H
#define LSP_DOCUMENT_STORE_H

#include "../analyzer/symbol_collector.h"
#include "../analyzer/type_checker.h"
#include "../parser/parser.h"
#include "../token/token.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lsp {

    struct AnalyzedDocument {
        std::string uri;
        std::string text;
        long long version = 0;
        std::vector<std::string> lines;
        std::vector<std::shared_ptr<Token>> tokens;
        std::shared_ptr<Program> program;
        std::vector<ParseDiagnostic> parseDiagnostics;
        std::vector<TypeDiagnostic> typeDiagnostics;
        std::vector<SymbolInfo> symbols;
    };

    class DocumentStore {
    public:
        const AnalyzedDocument* open(const std::string& uri, const std::string& text, long long version);
        const AnalyzedDocument* update(const std::string& uri, const std::string& text, long long version);
        void close(const std::string& uri);
        const AnalyzedDocument* get(const std::string& uri) const;

    private:
        static AnalyzedDocument analyze(std::string uri, std::string text, long long version);
        std::map<std::string, AnalyzedDocument> docs_;
    };

} // namespace lsp

#endif // LSP_DOCUMENT_STORE_H
