#include "hover.h"

#include "../../object/builtin_signatures.h"
#include "../position.h"
#include "diagnostics.h"
#include <climits>

namespace lsp::features {

    std::shared_ptr<Token> tokenAt(const AnalyzedDocument& doc, long long line1, long long cpCol) {
        for (const auto& t : doc.tokens) {
            if (t->line == line1 && t->column <= cpCol && cpCol < t->endColumn) {
                return t;
            }
        }
        return nullptr;
    }

    const SymbolInfo* resolveSymbol(const AnalyzedDocument& doc, const std::string& name, long long line1) {
        const SymbolInfo* before = nullptr; // line <= 요청 줄 중 가장 늦은 선언
        const SymbolInfo* first  = nullptr; // 전체 중 가장 이른 선언
        for (const auto& s : doc.symbols) {
            if (s.name != name) {
                continue;
            }
            if (!first || s.line < first->line) {
                first = &s;
            }
            if (s.line <= line1 && (!before || s.line > before->line)) {
                before = &s;
            }
        }
        return before ? before : first;
    }

    namespace {

        std::string kindLabel(DocSymbolKind kind) {
            switch (kind) {
            case DocSymbolKind::Variable:
                return "변수";
            case DocSymbolKind::Function:
                return "함수";
            case DocSymbolKind::Class:
                return "클래스";
            case DocSymbolKind::Field:
                return "필드";
            case DocSymbolKind::Method:
                return "메서드";
            case DocSymbolKind::Parameter:
                return "매개변수";
            }
            return "";
        }

        std::string symbolMarkdown(const SymbolInfo& s) {
            std::string code;
            if (s.kind == DocSymbolKind::Function || s.kind == DocSymbolKind::Method
                || s.kind == DocSymbolKind::Class) {
                code = s.typeText; // 시그니처/클래스 표기 전체
            } else {
                code = s.typeText.empty() ? s.name : s.typeText + " " + s.name;
            }
            std::string md = "```hongik\n" + code + "\n```\n" + kindLabel(s.kind);
            if (!s.container.empty()) {
                md += " — " + s.container;
            }
            return md;
        }

        const BuiltinSignature* findBuiltin(const std::string& name) {
            for (const auto& sig : kBuiltinSignatures) {
                if (name == sig.name) {
                    return &sig;
                }
            }
            return nullptr;
        }

        std::string builtinMarkdown(const BuiltinSignature& sig) {
            std::string arity = sig.maxArity == INT_MAX ? std::to_string(sig.minArity) + "개 이상"
                              : sig.minArity == sig.maxArity
                                  ? std::to_string(sig.minArity) + "개"
                                  : std::to_string(sig.minArity) + "~" + std::to_string(sig.maxArity) + "개";
            return "내장 함수 `" + std::string(sig.name) + "` — 인자 " + arity;
        }

    } // namespace

    nlohmann::json hover(const AnalyzedDocument& doc, long long lspLine, long long lspChar) {
        if (lspLine < 0 || lspLine >= static_cast<long long>(doc.lines.size())) {
            return nullptr;
        }
        const long long cpCol = utf16ToCp(doc.lines[lspLine], lspChar);
        auto token            = tokenAt(doc, lspLine + 1, cpCol);
        if (!token || token->type != TokenType::IDENTIFIER) {
            return nullptr;
        }

        std::string value;
        if (const auto* sym = resolveSymbol(doc, token->text, lspLine + 1)) {
            value = symbolMarkdown(*sym);
        } else if (const auto* builtin = findBuiltin(token->text)) {
            value = builtinMarkdown(*builtin);
        } else {
            return nullptr;
        }
        return {{"contents", {{"kind", "markdown"}, {"value", value}}},
            {"range", toRange(doc, token->line, token->column, token->endColumn)}};
    }

} // namespace lsp::features
