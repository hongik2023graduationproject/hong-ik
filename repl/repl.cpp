#include "repl.h"

#include "../lexer/lexer.h"
#include "../token/token.h"
#include "../utf8_converter/utf8_converter.h"
#include <fstream>
#include <iostream>

using namespace std;

Repl::Repl() {
    lexer     = new Lexer();
    parser    = new Parser();
    evaluator = new Evaluator();
}

void Repl::Run() {
    cout << "한국어 프로그래밍 언어 프로젝트 홍익" << endl;
    cout << "제작: ezeun, jh-lee-kor, tolelom" << endl;

    vector<Token*> tokens;
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

            // Repl 프롬포트를 끝내기 위한 명령어로 "종료하기"를 하드 코딩한 상태,
            // 별도 함수로 빼거나 하드 코딩 하지 않는 편이 좋아 보임
            if (code == "종료하기") {
                return;
            }
            code += "\n";

            vector<string> utf8_strings = Utf8Converter::Convert(code);
            vector<Token*> new_tokens   = lexer->Tokenize(utf8_strings);
            tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

            if (tokens.size() >= 2 && (tokens[tokens.size() - 2]->type == TokenType::COLON)) {
                indent += 1;
            }
            for (auto token : new_tokens) {
                if (token->type == TokenType::END_BLOCK) {
                    indent -= 1;
                }
            }

            if (indent != 0) {
                continue;
            }

            Program* program = parser->Parsing(tokens);
            Object* object   = evaluator->Evaluate(program);

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

void Repl::FileMode(string& filename) {
    vector<Token*> tokens;
    int indent = 0;

    // 파일 스트림 생성 및 열기
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: cannot open file: " << filename << std::endl;
        return;
    }

    string code;
    while (getline(file, code)) {
        try {
            vector<string> utf8_strings = Utf8Converter::Convert(code);
            vector<Token*> new_tokens   = lexer->Tokenize(utf8_strings);
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

            Program* program = parser->Parsing(tokens);
            Object* object   = evaluator->Evaluate(program);

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

    vector<Token*> tokens;
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

            // Repl 프롬포트를 끝내기 위한 명령어로 "종료하기"를 하드 코딩한 상태,
            // 별도 함수로 빼거나 하드 코딩 하지 않는 편이 좋아 보임
            if (code == "종료하기") {
                return;
            }
            code += "\n";

            vector<string> utf8_strings = Utf8Converter::Convert(code);
            vector<Token*> new_tokens   = lexer->Tokenize(utf8_strings);

            for (auto token : new_tokens) {
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

            for (auto token : tokens) {
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

    vector<Token*> tokens;
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

            // Repl 프롬포트를 끝내기 위한 명령어로 "종료하기"를 하드 코딩한 상태,
            // 별도 함수로 빼거나 하드 코딩 하지 않는 편이 좋아 보임
            if (code == "종료하기") {
                return;
            }
            code += "\n";

            vector<string> utf8_strings = Utf8Converter::Convert(code);
            vector<Token*> new_tokens   = lexer->Tokenize(utf8_strings);
            tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

            if (tokens.back()->type == TokenType::COLON) {
                indent += 1;
            }
            for (auto token : new_tokens) {
                if (token->type == TokenType::END_BLOCK) {
                    indent -= 1;
                }
            }

            if (indent != 0) {
                continue;
            }

            Program* program = parser->Parsing(tokens);

            cout << program->String() << endl;
            tokens.clear();
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
            tokens.clear();
            indent = 0;
        }
    }
}
