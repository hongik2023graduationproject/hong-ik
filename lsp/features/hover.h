#ifndef LSP_FEATURES_HOVER_H
#define LSP_FEATURES_HOVER_H

#include "../document_store.h"

#include <nlohmann/json.hpp>

namespace lsp::features {

    // 내부 좌표(line 1-based, cp col)의 토큰. 없으면 nullptr
    std::shared_ptr<Token> tokenAt(const AnalyzedDocument& doc, long long line1, long long cpCol);
    // 이름 → 심볼. 선언 line <= 요청 line 중 최근접, 없으면 최초 선언 (spec v1.1 한계 ②)
    const SymbolInfo* resolveSymbol(const AnalyzedDocument& doc, const std::string& name, long long line1);
    // LSP 좌표를 받아 Hover result JSON (해당 없음 → null)
    nlohmann::json hover(const AnalyzedDocument& doc, long long lspLine, long long lspChar);

} // namespace lsp::features

#endif // LSP_FEATURES_HOVER_H
