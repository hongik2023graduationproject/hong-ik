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
#include "../vm/compiler.h"
#include "../vm/vm.h"
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
        long long timeoutMs = ExecutionLimiter::DEFAULT_TIMEOUT_MS
    );

    // 신택스 하이라이팅용 토큰 정보 반환 (JSON)
    std::string GetTokens(const std::string& code);

    // 인터프리터 상태 초기화
    void Reset();

    // 백엔드 선택: "vm" 또는 "evaluator". 알 수 없는 값은 무시.
    // VM은 5–10x 빠르지만, evaluator 경로가 fallback으로 남아 있다.
    void SetBackend(const std::string& name);

    // 현재 백엔드 이름 ("vm" 또는 "evaluator"). 디버깅/JS 측 확인용.
    std::string GetBackend() const;

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

    // 백엔드 선택. 초기값 false(=evaluator)로 두면 기존 호출자 호환성 유지.
    // S3.3에서 true로 flip 예정.
    bool useVM = false;

    // I/O 컨텍스트 초기화 (MemoryFileSystem 연동)
    void setupIOContext();

    // 백엔드별 실행 helper. 각각 outputBuffer를 채우고 result를 반환한다.
    std::shared_ptr<Object> executeWithEvaluator(std::shared_ptr<Program> program,
                                                  ExecutionLimiter& limiter);
    std::shared_ptr<Object> executeWithVM(std::shared_ptr<Program> program,
                                           ExecutionLimiter& limiter);
};

#endif // WASM_INTERFACE_H
