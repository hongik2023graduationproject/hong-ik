#include "symbols.h"

#include "diagnostics.h"

namespace lsp::features {

    namespace {
        // LSP SymbolKind (CompletionItemKind와 다른 enum임에 주의)
        constexpr int kSymClass = 5, kSymMethod = 6, kSymField = 8, kSymFunction = 12, kSymVariable = 13;

        int lspSymbolKind(DocSymbolKind kind) {
            switch (kind) {
            case DocSymbolKind::Class:
                return kSymClass;
            case DocSymbolKind::Method:
                return kSymMethod;
            case DocSymbolKind::Field:
                return kSymField;
            case DocSymbolKind::Function:
                return kSymFunction;
            default:
                return kSymVariable;
            }
        }

        nlohmann::json toDocumentSymbol(const AnalyzedDocument& doc, const SymbolInfo& s) {
            auto range = toRange(doc, s.line, s.column, s.endColumn);
            return {{"name", s.name}, {"kind", lspSymbolKind(s.kind)}, {"detail", s.typeText}, {"range", range},
                {"selectionRange", range}, {"children", nlohmann::json::array()}};
        }
    } // namespace

    nlohmann::json documentSymbols(const AnalyzedDocument& doc) {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& s : doc.symbols) {
            if (!s.container.empty()) {
                continue; // 톱레벨만 (멤버는 children으로)
            }
            auto node = toDocumentSymbol(doc, s);
            if (s.kind == DocSymbolKind::Class) {
                for (const auto& member : doc.symbols) {
                    if (member.container != s.name) {
                        continue;
                    }
                    if (member.kind != DocSymbolKind::Field && member.kind != DocSymbolKind::Method) {
                        continue;
                    }
                    node["children"].push_back(toDocumentSymbol(doc, member));
                }
            }
            result.push_back(std::move(node));
        }
        return result;
    }

} // namespace lsp::features
