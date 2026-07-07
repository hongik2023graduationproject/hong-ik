#ifndef LSP_FEATURES_SYMBOLS_H
#define LSP_FEATURES_SYMBOLS_H

#include "../document_store.h"

#include <nlohmann/json.hpp>

namespace lsp::features {
    // DocumentSymbol[] — 톱레벨(container=="")의 변수/함수/클래스, 클래스는 필드/메서드 children
    nlohmann::json documentSymbols(const AnalyzedDocument& doc);
} // namespace lsp::features

#endif // LSP_FEATURES_SYMBOLS_H
