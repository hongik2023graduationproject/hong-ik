#include "jsonrpc.h"
#include "server.h"
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main() {
#ifdef _WIN32
    // CRLF 변환이 Content-Length 바이트 계산을 깨뜨린다 — 바이너리 모드 필수 (spec 리스크 4)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    lsp::Dispatcher dispatcher;
    lsp::Server server(std::cout);
    server.bind(dispatcher);

    while (!server.shouldExit()) {
        auto body = lsp::readMessage(std::cin);
        if (!body) {
            break; // EOF — 클라이언트 종료
        }
        auto msg = nlohmann::json::parse(*body, nullptr, /*allow_exceptions=*/false);
        if (msg.is_discarded()) {
            continue; // 손상 프레임 — 다음 메시지 계속
        }
        dispatcher.handle(msg, std::cout);
    }
    return 0;
}
