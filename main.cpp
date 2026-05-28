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

    // 기본 백엔드는 VM (5–10x 더 빠르고 S1/S4를 거치며 IOContext + ExecutionLimiter
    // 완전 와이어업 완료). 트리워킹 인터프리터를 강제하려면 --eval 플래그를 사용.
    // --vm 플래그는 호환성을 위해 남겨두지만 no-op이다.
    bool useVM = true;
    std::string filename;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--vm") {
            useVM = true; // 명시적 opt-in (no-op since default)
        } else if (arg == "--eval") {
            useVM = false; // 트리워킹 평가기로 opt-back
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
