#ifndef LSP_JSONRPC_H
#define LSP_JSONRPC_H

#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace lsp {

    // LSP Base Protocol: "Content-Length: N\r\n\r\n<N바이트 UTF-8 본문>"
    std::optional<std::string> readMessage(std::istream& in);
    void writeMessage(std::ostream& out, const nlohmann::json& msg);

    class Dispatcher {
    public:
        using RequestHandler      = std::function<nlohmann::json(const nlohmann::json& params)>;
        using NotificationHandler = std::function<void(const nlohmann::json& params)>;

        void onRequest(const std::string& method, RequestHandler handler);
        void onNotification(const std::string& method, NotificationHandler handler);
        // 메시지 1건 처리. 요청이면 응답을 out에 기록, 알림이면 기록 없음.
        void handle(const nlohmann::json& msg, std::ostream& out);

    private:
        std::map<std::string, RequestHandler> requests_;
        std::map<std::string, NotificationHandler> notifications_;
    };

} // namespace lsp

#endif // LSP_JSONRPC_H
