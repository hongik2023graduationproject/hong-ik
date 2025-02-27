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
    // cout << "한국어 프로그래밍 언어 프로젝트 홍익" << endl;
    // cout << "제작: ezeun, jh-lee-kor, tolelom" << endl;

    while (true) {
        string code;
        cout << ">>> ";
        getline(cin, code);

        vector<string> utf8_strings = Utf8Converter::Convert(code);
        vector<Token *> tokens = lexer->Tokenize(utf8_strings);
        Program *program = parser->Parsing(tokens);
        vector<Object*> objects = evaluator->evaluate(program);

        for (auto object : objects) {
            cout << object->String() << endl;
        }
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
    while (true) {
        string code;
        cout << ">>> ";
        getline(cin, code);

        vector<string> utf8_strings = Utf8Converter::Convert(code);
        vector<Token *> tokens = lexer->Tokenize(utf8_strings);
        Program *program = parser->Parsing(tokens);

        cout << program->String() << endl;
    }
}


