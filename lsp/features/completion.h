#ifndef LSP_FEATURES_COMPLETION_H
#define LSP_FEATURES_COMPLETION_H

#include "../document_store.h"

#include <nlohmann/json.hpp>

namespace lsp::features {

    // v1: 키워드 + 내장 함수 + 문서 전체 심볼 (스코프 필터링은 v2)
    nlohmann::json completion(const AnalyzedDocument& doc);

} // namespace lsp::features

#endif // LSP_FEATURES_COMPLETION_H
