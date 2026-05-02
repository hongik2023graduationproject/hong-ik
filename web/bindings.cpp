#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include "wasm_interface.h"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(hongik_module) {
    class_<WasmInterface>("HongIkInterpreter")
        .constructor<>()
        // execute: 기본 타임아웃 사용하는 래퍼
        .function("execute", optional_override([](WasmInterface& self, const std::string& code) {
            return self.Execute(code);
        }))
        // executeWithTimeout: 커스텀 타임아웃 설정
        .function("executeWithTimeout", optional_override([](WasmInterface& self, const std::string& code, int timeoutMs) {
            return self.Execute(code, static_cast<long long>(timeoutMs));
        }))
        .function("getTokens", &WasmInterface::GetTokens)
        .function("reset", &WasmInterface::Reset)
        .function("writeFile", &WasmInterface::WriteFile)
        .function("readFile", &WasmInterface::ReadFile)
        .function("fileExists", &WasmInterface::FileExists)
        .function("deleteFile", &WasmInterface::DeleteFile);
}

#endif // __EMSCRIPTEN__
