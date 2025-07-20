#include "repl/repl.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // 코드 페이지를 UTF-8로 설정
    SetConsoleCP(CP_UTF8);
#endif

    if (argc > 2) {
        std::cout << "사용법: hongik [script]"; // 추후 추가 예정
        return 0;
    }

    // 파일 모드
    if (argc == 2) {
        Repl repl;
        std::string filename = argv[1];
        repl.FileMode(filename);
    }

    // 인터프리터 모드
    if (argc == 1) {
        Repl repl;
        repl.TestLexer();
        // repl.TestParser();
        repl.Run();
    }

    return 0;
}
