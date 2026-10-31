#include "repl.h"

#include "../lexer/lexer.h"
#include "../token/token.h"
#include "../utf8_converter/utf8_converter.h"
#include "../error/hongik_error.h"
#include "../exception/exception.h"
#include "../util/token_utils.h"
#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

namespace {

// 타입 진단 출력 (spec 4). strict일 때만 'type-error' 표기.
void printTypeDiagnostics(const vector<TypeDiagnostic>& diagnostics, bool strict) {
    for (const auto& d : diagnostics) {
        cerr << (strict ? "type-error" : "type-warning") << "[" << d.code << "]"
             << " [줄 " << d.line << "] " << d.message << endl;
    }
}

} // namespace

Repl::Repl(bool useVM, TypeCheckMode typeCheckMode) : useVM(useVM), typeCheckMode(typeCheckMode) {
    lexer     = make_unique<Lexer>();
    parser    = make_unique<Parser>();
    if (!useVM) {
        evaluator = make_unique<Evaluator>();
    }
    if (typeCheckMode != TypeCheckMode::Off) {
        typeChecker = make_unique<TypeChecker>();
    }
}

void Repl::Run() {
    cout << "한국어 프로그래밍 언어 프로젝트 홍익" << endl;
    cout << "제작: ezeun, jh-lee-kor, tolelom" << endl;
    if (useVM) {
        cout << "[VM 모드]" << endl;
    }
    if (typeCheckMode == TypeCheckMode::Strict) {
        // REPL은 항상 warn으로 동작 (spec D4)
        cout << "[알림] REPL에서는 strict 모드가 warn으로 동작합니다." << endl;
        typeCheckMode = TypeCheckMode::Warn;
    }

    vector<shared_ptr<Token>> tokens;
    int indent = 0;
    // VM REPL 세션 전역 상태 유지
    map<string, shared_ptr<Object>> vmGlobals;

    bool reachedEof = false;
    while (!reachedEof) {
        try {
            if (indent == 0) {
                cout << ">>> ";
            } else {
                cout << "    ";
            }

            string code;
            if (!getline(cin, code)) {
                // EOF(Ctrl+D, 파이프 입력 끝): 쌓인 여러 줄 입력이 있으면 빈 줄처럼 마무리해 실행하고 종료한다.
                reachedEof = true;
                cout << endl;
                if (tokens.empty()) {
                    return;
                }
                code.clear();
            }

            if (code == EXIT_COMMAND) {
                return;
            }
            code += "\n";

            vector<string> utf8_strings        = Utf8Converter::Convert(code);
            vector<shared_ptr<Token>> new_tokens = lexer->Tokenize(utf8_strings);
            tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

            if (tokens.size() >= 2 && (tokens[tokens.size() - 2]->type == TokenType::COLON)) {
                indent += 1;
            }
            for (auto& token : new_tokens) {
                if (token->type == TokenType::END_BLOCK) {
                    indent -= 1;
                }
            }

            if (indent != 0) {
                continue;
            }

            auto program = parser->Parsing(tokens);
            if (!parser->getErrors().empty()) {
                throw RuntimeException(parser->getErrors().front());
            }

            if (typeCheckMode != TypeCheckMode::Off) {
                // 단일 인스턴스 누적 check — 이전 입력의 글로벌/클래스 타입이 보임
                auto result = typeChecker->check(program);
                printTypeDiagnostics(result.diagnostics, false);
            }

            if (useVM) {
                Compiler compiler;
                auto bytecode = compiler.Compile(program);
                VM vm;
                vm.setGlobals(vmGlobals);
                auto object = vm.Execute(bytecode);
                vmGlobals = vm.getGlobals();
                if (object != nullptr && !dynamic_cast<Null*>(object.get())) {
                    cout << object->ToString() << endl;
                }
            } else {
                auto object = evaluator->Evaluate(program);
                if (object != nullptr) {
                    cout << object->ToString() << endl;
                }
            }

            tokens.clear();
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
            tokens.clear();
            indent = 0;
        }
    }
}

int Repl::FileMode(const string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: cannot open file: " << filename << std::endl;
        return 0;
    }

    // 파일 전체를 한 번에 토크나이즈/파싱한다.
    // (Tokenize는 호출마다 line을 리셋하는 REPL 지향 동작이라, 줄 단위 호출은
    //  톱레벨 문장의 줄 번호를 전부 1로 만든다 — 진단/에러 줄 번호 보존 목적)
    string code;
    vector<string> sourceLines;
    string fileLine;
    while (getline(file, fileLine)) {
        sourceLines.push_back(fileLine);
        code += fileLine;
        code += "\n";
    }
    auto utf8   = Utf8Converter::Convert(code);
    auto tokens = lexer->Tokenize(utf8);
    token_utils::appendMissingBlockClosers(tokens);
    if (tokens.empty()) return 0;

    try {
        auto program = parser->Parsing(tokens);
        if (!parser->getErrors().empty()) {
            throw RuntimeException(parser->getErrors().front());
        }
        if (typeCheckMode != TypeCheckMode::Off) {
            auto result = typeChecker->check(program);
            printTypeDiagnostics(result.diagnostics, typeCheckMode == TypeCheckMode::Strict);
            if (typeCheckMode == TypeCheckMode::Strict && !result.diagnostics.empty()) {
                return 1;
            }
        }
        // 실행 단계만 백엔드별로 다르다.
        shared_ptr<Object> object;
        if (useVM) {
            Compiler compiler;
            auto bytecode = compiler.Compile(program);
            VM vm;
            object = vm.Execute(bytecode);
        } else {
            object = evaluator->Evaluate(program);
        }
        // Null 객체는 출력하지 않는다 (builtin 부수효과 호출의 결과).
        if (object != nullptr && !dynamic_cast<Null*>(object.get())) {
            cout << object->ToString() << endl;
        }
    } catch (const RuntimeException& e) {
        string msg = e.what();
        // 줄 번호 추출 시도: "[줄 N] ..." 형식
        long long errorLine = 0;
        std::regex lineRegex("\\[줄 ([0-9]+)\\]");
        std::smatch match;
        if (std::regex_search(msg, match, lineRegex)) {
            errorLine = std::stoll(match[1].str());
        }
        if (errorLine > 0 && errorLine <= static_cast<long long>(sourceLines.size())) {
            cout << HongIkError::formatError(
                msg, errorLine, sourceLines[errorLine - 1]) << endl;
        } else {
            cout << "Error: " << msg << endl;
        }
    } catch (const exception& e) {
        cout << "Error: " << e.what() << endl;
    }
    return 0;
}
