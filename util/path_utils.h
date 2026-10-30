#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <string_view>

namespace path_utils {

    // 사용자 코드가 넘긴 파일 경로(가져오기·파일읽기·파일쓰기)의 경로 탐색 방지 검사.
    // 위반이면 한국어 오류 메시지를, 통과하면 nullptr를 돌려준다 — 예외 타입·줄 번호는 호출부가 정한다.
    // 완전한 샌드박스는 아니다 (심볼릭 링크 등은 막지 않음).
    inline const char* userPathError(std::string_view path) noexcept {
        if (path.find('\0') != std::string_view::npos) {
            return "파일 경로에 null 바이트를 사용할 수 없습니다.";
        }
        if (path.find("..") != std::string_view::npos) {
            return "파일 경로에 '..'을 사용할 수 없습니다.";
        }
        if (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
            return "절대 경로는 사용할 수 없습니다.";
        }
        if (path.size() >= 2 && path[1] == ':') {
            return "절대 경로는 사용할 수 없습니다.";
        }
        return nullptr;
    }

} // namespace path_utils

#endif // PATH_UTILS_H
