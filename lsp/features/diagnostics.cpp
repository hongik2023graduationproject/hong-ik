#include "diagnostics.h"

#include "../position.h"
#include <algorithm>

namespace lsp::features {

    nlohmann::json toRange(const AnalyzedDocument& doc, long long line1, long long col, long long endCol) {
        const long long line0      = std::max(0LL, line1 - 1);
        const std::string lineText = line0 < static_cast<long long>(doc.lines.size()) ? doc.lines[line0] : "";
        if (col == 0 && endCol == 0) {
            endCol = std::max(1LL, cpLength(lineText));
        }
        if (endCol <= col) {
            endCol = col + 1;
        }
        return {{"start", {{"line", line0}, {"character", cpToUtf16(lineText, col)}}},
            {"end",
                {{"line", line0}, {"character", std::max(cpToUtf16(lineText, endCol), cpToUtf16(lineText, col) + 1)}}}};
    }

    nlohmann::json buildDiagnostics(const AnalyzedDocument& doc) {
        nlohmann::json diags = nlohmann::json::array();
        for (const auto& d : doc.parseDiagnostics) {
            diags.push_back({{"range", toRange(doc, d.line, d.column, d.endColumn)}, {"severity", 1},
                {"source", "hongik"}, {"message", d.message}});
        }
        for (const auto& d : doc.typeDiagnostics) {
            diags.push_back(
                {{"range", toRange(doc, d.line, d.column, 0)}, {"severity", d.severity == Severity::ERROR ? 1 : 2},
                    {"source", "hongik-type"}, {"code", d.code}, {"message", d.message}});
        }
        return diags;
    }

} // namespace lsp::features
