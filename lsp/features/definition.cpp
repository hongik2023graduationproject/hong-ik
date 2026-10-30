#include "definition.h"

#include "../position.h"
#include "diagnostics.h"
#include "hover.h"

namespace lsp::features {

    nlohmann::json definition(const AnalyzedDocument& doc, long long lspLine, long long lspChar) {
        if (lspLine < 0 || lspLine >= static_cast<long long>(doc.lines.size())) {
            return nullptr;
        }
        const long long cpCol = utf16ToCp(doc.lines[lspLine], lspChar);
        auto token            = tokenAt(doc, lspLine + 1, cpCol);
        if (!token || token->type != TokenType::IDENTIFIER) {
            return nullptr;
        }
        const auto* sym = resolveSymbol(doc, token->text, lspLine + 1);
        if (!sym) {
            return nullptr;
        }
        return {{"uri", doc.uri}, {"range", toRange(doc, sym->line, sym->column, sym->endColumn)}};
    }

} // namespace lsp::features
