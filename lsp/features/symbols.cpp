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
        // 수집 순서 불변식(analyzer/symbol_collector.cpp walkStatement): 클래스 심볼 뒤에 그
        // 클래스의 멤버 심볼들이 연속으로 나온다 — 이름 매칭 대신 "가장 최근 톱레벨 클래스"에
        // 귀속시켜 동명 톱레벨 클래스 간 children 오염을 방지한다 (단일 패스, O(N)).
        // currentClassIdx는 포인터가 아닌 인덱스로 잡는다: nlohmann::json의 array는 내부적으로
        // std::vector이므로 result.push_back()이 재할당을 일으키면 이전에 잡아둔 포인터/참조는
        // 무효화될 수 있다. 인덱스는 재할당과 무관하게 유효하다.
        bool hasCurrentClass   = false;
        size_t currentClassIdx = 0;
        std::string currentClassName;
        for (const auto& s : doc.symbols) {
            if (s.container.empty()) {
                result.push_back(toDocumentSymbol(doc, s));
                if (s.kind == DocSymbolKind::Class) {
                    hasCurrentClass  = true;
                    currentClassIdx  = result.size() - 1;
                    currentClassName = s.name;
                } else {
                    hasCurrentClass = false;
                    currentClassName.clear();
                }
                continue;
            }
            if (hasCurrentClass && s.container == currentClassName
                && (s.kind == DocSymbolKind::Field || s.kind == DocSymbolKind::Method)) {
                result[currentClassIdx]["children"].push_back(toDocumentSymbol(doc, s));
            }
        }
        return result;
    }

} // namespace lsp::features
