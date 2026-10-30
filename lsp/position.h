#ifndef LSP_POSITION_H
#define LSP_POSITION_H

#include <string>
#include <vector>

// 좌표 규약 (spec v1.1):
//   내부  = line 1-based, column 0-based 코드포인트 (Utf8Converter가 코드포인트 단위 분해)
//   LSP   = line 0-based, character 0-based UTF-16 코드 유닛
// 변환은 반드시 이 모듈을 통해서만 수행한다 (오프바이원 일원화).
namespace lsp {

long long cpToUtf16(const std::string& lineText, long long cpColumn);
long long utf16ToCp(const std::string& lineText, long long utf16Col);
long long cpLength(const std::string& lineText);
// '\n' 기준 분리, 각 줄 끝 '\r' 제거. 빈 입력도 빈 줄 1개를 반환한다.
std::vector<std::string> splitLines(const std::string& text);

} // namespace lsp

#endif // LSP_POSITION_H
