#include "wasm_interface.h"
#include "../exception/exception.h"
#include "../util/json_util.h"
#include "../util/token_utils.h"
#include <sstream>

WasmInterface::WasmInterface() {
    setupIOContext();
    lexer = std::make_unique<Lexer>();
    parser = std::make_unique<Parser>();
    evaluator = std::make_unique<Evaluator>(&ioCtx, nullptr);
}

void WasmInterface::setupIOContext() {
    // 출력을 버퍼에 캡처
    ioCtx.print = [this](const std::string& text) {
        outputBuffer += text;
    };

    // 웹에서 input은 빈 문자열 반환 (JS에서 오버라이드 가능)
    ioCtx.input = [](const std::string& prompt) -> std::string {
        return "";
    };

    // 인메모리 파일시스템 연동
    ioCtx.fileRead = [this](const std::string& path) -> std::string {
        return memFs.ReadFile(path);
    };

    ioCtx.fileWrite = [this](const std::string& path, const std::string& content) {
        memFs.WriteFile(path, content);
    };
}

std::shared_ptr<Object> WasmInterface::executeWithEvaluator(std::shared_ptr<Program> program,
                                                              ExecutionLimiter& limiter) {
    // 매 호출마다 리미터 결합 새 evaluator 생성 (Evaluator는 ctor 시점에만 limiter를 받음).
    evaluator = std::make_unique<Evaluator>(&ioCtx, &limiter);
    return evaluator->Evaluate(program);
}

std::shared_ptr<Object> WasmInterface::executeWithVM(std::shared_ptr<Program> program,
                                                      ExecutionLimiter& limiter) {
    // VM은 호출마다 새로 만든다 (호출 간 전역 상태 공유 없음, evaluator 경로와 동일).
    Compiler compiler;
    auto bytecode = compiler.Compile(program);
    VM vm(&ioCtx, &limiter);
    return vm.Execute(bytecode);
}

std::string WasmInterface::Execute(const std::string& code, long long timeoutMs) {
    outputBuffer.clear();

    std::ostringstream json;

    try {
        ExecutionLimiter limiter(timeoutMs);

        auto utf8Strings = Utf8Converter::Convert(code + "\n");
        auto tokens = lexer->Tokenize(utf8Strings);
        token_utils::appendMissingBlockClosers(tokens);

        auto program = parser->Parsing(tokens);
        // 파서가 에러를 errors{}에 모으기만 하고 그대로 진행하면, 깨진 AST가 silent하게 실행돼
        // 사용자에게 진단이 전혀 노출되지 않는다. 첫 파싱 오류를 surface한다.
        if (!parser->getErrors().empty()) {
            throw RuntimeException(parser->getErrors().front());
        }

        auto result = useVM ? executeWithVM(program, limiter)
                            : executeWithEvaluator(program, limiter);

        json << "{\"success\":true";
        json << ",\"output\":\"" << json_util::escape(outputBuffer) << "\"";
        if (result != nullptr) {
            json << ",\"result\":\"" << json_util::escape(result->ToString()) << "\"";
        } else {
            json << ",\"result\":null";
        }
        json << "}";

    } catch (const HongIkError& e) {
        json << "{\"success\":false";
        json << ",\"output\":\"" << json_util::escape(outputBuffer) << "\"";
        json << ",\"error\":" << e.toJsonString();
        json << "}";

    } catch (const std::exception& e) {
        json << "{\"success\":false";
        json << ",\"output\":\"" << json_util::escape(outputBuffer) << "\"";
        json << ",\"error\":{";
        json << "\"type\":\"RuntimeError\"";
        json << ",\"typeCode\":1";
        json << ",\"message\":\"" << json_util::escape(e.what()) << "\"";
        json << ",\"location\":{\"line\":0,\"column\":0,\"endLine\":0,\"endColumn\":0}";
        json << "}}";
    }

    // limiter는 스택 변수이므로, 실행 후 evaluator를 리미터 없이 재생성
    evaluator = std::make_unique<Evaluator>(&ioCtx, nullptr);

    return json.str();
}

void WasmInterface::SetBackend(const std::string& name) {
    if (name == "vm")        useVM = true;
    else if (name == "evaluator") useVM = false;
    // 그 외 값은 무시 (호출자 오타로 silent flip되는 것을 막는다).
}

std::string WasmInterface::GetBackend() const {
    return useVM ? "vm" : "evaluator";
}

std::string WasmInterface::GetTokens(const std::string& code) {
    auto result = Analyzer::GetTokens(code);

    if (result.success) {
        return Analyzer::TokensToJson(result.tokens);
    }

    // 에러 시 에러 정보 포함
    std::ostringstream os;
    os << "{\"success\":false";
    os << ",\"error\":\"" << json_util::escape(result.error) << "\"";
    if (result.errorLine > 0) {
        os << ",\"errorLine\":" << result.errorLine;
    }
    os << "}";
    return os.str();
}

void WasmInterface::Reset() {
    outputBuffer.clear();
    setupIOContext();
    lexer = std::make_unique<Lexer>();
    parser = std::make_unique<Parser>();
    evaluator = std::make_unique<Evaluator>(&ioCtx, nullptr);
    memFs.Clear();
}

void WasmInterface::WriteFile(const std::string& path, const std::string& content) {
    memFs.WriteFile(path, content);
}

std::string WasmInterface::ReadFile(const std::string& path) {
    return memFs.ReadFile(path);
}

bool WasmInterface::FileExists(const std::string& path) {
    return memFs.FileExists(path);
}

void WasmInterface::DeleteFile(const std::string& path) {
    memFs.DeleteFile(path);
}
