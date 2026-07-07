#ifndef LSP_FEATURES_DEFINITION_H
#define LSP_FEATURES_DEFINITION_H

#include "../document_store.h"

#include <nlohmann/json.hpp>

namespace lsp::features {
    nlohmann::json definition(const AnalyzedDocument& doc, long long lspLine, long long lspChar);
} // namespace lsp::features

#endif // LSP_FEATURES_DEFINITION_H
