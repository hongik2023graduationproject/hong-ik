#include "io_interface.h"
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

IOContext IOContext::CreateConsoleIO() {
    IOContext ctx;

    ctx.print = [](const std::string& text) {
        std::cout << text;
    };

    ctx.input = [](const std::string& prompt) -> std::string {
        if (!prompt.empty()) {
            std::cout << prompt;
        }
        std::string line;
        std::getline(std::cin, line);
        return line;
    };

    ctx.fileRead = [](const std::string& path) -> std::string {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error(
                "파일을 열 수 없습니다: " + path + " (" + std::strerror(errno) + ")");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    };

    ctx.fileWrite = [](const std::string& path, const std::string& content) {
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error(
                "파일을 쓸 수 없습니다: " + path + " (" + std::strerror(errno) + ")");
        }
        file << content;
    };

    return ctx;
}

IOContext IOContext::CreateWebIO() {
    IOContext ctx;

    // 웹 환경에서는 이 콜백들이 JavaScript에서 오버라이드됨
    ctx.print = [](const std::string& text) {
        // WASM에서 console.log로 매핑됨
    };

    ctx.input = [](const std::string& prompt) -> std::string {
        // WASM에서 JavaScript prompt()로 매핑됨
        return "";
    };

    ctx.fileRead = [](const std::string& path) -> std::string {
        throw std::runtime_error("파일 I/O는 웹 환경에서 지원되지 않습니다.");
    };

    ctx.fileWrite = [](const std::string& path, const std::string& content) {
        throw std::runtime_error("파일 I/O는 웹 환경에서 지원되지 않습니다.");
    };

    return ctx;
}
