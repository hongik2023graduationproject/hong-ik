#ifndef LSP_FEATURES_DIAGNOSTICS_H
#define LSP_FEATURES_DIAGNOSTICS_H

#include "../document_store.h"

#include <nlohmann/json.hpp>

namespace lsp::features {

    // 내부 좌표(line 1-based, cp column) → LSP Range JSON.
    // col==endCol==0(위치 미상)이면 줄 전체를 범위로 잡는다.
    nlohmann::json toRange(const AnalyzedDocument& doc, long long line1, long long col, long long endCol);
    // parse + type 진단을 LSP Diagnostic 배열로 변환 (parse=Error(1), 타입 ERROR=1/WARNING=2)
    nlohmann::json buildDiagnostics(const AnalyzedDocument& doc);

} // namespace lsp::features

#endif // LSP_FEATURES_DIAGNOSTICS_H
