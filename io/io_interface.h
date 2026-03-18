#ifndef IO_INTERFACE_H
#define IO_INTERFACE_H

#include <functional>
#include <string>

// 콜백 기반 I/O 인터페이스
class IOContext {
public:
    using PrintCallback = std::function<void(const std::string&)>;
    using InputCallback = std::function<std::string(const std::string&)>;
    using FileReadCallback = std::function<std::string(const std::string&)>;
    using FileWriteCallback = std::function<void(const std::string&, const std::string&)>;

    PrintCallback print;
    InputCallback input;
    FileReadCallback fileRead;
    FileWriteCallback fileWrite;

    // 기본 콘솔 구현
    static IOContext CreateConsoleIO();

    // 웹 환경용 (공 구현 - 웹에서 JavaScript로 오버라이드)
    static IOContext CreateWebIO();
};

#endif // IO_INTERFACE_H
