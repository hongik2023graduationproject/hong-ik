#include "jsonrpc.h"

#include <stdexcept>
#include <string_view>

namespace lsp {

    std::optional<std::string> readMessage(std::istream& in) {
        long long contentLength = -1;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                break; // 헤더 종료
            }
            constexpr std::string_view kPrefix = "Content-Length:";
            if (line.rfind(kPrefix, 0) == 0) {
                try {
                    contentLength = std::stoll(line.substr(kPrefix.size()));
                } catch (const std::exception&) {
                    return std::nullopt;
                }
            }
        }
        if (!in || contentLength < 0) {
            return std::nullopt;
        }
        std::string body(static_cast<size_t>(contentLength), '\0');
        in.read(body.data(), contentLength);
        if (in.gcount() != contentLength) {
            return std::nullopt;
        }
        return body;
    }

    void writeMessage(std::ostream& out, const nlohmann::json& msg) {
        const std::string body = msg.dump();
        out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
        out.flush();
    }

    void Dispatcher::onRequest(const std::string& method, RequestHandler handler) {
        requests_[method] = std::move(handler);
    }

    void Dispatcher::onNotification(const std::string& method, NotificationHandler handler) {
        notifications_[method] = std::move(handler);
    }

    void Dispatcher::handle(const nlohmann::json& msg, std::ostream& out) {
        const std::string method    = msg.value("method", "");
        const nlohmann::json params = msg.contains("params") ? msg["params"] : nlohmann::json::object();

        if (!msg.contains("id")) { // 알림
            auto it = notifications_.find(method);
            if (it != notifications_.end()) {
                try {
                    it->second(params);
                } catch (...) {
                    // 알림은 응답 채널이 없다 — 서버 생존 우선, 무시
                }
            }
            return;
        }

        nlohmann::json response = {{"jsonrpc", "2.0"}, {"id", msg["id"]}};
        auto it                 = requests_.find(method);
        if (it == requests_.end()) {
            response["error"] = {{"code", -32601}, {"message", "method not found: " + method}};
        } else {
            try {
                response["result"] = it->second(params);
            } catch (const std::exception& e) {
                response["error"] = {{"code", -32603}, {"message", e.what()}};
            } catch (...) {
                response["error"] = {{"code", -32603}, {"message", "internal error"}};
            }
        }
        writeMessage(out, response);
    }

} // namespace lsp
