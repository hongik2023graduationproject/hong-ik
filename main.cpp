#include "repl/repl.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    bool useVM = false;
    std::string filename;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--vm") {
            useVM = true;
        } else {
            filename = arg;
        }
    }

    if (filename.empty()) {
        // REPL 모드
        Repl repl(useVM);
        repl.Run();
    } else {
        // 파일 모드
        Repl repl(useVM);
        repl.FileMode(filename);
    }

    return 0;
}
