#include "document_store.h"

#include "../exception/exception.h"
#include "../lexer/lexer.h"
#include "../utf8_converter/utf8_converter.h"
#include "../util/token_utils.h"
#include "position.h"

namespace lsp {

    AnalyzedDocument DocumentStore::analyze(std::string uri, std::string text, long long version) {
        AnalyzedDocument doc;
        doc.uri     = std::move(uri);
        doc.text    = std::move(text);
        doc.version = version;
        doc.lines   = splitLines(doc.text);

        // 파일 끝 개행 보장 — 없으면 최상위 블록문이 파싱에서 누락되는 기존 파서 한계 방어
        std::string source = doc.text;
        if (source.empty() || source.back() != '\n') {
            source += '\n';
        }

        try {
            Lexer lexer;
            doc.tokens = lexer.Tokenize(Utf8Converter::Convert(source));
            // 파일 끝에서 아직 닫히지 않은 들여쓰기 블록을 보정 — file mode(repl.cpp)와 동일한 관행.
            // 렉서는 dedent를 다음 줄의 들여쓰기를 읽을 때만 감지하므로, 블록 안에서 파일이 끝나면
            // 위 개행 보정만으로는 START_BLOCK이 닫히지 않아 최상위 구문이 파싱에서 누락된다.
            token_utils::appendMissingBlockClosers(doc.tokens);
        } catch (const ParseError& e) {
            // 렉서 에러 복구는 v2 — 파일 전체 분석 중단, 단일 진단 (spec v1.1 한계 ③)
            doc.parseDiagnostics.push_back({e.line, e.column, e.endColumn, e.what()});
            return doc;
        } catch (const std::exception& e) {
            doc.parseDiagnostics.push_back({0, 0, 0, e.what()});
            return doc;
        }

        Parser parser;
        doc.program          = parser.Parsing(doc.tokens);
        doc.parseDiagnostics = parser.getDiagnostics();

        if (doc.program) {
            try {
                TypeChecker checker; // 매 분석 새 인스턴스 — REPL용 글로벌 누적이 편집 간 오염을 만들기 때문
                doc.typeDiagnostics = checker.check(doc.program).diagnostics;
            } catch (...) {
                // 에러 복구된 부분 AST 방어 — 타입 진단만 생략하고 계속
            }
            try {
                SymbolCollector collector;
                doc.symbols = collector.collect(doc.program);
            } catch (...) {
                // 동일 방어
            }
        }
        return doc;
    }

    const AnalyzedDocument* DocumentStore::open(const std::string& uri, const std::string& text, long long version) {
        docs_[uri] = analyze(uri, text, version);
        return &docs_[uri];
    }

    const AnalyzedDocument* DocumentStore::update(const std::string& uri, const std::string& text, long long version) {
        return open(uri, text, version);
    }

    void DocumentStore::close(const std::string& uri) {
        docs_.erase(uri);
    }

    const AnalyzedDocument* DocumentStore::get(const std::string& uri) const {
        auto it = docs_.find(uri);
        return it == docs_.end() ? nullptr : &it->second;
    }

} // namespace lsp
