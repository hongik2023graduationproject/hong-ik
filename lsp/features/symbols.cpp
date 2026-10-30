#include "symbols.h"

#include "diagnostics.h"
#include <map>

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
        // 심볼 벡터 인덱스 → result 내 클래스 노드 인덱스 (명시적 소유 관계 — 이름/순서 추론 없음).
        // SymbolCollector가 클래스 직접 멤버(Field/Method)에만 ownerClassIndex를 채워주므로,
        // 메서드 본문에 중첩된 동명 클래스의 멤버는 ownerClassIndex가 -1이라 여기서 걸러진다.
        std::map<long long, size_t> classNodeIndex;
        for (size_t i = 0; i < doc.symbols.size(); ++i) {
            const auto& s = doc.symbols[i];
            if (s.container.empty()) {
                result.push_back(toDocumentSymbol(doc, s));
                if (s.kind == DocSymbolKind::Class) {
                    classNodeIndex[static_cast<long long>(i)] = result.size() - 1;
                }
                continue;
            }
            if (s.ownerClassIndex < 0) {
                continue;
            }
            auto it = classNodeIndex.find(s.ownerClassIndex);
            if (it == classNodeIndex.end()) {
                continue; // 중첩 클래스의 멤버 — 톱레벨 트리에 없음
            }
            if (s.kind != DocSymbolKind::Field && s.kind != DocSymbolKind::Method) {
                continue;
            }
            result[it->second]["children"].push_back(toDocumentSymbol(doc, s));
        }
        return result;
    }

} // namespace lsp::features
