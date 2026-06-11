#ifndef BUILTIN_SIGNATURES_H
#define BUILTIN_SIGNATURES_H

#include "object_type.h"

#include <climits>
#include <cstddef>

// Phase A 정적 타입 검사용 빌트인 시그니처 테이블 (spec D2.1).
// arity·반환 타입은 object/builtins/*.cpp 실측 기준 (plan Task 0, 2026-05-19).
//
// Phase A는 전 항목 skipArgTypeCheck=true — BuiltinFunctionType이 매개변수
// 타입을 보유하지 않으므로 인자 개수만 검사한다. Phase B에서 다중 오버로드로 확장.
// 반환 타입이 입력에 의존하는 다형 빌트인은 retIsAny=true.
//
// `반복`(Repeat)은 BuiltinRegistry에 등록되어 있으나 lexer 키워드 충돌로
// 사용자 코드에서 호출 불가능한 dead builtin이라 제외 (총 45개, spec D2.1).

struct BuiltinSignature {
    const char* name;
    int minArity;
    int maxArity;          // 가변 인자는 INT_MAX
    bool skipArgTypeCheck;
    ObjectType retKind;    // retIsAny == true면 무시
    bool retIsAny;
};

inline constexpr BuiltinSignature kBuiltinSignatures[] = {
    // I/O (io.cpp)
    {"출력", 0, INT_MAX, true, ObjectType::NULL_OBJ, false},   // io.cpp:13 (arity 검사 없음, 0개 허용)
    {"입력", 0, 1, true, ObjectType::STRING, false},           // io.cpp:56
    {"파일읽기", 1, 1, true, ObjectType::STRING, false},       // io.cpp:81
    {"파일쓰기", 2, 2, true, ObjectType::NULL_OBJ, false},     // io.cpp:119

    // 수학 (math.cpp)
    {"절대값", 1, 1, true, ObjectType::NULL_OBJ, true},        // math.cpp:12 (정수→정수, 실수→실수)
    {"제곱근", 1, 1, true, ObjectType::FLOAT, false},          // math.cpp:25
    {"최대", 2, 2, true, ObjectType::NULL_OBJ, true},          // math.cpp:43 (정수쌍→정수, 그외→실수)
    {"최소", 2, 2, true, ObjectType::NULL_OBJ, true},          // math.cpp:63
    {"난수", 2, 2, true, ObjectType::INTEGER, false},          // math.cpp:82
    {"사인", 1, 1, true, ObjectType::FLOAT, false},            // math.cpp:101
    {"코사인", 1, 1, true, ObjectType::FLOAT, false},          // math.cpp:116
    {"탄젠트", 1, 1, true, ObjectType::FLOAT, false},          // math.cpp:131
    {"로그", 1, 1, true, ObjectType::FLOAT, false},            // math.cpp:148
    {"자연로그", 1, 1, true, ObjectType::FLOAT, false},        // math.cpp:166
    {"거듭제곱", 2, 2, true, ObjectType::FLOAT, false},        // math.cpp:184
    {"파이", 0, 0, true, ObjectType::FLOAT, false},            // math.cpp:208
    {"자연수e", 0, 0, true, ObjectType::FLOAT, false},         // math.cpp:215
    {"반올림", 1, 1, true, ObjectType::INTEGER, false},        // math.cpp:224
    {"올림", 1, 1, true, ObjectType::INTEGER, false},          // math.cpp:237
    {"내림", 1, 1, true, ObjectType::INTEGER, false},          // math.cpp:250

    // 문자열 (string.cpp) — `반복`(string.cpp:123)은 dead builtin, 제외
    {"분리", 2, 2, true, ObjectType::ARRAY, false},            // string.cpp:13
    {"대문자", 1, 1, true, ObjectType::STRING, false},         // string.cpp:45
    {"소문자", 1, 1, true, ObjectType::STRING, false},         // string.cpp:56
    {"치환", 3, 3, true, ObjectType::STRING, false},           // string.cpp:67
    {"자르기", 1, 1, true, ObjectType::STRING, false},         // string.cpp:86
    {"시작확인", 2, 2, true, ObjectType::BOOLEAN, false},      // string.cpp:99
    {"끝확인", 2, 2, true, ObjectType::BOOLEAN, false},        // string.cpp:111
    {"채우기", 3, 3, true, ObjectType::STRING, false},         // string.cpp:148
    {"부분문자", 3, 3, true, ObjectType::STRING, false},       // string.cpp:179

    // 배열 (array.cpp)
    {"추가", 2, 2, true, ObjectType::NULL_OBJ, false},         // array.cpp:12
    {"정렬", 1, 1, true, ObjectType::ARRAY, false},            // array.cpp:24
    {"뒤집기", 1, 1, true, ObjectType::NULL_OBJ, true},        // array.cpp:51 (배열→배열, 문자→문자)
    {"찾기", 2, 2, true, ObjectType::INTEGER, false},          // array.cpp:68
    {"조각", 3, 3, true, ObjectType::ARRAY, false},            // array.cpp:84

    // 컬렉션 (collection.cpp)
    {"길이", 1, 1, true, ObjectType::INTEGER, false},          // collection.cpp:11
    {"키목록", 1, 1, true, ObjectType::ARRAY, false},          // collection.cpp:29
    {"포함", 2, 2, true, ObjectType::BOOLEAN, false},          // collection.cpp:46
    {"설정", 3, 3, true, ObjectType::NULL_OBJ, false},         // collection.cpp:73
    {"삭제", 2, 2, true, ObjectType::NULL_OBJ, false},         // collection.cpp:90

    // 변환 (conversion.cpp)
    {"타입", 1, 1, true, ObjectType::STRING, false},           // conversion.cpp:10
    {"정수변환", 1, 1, true, ObjectType::INTEGER, false},      // conversion.cpp:32
    {"실수변환", 1, 1, true, ObjectType::FLOAT, false},        // conversion.cpp:57
    {"문자변환", 1, 1, true, ObjectType::STRING, false},       // conversion.cpp:79

    // JSON (json.cpp)
    {"JSON_파싱", 1, 1, true, ObjectType::NULL_OBJ, true},     // json.cpp:278 (파싱 결과 의존)
    {"JSON_직렬화", 1, 1, true, ObjectType::STRING, false},    // json.cpp:290
};

inline constexpr std::size_t kBuiltinSignatureCount =
    sizeof(kBuiltinSignatures) / sizeof(kBuiltinSignatures[0]);

static_assert(kBuiltinSignatureCount == 45, "빌트인 시그니처는 45개 (반복 제외, spec D2.1)");

#endif // BUILTIN_SIGNATURES_H
