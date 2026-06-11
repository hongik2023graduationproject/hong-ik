#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include "../exception/exception.h"
#include "../object/object.h"
#include "vm_value.h"

#include <memory>
#include <string>
#include <vector>

// vm.cpp / vm_handlers.cpp 공용 내부 헬퍼 (Phase 2-2 분할) — 외부 사용 금지
namespace vm_internal {

// 손상된 또는 외부에서 들어온 바이트코드가 상수 테이블 인덱스를 임의로 지정할 수 있으므로,
// 매 접근마다 size 검사로 OOB UB를 차단한다.
inline const std::shared_ptr<Object>& checkedConstant(
    const std::vector<std::shared_ptr<Object>>& constants, uint16_t idx, long long line) {
    if (idx >= constants.size()) {
        throw RuntimeException("상수 인덱스가 범위 밖입니다: " + std::to_string(idx), line);
    }
    return constants[idx];
}

inline ObjectType vmValueToObjectType(const VMValue& val) {
    switch (val.kind()) {
    case ValueTag::INT: return ObjectType::INTEGER;
    case ValueTag::FLOAT: return ObjectType::FLOAT;
    case ValueTag::BOOL: return ObjectType::BOOLEAN;
    case ValueTag::NULL_V: return ObjectType::NULL_OBJ;
    case ValueTag::OBJECT:
        if (val.asObject()) return val.asObject()->type;
        return ObjectType::NULL_OBJ;
    }
    return ObjectType::NULL_OBJ;
}

} // namespace vm_internal

#endif // VM_INTERNAL_H
