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
    TypeCheckMode typeCheckMode = TypeCheckMode::Warn;  // Phase A 기본값 (spec D4)
    std::string filename;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--vm") {
            useVM = true; // 명시적 opt-in (no-op since default)
        } else if (arg == "--eval") {
            useVM = false; // 트리워킹 평가기로 opt-back
        } else if (arg.rfind("--type-check=", 0) == 0) {
            std::string mode = arg.substr(13);
            if (mode == "off") {
                typeCheckMode = TypeCheckMode::Off;
            } else if (mode == "warn") {
                typeCheckMode = TypeCheckMode::Warn;
            } else if (mode == "strict") {
                typeCheckMode = TypeCheckMode::Strict;
            } else {
                std::cerr << "알 수 없는 --type-check 값: " << mode
                          << " (off|warn|strict)" << std::endl;
                return 1;
            }
        } else {
            filename = arg;
        }
    }

    if (filename.empty()) {
        // REPL 모드
        Repl repl(useVM, typeCheckMode);
        repl.Run();
        return 0;
    }

    // 파일 모드
    Repl repl(useVM, typeCheckMode);
    return repl.FileMode(filename);
}
