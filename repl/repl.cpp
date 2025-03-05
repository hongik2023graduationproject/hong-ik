#include "repl.h"
#include <iostream>

#include "../utf8_converter/utf8_converter.h"
#include "../token/token.h"
#include "../lexer/lexer.h"

using namespace std;

Repl::Repl() {
    lexer = new Lexer();
    parser = new Parser();
    evaluator = new Evaluator();
}


void Repl::Run() {
    cout << "한국어 프로그래밍 언어 프로젝트 홍익" << endl;
    cout << "제작: ezeun, jh-lee-kor, tolelom" << endl;

    vector<Token *> tokens;
    int indent = 0;
    auto* env = new Environment();

    while (true) {
        if (indent == 0) {
            cout << ">>> ";
        } else {
            for (int i = 0; i <= indent; i++) {
                cout << "    ";
            }
        }

        string code;
        getline(cin, code);
        if (code == "종료하기") {
            return;
        }

        vector<string> utf8_strings = Utf8Converter::Convert(code);
        vector<Token *> new_tokens = lexer->Tokenize(utf8_strings);
        tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

        // LBRACE, RBRACE는 각각 START_BLOCK, END_BLOCK을 임시로 대체하고 있다.
        // 나중에 교체가 되면 코드 수정이 필요하다.
        if (tokens.back()->type == TokenType::LBRACE) {
            indent += 1;
        }
        if (tokens.back()->type == TokenType::RBRACE) {
            indent -= 1;
        }

        if (indent != 0) {
            continue;
        }

        Program *program = parser->Parsing(tokens);
        vector<Object *> objects = evaluator->evaluate(program, env);

        for (auto object: objects) {
            cout << object->String() << endl;
        }

        tokens.clear();
    }
};


void Repl::TestLexer() {
    while (true) {
        string code;
        cout << ">>> ";
        getline(cin, code);

        vector<string> utf8_strings = Utf8Converter::Convert(code);
        vector<Token *> tokens = lexer->Tokenize(utf8_strings);

        // lexer 디버깅 코드
        // 추후에 parser 작성 시 삭제
        for (auto token: tokens) {
            cout << token->line << ' ' << TokenTypeToString(token->type) << ' ' << token->text << endl;
        }
    }
}

void Repl::TestParser() {
    vector<Token *> tokens;
    int indent = 0;

    while (true) {
        string code;

        if (indent == 0) {
            cout << ">>> ";
        } else {
            for (int i = 0; i <= indent; i++) {
                cout << "    ";
            }
        }
        getline(cin, code);


        vector<string> utf8_strings = Utf8Converter::Convert(code);
        vector<Token *> new_tokens = lexer->Tokenize(utf8_strings);
        tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());

        if (tokens.back()->type == TokenType::LBRACE) {
            indent += 1;
        }
        if (tokens.back()->type == TokenType::RBRACE) {
            indent -= 1;
        }

        if (indent != 0) {
            continue;
        }

        Program *program = parser->Parsing(tokens);

        cout << program->String() << endl;
        tokens.clear();
    }
}
