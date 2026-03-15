#include "repl.h"

#include "../lexer/lexer.h"
#include "../token/token.h"
#include "../utf8_converter/utf8_converter.h"
#include <fstream>
#include <iostream>

using namespace std;

Repl::Repl(bool useVM) : useVM(useVM) {
    lexer     = make_unique<Lexer>();
    parser    = make_unique<Parser>();
    if (!useVM) {
        evaluator = make_unique<Evaluator>();
    }
}

void Repl::Run() {
    cout << "한국어 프로그래밍 언어 프로젝트 홍익" << endl;
    cout << "제작: ezeun, jh-lee-kor, tolelom" << endl;
    if (useVM) {
        cout << "[VM 모드]" << endl;
    }

    vector<shared_ptr<Token>> tokens;
    int indent = 0;

    while (true) {
        try {
            if (indent == 0) {
                cout << ">>> ";
            } else {
                cout << "    ";
            }

            string code;
            getline(cin, code);

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

            if (useVM) {
                Compiler compiler;
                auto bytecode = compiler.Compile(program);
                VM vm;
                auto object = vm.Execute(bytecode);
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

void Repl::FileMode(const string& filename) {
    vector<shared_ptr<Token>> tokens;
    int indent = 0;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: cannot open file: " << filename << std::endl;
        return;
    }

    if (useVM) {
        // VM 모드: 파일 전체를 한 번에 파싱 후 컴파일/실행
        string code;
        while (getline(file, code)) {
            code += "\n";
            auto utf8 = Utf8Converter::Convert(code);
            auto newTokens = lexer->Tokenize(utf8);
            tokens.insert(tokens.end(), newTokens.begin(), newTokens.end());
            for (auto& t : newTokens) {
                if (t->type == TokenType::START_BLOCK) indent++;
                if (t->type == TokenType::END_BLOCK) indent--;
            }
        }
        for (int i = 0; i < indent; i++) {
            tokens.push_back(make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
        }
        if (tokens.empty()) return;

        try {
            auto program = parser->Parsing(tokens);
            Compiler compiler;
            auto bytecode = compiler.Compile(program);
            VM vm;
            auto object = vm.Execute(bytecode);
            if (object != nullptr && !dynamic_cast<Null*>(object.get())) {
                cout << object->ToString() << endl;
            }
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
        }
        return;
    }

    // 트리워킹 모드
    string code;
    while (getline(file, code)) {
        try {
            vector<string> utf8_strings        = Utf8Converter::Convert(code);
            vector<shared_ptr<Token>> new_tokens = lexer->Tokenize(utf8_strings);
            tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

            if (tokens.back()->type == TokenType::START_BLOCK) {
                indent += 1;
            }
            if (tokens.back()->type == TokenType::END_BLOCK) {
                indent -= 1;
            }

            if (indent != 0) {
                continue;
            }

            auto program = parser->Parsing(tokens);
            auto object  = evaluator->Evaluate(program);

            if (object != nullptr) {
                cout << object->ToString() << endl;
            }

            tokens.clear();
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
            tokens.clear();
            indent = 0;
        }
    }
}

void Repl::TestLexer() {
    cout << "한국어 프로그래밍 언어 프로젝트 홍익" << endl;
    cout << "제작: ezeun, jh-lee-kor, tolelom" << endl;
    cout << "Lexer Test Mode" << endl;

    vector<shared_ptr<Token>> tokens;
    int indent = 0;

    while (true) {
        try {
            if (indent == 0) {
                cout << ">>> ";
            } else {
                cout << "    ";
            }

            string code;
            getline(cin, code);

            if (code == EXIT_COMMAND) {
                return;
            }
            code += "\n";

            vector<string> utf8_strings        = Utf8Converter::Convert(code);
            vector<shared_ptr<Token>> new_tokens = lexer->Tokenize(utf8_strings);

            for (auto& token : new_tokens) {
                if (token == new_tokens.back() && token->type == TokenType::COLON) {
                    indent += 1;
                }
                if (token->type == TokenType::END_BLOCK) {
                    indent -= 1;
                }
            }

            tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

            if (indent != 0) {
                continue;
            }

            for (auto& token : tokens) {
                cout << token->line << ' ' << TokenTypeToString(token->type) << ' ' << token->text << endl;
            }

            tokens.clear();
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
            tokens.clear();
            indent = 0;
        }
    }
}

void Repl::TestParser() {
    cout << "한국어 프로그래밍 언어 프로젝트 홍익" << endl;
    cout << "제작: ezeun, jh-lee-kor, tolelom" << endl;
    cout << "Parser Test Mode" << endl;

    vector<shared_ptr<Token>> tokens;
    int indent = 0;

    while (true) {
        try {
            if (indent == 0) {
                cout << ">>> ";
            } else {
                cout << "    ";
            }

            string code;
            getline(cin, code);

            if (code == EXIT_COMMAND) {
                return;
            }
            code += "\n";

            vector<string> utf8_strings        = Utf8Converter::Convert(code);
            vector<shared_ptr<Token>> new_tokens = lexer->Tokenize(utf8_strings);
            tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

            if (tokens.back()->type == TokenType::COLON) {
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

            cout << program->String() << endl;
            tokens.clear();
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
            tokens.clear();
            indent = 0;
        }
    }
}
