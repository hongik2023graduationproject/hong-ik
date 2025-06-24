#include "repl/repl.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // 코드 페이지를 UTF-8로 설정
    SetConsoleCP(CP_UTF8);
#endif

    Repl repl;
    // repl.TestLexer();
    // repl.TestParser();
    repl.Run();
    return 0;
}
