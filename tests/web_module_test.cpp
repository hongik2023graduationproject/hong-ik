#include <gtest/gtest.h>
#include <thread>

#include "../sandbox/execution_limiter.h"
#include "../error/hongik_error.h"
#include "../analyzer/analyzer.h"
#include "../io/io_interface.h"
#include "../lexer/lexer.h"
#include "../utf8_converter/utf8_converter.h"

// ===== ExecutionLimiter Tests =====

class ExecutionLimiterTest : public ::testing::Test {};

TEST_F(ExecutionLimiterTest, DefaultValues) {
    ExecutionLimiter limiter;
    EXPECT_EQ(limiter.getCurrentMemoryUsage(), 0u);
    EXPECT_EQ(limiter.getLoopIterationCount(), 0);
    EXPECT_FALSE(limiter.isTimeoutExceeded());
}

TEST_F(ExecutionLimiterTest, TimeoutNotExceededImmediately) {
    ExecutionLimiter limiter(1000); // 1초
    EXPECT_FALSE(limiter.isTimeoutExceeded());
    EXPECT_TRUE(limiter.getElapsedMs() < 100);
}

TEST_F(ExecutionLimiterTest, TimeoutExceeded) {
    ExecutionLimiter limiter(1); // 1ms 타임아웃
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(limiter.isTimeoutExceeded());
}

TEST_F(ExecutionLimiterTest, MemoryTracking) {
    ExecutionLimiter limiter(5000, 1024); // 1KB 메모리 제한
    limiter.trackMemoryAllocation(512);
    EXPECT_EQ(limiter.getCurrentMemoryUsage(), 512u);

    limiter.trackMemoryAllocation(256);
    EXPECT_EQ(limiter.getCurrentMemoryUsage(), 768u);

    limiter.freeMemory(256);
    EXPECT_EQ(limiter.getCurrentMemoryUsage(), 512u);
}

TEST_F(ExecutionLimiterTest, MemoryExceeded) {
    ExecutionLimiter limiter(5000, 1024);
    EXPECT_THROW(limiter.trackMemoryAllocation(2048), std::runtime_error);
}

TEST_F(ExecutionLimiterTest, MemoryExceededAfterPartialAllocation) {
    ExecutionLimiter limiter(5000, 1024);
    limiter.trackMemoryAllocation(800);
    EXPECT_THROW(limiter.trackMemoryAllocation(300), std::runtime_error);
}

TEST_F(ExecutionLimiterTest, FreeMemoryUnderflowProtection) {
    ExecutionLimiter limiter;
    limiter.trackMemoryAllocation(100);
    limiter.freeMemory(200); // bytes > currentMemoryBytes이면 free 무시
    EXPECT_EQ(limiter.getCurrentMemoryUsage(), 100u);

    limiter.freeMemory(100); // 정확한 크기로 해제
    EXPECT_EQ(limiter.getCurrentMemoryUsage(), 0u);
}

TEST_F(ExecutionLimiterTest, LoopCounter) {
    ExecutionLimiter limiter(5000, 33554432, 10); // 10회 루프 제한
    for (int i = 0; i < 10; i++) {
        EXPECT_NO_THROW(limiter.incrementLoopCounter());
    }
    EXPECT_EQ(limiter.getLoopIterationCount(), 10);
    EXPECT_THROW(limiter.incrementLoopCounter(), std::runtime_error);
}

TEST_F(ExecutionLimiterTest, LoopCounterReset) {
    ExecutionLimiter limiter(5000, 33554432, 5);
    for (int i = 0; i < 5; i++) {
        limiter.incrementLoopCounter();
    }
    limiter.resetLoopCounter();
    EXPECT_EQ(limiter.getLoopIterationCount(), 0);
    EXPECT_NO_THROW(limiter.incrementLoopCounter());
}

// ===== HongIkError Tests =====

class HongIkErrorTest : public ::testing::Test {};

TEST_F(HongIkErrorTest, BasicConstruction) {
    HongIkError err(HongIkErrorType::RUNTIME_ERROR, "테스트 에러");
    EXPECT_EQ(err.type, HongIkErrorType::RUNTIME_ERROR);
    EXPECT_EQ(err.message, "테스트 에러");
    EXPECT_STREQ(err.what(), "테스트 에러");
    EXPECT_EQ(err.location.line, 0);
    EXPECT_TRUE(err.code.empty());
    EXPECT_TRUE(err.suggestion.empty());
}

TEST_F(HongIkErrorTest, ConstructionWithLocation) {
    ErrorLocation loc{10, 5, 10, 15};
    HongIkError err(HongIkErrorType::SYNTAX_ERROR, "구문 에러", loc, "코드 조각", "수정 제안");
    EXPECT_EQ(err.type, HongIkErrorType::SYNTAX_ERROR);
    EXPECT_EQ(err.location.line, 10);
    EXPECT_EQ(err.location.column, 5);
    EXPECT_EQ(err.location.endLine, 10);
    EXPECT_EQ(err.location.endColumn, 15);
    EXPECT_EQ(err.code, "코드 조각");
    EXPECT_EQ(err.suggestion, "수정 제안");
}

TEST_F(HongIkErrorTest, RuntimeErrorHelper) {
    auto err = HongIkError::RuntimeError("런타임 문제", 42);
    EXPECT_EQ(err.type, HongIkErrorType::RUNTIME_ERROR);
    EXPECT_EQ(err.message, "런타임 문제");
    EXPECT_EQ(err.location.line, 42);
}

TEST_F(HongIkErrorTest, TypeErrorHelper) {
    auto err = HongIkError::TypeError("타입 문제", 7);
    EXPECT_EQ(err.type, HongIkErrorType::TYPE_ERROR);
    EXPECT_EQ(err.message, "타입 문제");
    EXPECT_EQ(err.location.line, 7);
}

TEST_F(HongIkErrorTest, SyntaxErrorHelper) {
    auto err = HongIkError::SyntaxError("구문 문제", 3);
    EXPECT_EQ(err.type, HongIkErrorType::SYNTAX_ERROR);
    EXPECT_EQ(err.message, "구문 문제");
    EXPECT_EQ(err.location.line, 3);
}

TEST_F(HongIkErrorTest, ToJsonString) {
    auto err = HongIkError::RuntimeError("에러 메시지", 5);
    std::string json = err.toJsonString();

    EXPECT_NE(json.find("\"type\":\"RuntimeError\""), std::string::npos);
    EXPECT_NE(json.find("\"typeCode\":1"), std::string::npos);
    EXPECT_NE(json.find("\"line\":5"), std::string::npos);
}

TEST_F(HongIkErrorTest, ToJsonStringWithCodeAndSuggestion) {
    ErrorLocation loc{1, 0, 1, 10};
    HongIkError err(HongIkErrorType::SYNTAX_ERROR, "에러", loc, "코드", "제안");
    std::string json = err.toJsonString();

    EXPECT_NE(json.find("\"type\":\"SyntaxError\""), std::string::npos);
    EXPECT_NE(json.find("\"code\""), std::string::npos);
    EXPECT_NE(json.find("\"suggestion\""), std::string::npos);
}

TEST_F(HongIkErrorTest, ToJsonStringWithoutOptionalFields) {
    auto err = HongIkError::RuntimeError("에러");
    std::string json = err.toJsonString();

    EXPECT_EQ(json.find("\"code\""), std::string::npos);
    EXPECT_EQ(json.find("\"suggestion\""), std::string::npos);
}

TEST_F(HongIkErrorTest, ToJsonEscapesSpecialChars) {
    auto err = HongIkError::RuntimeError("에러\n\"따옴표\"포함");
    std::string json = err.toJsonString();

    // JSON에서 이스케이프 처리 확인 (\\n, \\\")
    EXPECT_NE(json.find("\\n"), std::string::npos);
    EXPECT_NE(json.find("\\\""), std::string::npos);
}

TEST_F(HongIkErrorTest, IsStdException) {
    auto err = HongIkError::RuntimeError("테스트");
    // HongIkError는 std::exception을 상속
    const std::exception& ex = err;
    EXPECT_STREQ(ex.what(), "테스트");
}

// ===== Analyzer Tests =====

class AnalyzerTest : public ::testing::Test {};

TEST_F(AnalyzerTest, GetTokensSimpleExpression) {
    auto result = Analyzer::GetTokens("1 + 2\n");
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.tokens.size(), 3u); // 1, +, 2
}

TEST_F(AnalyzerTest, GetTokensKeyword) {
    auto result = Analyzer::GetTokens("만약 true 라면:\n");
    EXPECT_TRUE(result.success);

    bool foundKeyword = false;
    for (const auto& t : result.tokens) {
        if (t.value == "만약" && t.semanticType == "keyword") {
            foundKeyword = true;
            break;
        }
    }
    EXPECT_TRUE(foundKeyword);
}

TEST_F(AnalyzerTest, SemanticTypeNumber) {
    auto result = Analyzer::GetTokens("42\n");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.tokens.size(), 1u);
    EXPECT_EQ(result.tokens[0].semanticType, "number");
    EXPECT_EQ(result.tokens[0].value, "42");
}

TEST_F(AnalyzerTest, SemanticTypeFloat) {
    auto result = Analyzer::GetTokens("3.14\n");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.tokens.size(), 1u);
    EXPECT_EQ(result.tokens[0].semanticType, "number");
}

TEST_F(AnalyzerTest, SemanticTypeString) {
    auto result = Analyzer::GetTokens("\"hello\"\n");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.tokens.size(), 1u);
    EXPECT_EQ(result.tokens[0].semanticType, "string");
}

TEST_F(AnalyzerTest, SemanticTypeBoolean) {
    auto result = Analyzer::GetTokens("true\n");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.tokens.size(), 1u);
    EXPECT_EQ(result.tokens[0].semanticType, "boolean");
}

TEST_F(AnalyzerTest, SemanticTypeBuiltin) {
    auto result = Analyzer::GetTokens("출력\n");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.tokens.size(), 1u);
    EXPECT_EQ(result.tokens[0].semanticType, "builtin");
}

TEST_F(AnalyzerTest, SemanticTypeVariable) {
    auto result = Analyzer::GetTokens("변수명\n");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.tokens.size(), 1u);
    EXPECT_EQ(result.tokens[0].semanticType, "variable");
}

TEST_F(AnalyzerTest, SemanticTypeOperator) {
    auto result = Analyzer::GetTokens("1 + 2\n");
    EXPECT_TRUE(result.success);

    bool foundOperator = false;
    for (const auto& t : result.tokens) {
        if (t.value == "+" && t.semanticType == "operator") {
            foundOperator = true;
            break;
        }
    }
    EXPECT_TRUE(foundOperator);
}

TEST_F(AnalyzerTest, SemanticTypePunctuation) {
    auto result = Analyzer::GetTokens("()\n");
    EXPECT_TRUE(result.success);

    bool foundPunct = false;
    for (const auto& t : result.tokens) {
        if (t.value == "(" && t.semanticType == "punctuation") {
            foundPunct = true;
            break;
        }
    }
    EXPECT_TRUE(foundPunct);
}

TEST_F(AnalyzerTest, FiltersOutNewlineAndBlockTokens) {
    auto result = Analyzer::GetTokens("1\n2\n");
    EXPECT_TRUE(result.success);

    for (const auto& t : result.tokens) {
        EXPECT_NE(t.type, "NEW_LINE");
        EXPECT_NE(t.type, "START_BLOCK");
        EXPECT_NE(t.type, "END_BLOCK");
    }
}

TEST_F(AnalyzerTest, TokensToJson) {
    auto result = Analyzer::GetTokens("42\n");
    EXPECT_TRUE(result.success);

    std::string json = Analyzer::TokensToJson(result.tokens);
    EXPECT_NE(json.find("\"tokens\":["), std::string::npos);
    EXPECT_NE(json.find("\"value\":\"42\""), std::string::npos);
    EXPECT_NE(json.find("\"semanticType\":\"number\""), std::string::npos);
}

TEST_F(AnalyzerTest, EmptyInput) {
    auto result = Analyzer::GetTokens("\n");
    EXPECT_TRUE(result.success);
    // 빈 입력 (개행만) 이면 유의미한 토큰 없음
}

TEST_F(AnalyzerTest, MultipleTokenTypes) {
    auto result = Analyzer::GetTokens("정수 x = 10\n");
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.tokens.size(), 4u); // 정수, x, =, 10

    // 각 타입 확인
    EXPECT_EQ(result.tokens[0].semanticType, "keyword");   // 정수
    EXPECT_EQ(result.tokens[1].semanticType, "variable");   // x
    EXPECT_EQ(result.tokens[2].semanticType, "operator");   // =
    EXPECT_EQ(result.tokens[3].semanticType, "number");     // 10
}

// ===== IOContext Tests =====

class IOContextTest : public ::testing::Test {};

TEST_F(IOContextTest, CreateConsoleIO) {
    auto ctx = IOContext::CreateConsoleIO();
    EXPECT_TRUE(ctx.print != nullptr);
    EXPECT_TRUE(ctx.input != nullptr);
    EXPECT_TRUE(ctx.fileRead != nullptr);
    EXPECT_TRUE(ctx.fileWrite != nullptr);
}

TEST_F(IOContextTest, CreateWebIO) {
    auto ctx = IOContext::CreateWebIO();
    EXPECT_TRUE(ctx.print != nullptr);
    EXPECT_TRUE(ctx.input != nullptr);
    EXPECT_TRUE(ctx.fileRead != nullptr);
    EXPECT_TRUE(ctx.fileWrite != nullptr);
}

TEST_F(IOContextTest, WebIOInputReturnsEmpty) {
    auto ctx = IOContext::CreateWebIO();
    EXPECT_EQ(ctx.input("프롬프트"), "");
}

TEST_F(IOContextTest, WebIOFileReadThrows) {
    auto ctx = IOContext::CreateWebIO();
    EXPECT_THROW(ctx.fileRead("test.txt"), std::runtime_error);
}

TEST_F(IOContextTest, WebIOFileWriteThrows) {
    auto ctx = IOContext::CreateWebIO();
    EXPECT_THROW(ctx.fileWrite("test.txt", "내용"), std::runtime_error);
}

TEST_F(IOContextTest, CustomCallbacks) {
    IOContext ctx;
    std::string captured;

    ctx.print = [&captured](const std::string& text) {
        captured += text;
    };

    ctx.print("hello");
    ctx.print(" world");
    EXPECT_EQ(captured, "hello world");
}

TEST_F(IOContextTest, CustomInputCallback) {
    IOContext ctx;
    ctx.input = [](const std::string& prompt) -> std::string {
        return "사용자 입력";
    };

    EXPECT_EQ(ctx.input("프롬프트"), "사용자 입력");
}

TEST_F(IOContextTest, DefaultCallbacksAreNull) {
    IOContext ctx;
    EXPECT_TRUE(ctx.print == nullptr);
    EXPECT_TRUE(ctx.input == nullptr);
    EXPECT_TRUE(ctx.fileRead == nullptr);
    EXPECT_TRUE(ctx.fileWrite == nullptr);
}

// ===== Lexer Column Tests =====

class LexerColumnTest : public ::testing::Test {
protected:
    Lexer lexer;
};

TEST_F(LexerColumnTest, SingleCharOperatorColumn) {
    auto utf8 = Utf8Converter::Convert("1 + 2\n");
    auto tokens = lexer.Tokenize(utf8);
    // tokens: 1(col 0-1), +(col 2-3), 2(col 4-5), \n
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0]->column, 0);    // "1"
    EXPECT_EQ(tokens[1]->column, 2);    // "+"
    EXPECT_EQ(tokens[2]->column, 4);    // "2"
}

TEST_F(LexerColumnTest, MultiCharOperatorColumn) {
    auto utf8 = Utf8Converter::Convert("1 == 2\n");
    auto tokens = lexer.Tokenize(utf8);
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0]->column, 0);      // "1"
    EXPECT_EQ(tokens[1]->column, 2);      // "=="
    EXPECT_EQ(tokens[1]->endColumn, 4);   // "==" 끝
    EXPECT_EQ(tokens[2]->column, 5);      // "2"
}

TEST_F(LexerColumnTest, StringLiteralColumn) {
    auto utf8 = Utf8Converter::Convert("\"hello\"\n");
    auto tokens = lexer.Tokenize(utf8);
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0]->column, 0);      // 문자열 시작 (")
    EXPECT_EQ(tokens[0]->endColumn, 7);   // 문자열 끝 (closing " 뒤)
}

TEST_F(LexerColumnTest, IdentifierColumn) {
    auto utf8 = Utf8Converter::Convert("abc def\n");
    auto tokens = lexer.Tokenize(utf8);
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0]->column, 0);      // "abc"
    EXPECT_EQ(tokens[0]->endColumn, 3);
    EXPECT_EQ(tokens[1]->column, 4);      // "def"
    EXPECT_EQ(tokens[1]->endColumn, 7);
}

TEST_F(LexerColumnTest, NumberColumn) {
    auto utf8 = Utf8Converter::Convert("123 45\n");
    auto tokens = lexer.Tokenize(utf8);
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0]->column, 0);
    EXPECT_EQ(tokens[0]->endColumn, 3);
    EXPECT_EQ(tokens[1]->column, 4);
    EXPECT_EQ(tokens[1]->endColumn, 6);
}
