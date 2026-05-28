#include "builtin_registry.h"

namespace BuiltinRegistry {

std::unordered_map<std::string, std::shared_ptr<Builtin>> build(IOContext* ioCtx) {
    return {
        {"길이", std::make_shared<Length>()},
        {"출력", std::make_shared<Print>(ioCtx)},
        {"추가", std::make_shared<Push>()},
        {"타입", std::make_shared<TypeOf>()},
        {"정수변환", std::make_shared<ToInteger>()},
        {"실수변환", std::make_shared<ToFloat>()},
        {"문자변환", std::make_shared<ToString_>()},
        {"입력", std::make_shared<Input>(ioCtx)},
        {"키목록", std::make_shared<Keys>()},
        {"포함", std::make_shared<Contains>()},
        {"설정", std::make_shared<MapSet>()},
        {"삭제", std::make_shared<Remove>()},
        {"파일읽기", std::make_shared<FileRead>(ioCtx)},
        {"파일쓰기", std::make_shared<FileWrite>(ioCtx)},
        {"절대값", std::make_shared<Abs>()},
        {"제곱근", std::make_shared<Sqrt>()},
        {"최대", std::make_shared<Max>()},
        {"최소", std::make_shared<Min>()},
        {"난수", std::make_shared<Random>()},
        {"사인", std::make_shared<Sin>()},
        {"코사인", std::make_shared<Cos>()},
        {"탄젠트", std::make_shared<Tan>()},
        {"로그", std::make_shared<Log>()},
        {"자연로그", std::make_shared<Ln>()},
        {"거듭제곱", std::make_shared<Power>()},
        {"파이", std::make_shared<Pi>()},
        {"자연수e", std::make_shared<EulerE>()},
        {"반올림", std::make_shared<Round>()},
        {"올림", std::make_shared<Ceil>()},
        {"내림", std::make_shared<Floor>()},
        {"분리", std::make_shared<Split>()},
        {"대문자", std::make_shared<ToUpper>()},
        {"소문자", std::make_shared<ToLower>()},
        {"치환", std::make_shared<Replace>()},
        {"자르기", std::make_shared<Trim>()},
        {"시작확인", std::make_shared<StartsWith>()},
        {"끝확인", std::make_shared<EndsWith>()},
        {"반복", std::make_shared<Repeat>()},
        {"채우기", std::make_shared<Pad>()},
        {"부분문자", std::make_shared<Substring>()},
        {"정렬", std::make_shared<Sort>()},
        {"뒤집기", std::make_shared<Reverse>()},
        {"찾기", std::make_shared<Find>()},
        {"조각", std::make_shared<Slice>()},
        {"JSON_파싱", std::make_shared<JsonParse>()},
        {"JSON_직렬화", std::make_shared<JsonSerialize>()},
    };
}

const std::unordered_set<std::string>& names() {
    // build()와 동일한 키. build()의 빈 맵을 만드는 비용을 피하기 위해
    // 정적으로 분리해 두지만, 새 빌트인 추가 시 양쪽 모두 갱신 필요.
    static const std::unordered_set<std::string> kNames = {
        "길이",     "출력",     "추가",     "타입",     "정수변환",   "실수변환",
        "문자변환", "입력",     "키목록",   "포함",     "설정",       "삭제",
        "파일읽기", "파일쓰기", "절대값",   "제곱근",   "최대",       "최소",
        "난수",     "사인",     "코사인",   "탄젠트",   "로그",       "자연로그",
        "거듭제곱", "파이",     "자연수e",  "반올림",   "올림",       "내림",
        "분리",     "대문자",   "소문자",   "치환",     "자르기",     "시작확인",
        "끝확인",   "반복",     "채우기",   "부분문자", "정렬",       "뒤집기",
        "찾기",     "조각",     "JSON_파싱", "JSON_직렬화",
    };
    return kNames;
}

const std::unordered_set<std::string>& specialOperatorNames() {
    // 평가기에서 별도 처리되는 연산자성 식별자 (build()에는 없지만 신택스
    // 하이라이팅상 빌트인으로 보여야 함).
    static const std::unordered_set<std::string> kSpecialOps = {
        "매핑", "걸러내기", "줄이기",
    };
    return kSpecialOps;
}

} // namespace BuiltinRegistry
