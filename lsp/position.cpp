#include "position.h"

namespace {
int utf8SeqLen(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xE) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1;  // 잘못된 리드 바이트 — 1바이트씩 전진 (방어)
}
} // namespace

namespace lsp {

long long cpToUtf16(const std::string& lineText, long long cpColumn) {
    long long cp = 0, u16 = 0;
    size_t i = 0;
    while (i < lineText.size() && cp < cpColumn) {
        int len = utf8SeqLen(static_cast<unsigned char>(lineText[i]));
        u16 += (len == 4) ? 2 : 1;  // BMP 밖(4바이트)만 서로게이트 쌍
        i += len;
        cp++;
    }
    return u16;
}

long long utf16ToCp(const std::string& lineText, long long utf16Col) {
    long long cp = 0, u16 = 0;
    size_t i = 0;
    while (i < lineText.size() && u16 < utf16Col) {
        int len = utf8SeqLen(static_cast<unsigned char>(lineText[i]));
        u16 += (len == 4) ? 2 : 1;
        i += len;
        cp++;
    }
    return cp;
}

long long cpLength(const std::string& lineText) {
    long long cp = 0;
    size_t i = 0;
    while (i < lineText.size()) {
        i += utf8SeqLen(static_cast<unsigned char>(lineText[i]));
        cp++;
    }
    return cp;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            if (!current.empty() && current.back() == '\r') current.pop_back();
            lines.push_back(std::move(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty() && current.back() == '\r') current.pop_back();
    lines.push_back(std::move(current));
    return lines;
}

} // namespace lsp
