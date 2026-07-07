#include "completion.h"

#include "../../lexer/lexer.h"
#include "../../object/builtin_signatures.h"

namespace lsp::features {

    namespace {
        // LSP CompletionItemKind
        constexpr int kKindMethod = 2, kKindFunction = 3, kKindField = 5, kKindVariable = 6, kKindClass = 7,
                      kKindKeyword = 14;

        int completionKind(DocSymbolKind kind) {
            switch (kind) {
            case DocSymbolKind::Function:
                return kKindFunction;
            case DocSymbolKind::Class:
                return kKindClass;
            case DocSymbolKind::Field:
                return kKindField;
            case DocSymbolKind::Method:
                return kKindMethod;
            case DocSymbolKind::Variable:
            case DocSymbolKind::Parameter:
                return kKindVariable;
            }
            return kKindVariable;
        }
    } // namespace

    nlohmann::json completion(const AnalyzedDocument& doc) {
        nlohmann::json items = nlohmann::json::array();
        for (const auto& [keyword, type] : Lexer::Keywords()) {
            items.push_back({{"label", keyword}, {"kind", kKindKeyword}});
        }
        for (const auto& sig : kBuiltinSignatures) {
            items.push_back({{"label", sig.name}, {"kind", kKindFunction}, {"detail", "내장 함수"}});
        }
        for (const auto& s : doc.symbols) {
            items.push_back({{"label", s.name}, {"kind", completionKind(s.kind)}, {"detail", s.typeText}});
        }
        return items;
    }

} // namespace lsp::features
