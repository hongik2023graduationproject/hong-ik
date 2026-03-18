#include "wasm_interface.h"
#include "../util/json_util.h"
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

std::string WasmInterface::Execute(const std::string& code, long long timeoutMs, size_t maxMemoryBytes) {
    outputBuffer.clear();

    std::ostringstream json;

    try {
        ExecutionLimiter limiter(timeoutMs, maxMemoryBytes);

        // 기존 evaluator를 리미터와 함께 재생성
        evaluator = std::make_unique<Evaluator>(&ioCtx, &limiter);

        auto utf8Strings = Utf8Converter::Convert(code + "\n");
        auto tokens = lexer->Tokenize(utf8Strings);

        // 블록 종료 토큰 보정
        int indent = 0;
        for (const auto& t : tokens) {
            if (t->type == TokenType::START_BLOCK) indent++;
            if (t->type == TokenType::END_BLOCK) indent--;
        }
        for (int i = 0; i < indent; i++) {
            tokens.push_back(std::make_shared<Token>(Token{TokenType::END_BLOCK, "", 0}));
        }

        auto program = parser->Parsing(tokens);
        auto result = evaluator->Evaluate(program);

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
