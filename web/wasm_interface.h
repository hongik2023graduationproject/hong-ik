#ifndef WASM_INTERFACE_H
#define WASM_INTERFACE_H

#include <memory>
#include <string>
#include "../io/io_interface.h"
#include "../sandbox/execution_limiter.h"
#include "../analyzer/analyzer.h"
#include "../error/hongik_error.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../evaluator/evaluator.h"
#include "../utf8_converter/utf8_converter.h"
#include "memory_filesystem.h"

// 웹 환경용 WASM 인터페이스
class WasmInterface {
public:
    WasmInterface();

    // 코드 실행 → JSON 결과 반환
    // { "success": true, "output": "...", "result": "..." }
    // 또는 에러 시:
    // { "success": false, "error": { ... } }
    std::string Execute(
        const std::string& code,
        long long timeoutMs = ExecutionLimiter::DEFAULT_TIMEOUT_MS,
        size_t maxMemoryBytes = ExecutionLimiter::DEFAULT_MAX_MEMORY_BYTES
    );

    // 신택스 하이라이팅용 토큰 정보 반환 (JSON)
    std::string GetTokens(const std::string& code);

    // 인터프리터 상태 초기화
    void Reset();

    // 인메모리 파일시스템 접근
    MemoryFileSystem& GetFileSystem() { return memFs; }

    // 파일시스템 조작 (embind용 래퍼)
    void WriteFile(const std::string& path, const std::string& content);
    std::string ReadFile(const std::string& path);
    bool FileExists(const std::string& path);
    void DeleteFile(const std::string& path);

private:
    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<Parser> parser;
    std::unique_ptr<Evaluator> evaluator;

    IOContext ioCtx;
    MemoryFileSystem memFs;

    // 출력 캡처 버퍼
    std::string outputBuffer;

    // I/O 컨텍스트 초기화 (MemoryFileSystem 연동)
    void setupIOContext();
};

#endif // WASM_INTERFACE_H
