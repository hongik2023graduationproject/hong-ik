#ifndef ANALYZER_H
#define ANALYZER_H

#include "../lexer/lexer.h"
#include "../token/token.h"
#include <memory>
#include <string>
#include <vector>

// 신택스 하이라이팅용 토큰 정보
struct TokenInfo {
    std::string type;           // TokenTypeToString 결과
    std::string value;          // 토큰 텍스트
    long long line = 0;
    std::string semanticType;   // "keyword", "variable", "number", "string", "operator", "builtin"
};

// 렉싱 결과 정보
struct AnalysisResult {
    bool success = true;
    std::vector<TokenInfo> tokens;
    std::string error;
    long long errorLine = 0;
};

// 코드 분석기 (신택스 하이라이팅 및 디버깅용)
class Analyzer {
public:
    // 코드에서 토큰 정보 추출
    static AnalysisResult GetTokens(const std::string& code);

    // 토큰 정보를 JSON 형식 문자열로 반환
    static std::string TokensToJson(const std::vector<TokenInfo>& tokens);

private:
    // 토큰 타입의 의미론적 타입 결정
    static std::string getSemanticType(const std::shared_ptr<Token>& token);
};

#endif // ANALYZER_H
