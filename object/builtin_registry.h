#ifndef BUILTIN_REGISTRY_H
#define BUILTIN_REGISTRY_H

#include "../io/io_interface.h"
#include "built_in.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

// 단일 진실의 원천: 빌트인 함수 등록.
//
// 이전에는 Evaluator/VM/Analyzer 각각 자체 목록을 들고 있어서 새 빌트인 추가 시
// 세 곳을 수정해야 했고 실제로 divergence가 발생했다 (예: Analyzer가 16개 빌트인
// 누락, VM이 IOContext 미연결로 출력이 cout 직행).
//
// 모든 호출부는 BuiltinRegistry::build(ioCtx) 또는 BuiltinRegistry::names()를
// 거치므로, 새 빌트인은 한 곳만 수정하면 된다.
namespace BuiltinRegistry {

// 런타임 디스패치용 전체 맵. ioCtx가 non-null이면 Print/Input/FileRead/FileWrite가
// 콜백을 통해 라우팅되며, null이면 기본 stdio/파일시스템 동작.
std::unordered_map<std::string, std::shared_ptr<Builtin>> build(IOContext* ioCtx = nullptr);

// Analyzer(신택스 하이라이팅) 등 이름만 필요한 경우.
// build()의 키와 정확히 일치한다.
const std::unordered_set<std::string>& names();

// 특수 식별자(빌트인이 아니지만 빌트인처럼 보여야 하는 것).
// 예: 매핑/걸러내기/줄이기는 평가기에서 별도 키워드/연산자로 처리되지만,
// 신택스 하이라이팅 측면에서는 "빌트인"으로 분류한다.
const std::unordered_set<std::string>& specialOperatorNames();

} // namespace BuiltinRegistry

#endif // BUILTIN_REGISTRY_H
