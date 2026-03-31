#include "analyzer.h"
#include "../utf8_converter/utf8_converter.h"
#include "../util/json_util.h"
#include <sstream>
#include <unordered_set>

// 내장 함수 목록
static const std::unordered_set<std::string> BUILTINS = {
    "출력",     "입력",     "길이",     "추가",     "타입",     "정수변환", "실수변환",
    "문자변환", "키목록",   "포함",     "설정",     "삭제",     "파일읽기", "파일쓰기",
    "절대값",   "제곱근",   "최대",     "최소",     "난수",     "분리",     "대문자",
    "소문자",   "치환",     "자르기",   "정렬",     "뒤집기",   "찾기",     "조각",
    "매핑",     "걸러내기", "줄이기",
};

std::string Analyzer::getSemanticType(const std::shared_ptr<Token>& token) {
    if (!token) return "unknown";

    switch (token->type) {
    // 키워드
    case TokenType::정수:
    case TokenType::실수:
    case TokenType::문자:
    case TokenType::리턴:
    case TokenType::만약:
    case TokenType::라면:
    case TokenType::아니면:
    case TokenType::함수:
    case TokenType::반복:
    case TokenType::동안:
    case TokenType::중단:
    case TokenType::계속:
    case TokenType::논리:
    case TokenType::배열:
    case TokenType::사전:
    case TokenType::각각:
    case TokenType::에서:
    case TokenType::시도:
    case TokenType::실패:
    case TokenType::가져오기:
    case TokenType::없음:
    case TokenType::비교:
    case TokenType::경우:
    case TokenType::기본:
    case TokenType::클래스:
    case TokenType::생성:
    case TokenType::자기:
    case TokenType::부모:
    case TokenType::부터:
    case TokenType::까지:
        return "keyword";

    // 리터럴
    case TokenType::INTEGER:
    case TokenType::FLOAT:
        return "number";
    case TokenType::STRING:
        return "string";
    case TokenType::TRUE:
    case TokenType::FALSE:
        return "boolean";

    // 식별자 (내장 함수인지 확인)
    case TokenType::IDENTIFIER:
        if (BUILTINS.count(token->text)) {
            return "builtin";
        }
        return "variable";

    // 연산자 및 기호
    case TokenType::PLUS:
    case TokenType::MINUS:
    case TokenType::ASTERISK:
    case TokenType::SLASH:
    case TokenType::PERCENT:
    case TokenType::ASSIGN:
    case TokenType::EQUAL:
    case TokenType::NOT_EQUAL:
    case TokenType::PLUS_ASSIGN:
    case TokenType::MINUS_ASSIGN:
    case TokenType::ASTERISK_ASSIGN:
    case TokenType::SLASH_ASSIGN:
    case TokenType::PERCENT_ASSIGN:
    case TokenType::LESS_THAN:
    case TokenType::GREATER_THAN:
    case TokenType::LESS_EQUAL:
    case TokenType::GREATER_EQUAL:
    case TokenType::BANG:
    case TokenType::LOGICAL_AND:
    case TokenType::LOGICAL_OR:
    case TokenType::BITWISE_AND:
    case TokenType::BITWISE_OR:
    case TokenType::RIGHT_ARROW:
    case TokenType::DOT:
    case TokenType::ELLIPSIS:
    case TokenType::QUESTION:
        return "operator";

    // 구분자
    case TokenType::LPAREN:
    case TokenType::RPAREN:
    case TokenType::LBRACE:
    case TokenType::RBRACE:
    case TokenType::LBRACKET:
    case TokenType::RBRACKET:
    case TokenType::COLON:
    case TokenType::SEMICOLON:
    case TokenType::COMMA:
        return "punctuation";

    default:
        return "unknown";
    }
}

AnalysisResult Analyzer::GetTokens(const std::string& code) {
    AnalysisResult result;
    try {
        auto utf8Strings = Utf8Converter::Convert(code);
        Lexer lexer;
        auto tokens = lexer.Tokenize(utf8Strings);

        for (const auto& token : tokens) {
            // 렌더링 불필요한 토큰 스킵
            if (token->type == TokenType::NEW_LINE || token->type == TokenType::END_OF_FILE ||
                token->type == TokenType::END_BLOCK || token->type == TokenType::START_BLOCK ||
                token->type == TokenType::SPACE || token->type == TokenType::TAB) {
                continue;
            }

            TokenInfo info;
            info.type = TokenTypeToString(token->type);
            info.value = token->text;
            info.line = token->line;
            info.semanticType = getSemanticType(token);

            result.tokens.push_back(info);
        }
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
    }

    return result;
}

std::string Analyzer::TokensToJson(const std::vector<TokenInfo>& tokens) {
    std::ostringstream os;
    os << "{\"tokens\":[";

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (i > 0) os << ",";
        os << "{\"type\":\"" << json_util::escape(token.type) << "\",";
        os << "\"value\":\"" << json_util::escape(token.value) << "\",";
        os << "\"line\":" << token.line << ",";
        os << "\"semanticType\":\"" << json_util::escape(token.semanticType) << "\"}";
    }

    os << "]}";
    return os.str();
}
