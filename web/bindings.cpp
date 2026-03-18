#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include "wasm_interface.h"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(hongik_module) {
    class_<WasmInterface>("HongIkInterpreter")
        .constructor<>()
        // execute: 기본 타임아웃/메모리 제한 사용하는 래퍼
        .function("execute", optional_override([](WasmInterface& self, const std::string& code) {
            return self.Execute(code);
        }))
        // executeWithLimits: 커스텀 제한 설정
        .function("executeWithLimits", optional_override([](WasmInterface& self, const std::string& code, int timeoutMs, int maxMemoryBytes) {
            return self.Execute(code, static_cast<long long>(timeoutMs), static_cast<size_t>(maxMemoryBytes));
        }))
        .function("getTokens", &WasmInterface::GetTokens)
        .function("reset", &WasmInterface::Reset)
        .function("writeFile", &WasmInterface::WriteFile)
        .function("readFile", &WasmInterface::ReadFile)
        .function("fileExists", &WasmInterface::FileExists)
        .function("deleteFile", &WasmInterface::DeleteFile);
}

#endif // __EMSCRIPTEN__
