#include "lexer/lexer.h"
#include "utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>
#include <memory>

using namespace std;

class LexerTest : public ::testing::Test {
protected:
    Lexer lexer;

    void ExpectTokensEqual(const vector<shared_ptr<Token>>& actual, const vector<shared_ptr<Token>>& expected) {
        ASSERT_EQ(actual.size(), expected.size()) << "벡터 길이가 일치하지 않습니다.!";
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(*actual[i], *expected[i])
                << "예측과 다른 결과 발생, 예측: " << TokenTypeToString(expected[i]->type) << ' ' << expected[i]->text
                << ' ' << expected[i]->line << ", 결과: " << TokenTypeToString(actual[i]->type) << ' '
                << actual[i]->text << ' ' << actual[i]->line;
        }
    }
};

TEST_F(LexerTest, OperatorTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
        make_shared<Token>(Token{TokenType::MINUS, "-", 1}),
        make_shared<Token>(Token{TokenType::ASTERISK, "*", 1}),
        make_shared<Token>(Token{TokenType::SLASH, "/", 1}),
        make_shared<Token>(Token{TokenType::PERCENT, "%", 1}),
    };
    auto actual = lexer.Tokenize({
        "+",
        "-",
        "*",
        "/",
        "%",
    });

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, IdentifierTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::IDENTIFIER, "사과", 1}),
        make_shared<Token>(Token{TokenType::IDENTIFIER, "_바나나", 1}),
        make_shared<Token>(Token{TokenType::IDENTIFIER, "호박_보석", 1}),
        make_shared<Token>(Token{TokenType::IDENTIFIER, "book", 1}),
    };
    auto actual = lexer.Tokenize(
        {"사", "과", " ", "_", "바", "나", "나", " ", "호", "박", "_", "보", "석", " ", "b", "o", "o", "k"});

    ExpectTokensEqual(actual, expected);
}


TEST_F(LexerTest, SeparatorTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::LPAREN, "(", 1}),
        make_shared<Token>(Token{TokenType::RPAREN, ")", 1}),
        make_shared<Token>(Token{TokenType::LBRACE, "{", 1}),
        make_shared<Token>(Token{TokenType::RBRACE, "}", 1}),
        make_shared<Token>(Token{TokenType::LBRACKET, "[", 1}),
        make_shared<Token>(Token{TokenType::RBRACKET, "]", 1}),
        make_shared<Token>(Token{TokenType::COLON, ":", 1}),
        make_shared<Token>(Token{TokenType::SEMICOLON, ";", 1}),
        make_shared<Token>(Token{TokenType::COMMA, ",", 1}),
        make_shared<Token>(Token{TokenType::RIGHT_ARROW, "->", 1}),
    };
    auto actual = lexer.Tokenize({
        "(",
        ")",
        "{",
        "}",
        "[",
        "]",
        ":",
        ";",
        ",",
        "-",
        ">",
    });

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, LogicalOperatorTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::EQUAL, "==", 1}),
        make_shared<Token>(Token{TokenType::ASSIGN, "=", 1}),
        make_shared<Token>(Token{TokenType::NOT_EQUAL, "!=", 1}),
        make_shared<Token>(Token{TokenType::BANG, "!", 1}),
        make_shared<Token>(Token{TokenType::LOGICAL_AND, "&&", 1}),
        make_shared<Token>(Token{TokenType::BITWISE_AND, "&", 1}),
        make_shared<Token>(Token{TokenType::LOGICAL_OR, "||", 1}),
        make_shared<Token>(Token{TokenType::BITWISE_OR, "|", 1}),
        make_shared<Token>(Token{TokenType::LESS_THAN, "<", 1}),
        make_shared<Token>(Token{TokenType::LESS_EQUAL, "<=", 1}),
        make_shared<Token>(Token{TokenType::GREATER_THAN, ">", 1}),
        make_shared<Token>(Token{TokenType::GREATER_EQUAL, ">=", 1}),
    };
    auto actual =
        lexer.Tokenize({"=", "=", "=", "!", "=", "!", "&", "&", "&", "|", "|", "|", "<", "<", "=", ">", ">", "="});

    ExpectTokensEqual(actual, expected);
}


TEST_F(LexerTest, KeywordTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::정수, "정수", 1}),
        make_shared<Token>(Token{TokenType::실수, "실수", 1}),
        make_shared<Token>(Token{TokenType::문자, "문자", 1}),
        make_shared<Token>(Token{TokenType::리턴, "리턴", 1}),
        make_shared<Token>(Token{TokenType::만약, "만약", 1}),
        make_shared<Token>(Token{TokenType::라면, "라면", 1}),
        make_shared<Token>(Token{TokenType::함수, "함수", 1}),
        make_shared<Token>(Token{TokenType::반복, "반복", 1}),
        make_shared<Token>(Token{TokenType::동안, "동안", 1}),
        make_shared<Token>(Token{TokenType::중단, "중단", 1}),
        make_shared<Token>(Token{TokenType::TRUE, "true", 1}),
        make_shared<Token>(Token{TokenType::FALSE, "false", 1}),
    };
    auto actual = lexer.Tokenize({
        "정", "수", " ",
        "실", "수", " ",
        "문", "자", " ",
        "리", "턴", " ",
        "만", "약", " ",
        "라", "면", " ",
        "함", "수", " ",
        "반", "복", " ",
        "동", "안", " ",
        "중", "단", " ",
        "t", "r", "u", "e", " ",
        "f", "a", "l", "s", "e",
    });

    ExpectTokensEqual(actual, expected);
}


TEST_F(LexerTest, LiteralTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::INTEGER, "31", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "7", 1}),
        make_shared<Token>(Token{TokenType::STRING, "apple", 1}),
    };
    auto actual = lexer.Tokenize({
        "3",
        "1",
        " ",
        "7",
        "\"",
        "a",
        "p",
        "p",
        "l",
        "e",
        "\"",
    });

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, FloatLiteralTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::FLOAT, "3.14", 1}),
        make_shared<Token>(Token{TokenType::FLOAT, "0.5", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "42", 1}),
    };
    auto actual = lexer.Tokenize({
        "3", ".", "1", "4", " ",
        "0", ".", "5", " ",
        "4", "2",
    });

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, CompoundAssignTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::PLUS_ASSIGN, "+=", 1}),
        make_shared<Token>(Token{TokenType::MINUS_ASSIGN, "-=", 1}),
        make_shared<Token>(Token{TokenType::ASTERISK_ASSIGN, "*=", 1}),
        make_shared<Token>(Token{TokenType::SLASH_ASSIGN, "/=", 1}),
        make_shared<Token>(Token{TokenType::PERCENT_ASSIGN, "%=", 1}),
    };
    auto actual = lexer.Tokenize({
        "+", "=", " ",
        "-", "=", " ",
        "*", "=", " ",
        "/", "=", " ",
        "%", "=",
    });

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, LineCommentTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::INTEGER, "42", 1}),
    };
    auto actual = lexer.Tokenize({
        "4", "2", " ", "/", "/", " ", "주", "석",
    });

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, BlockCommentTest) {
    vector<shared_ptr<Token>> expected = {
        make_shared<Token>(Token{TokenType::INTEGER, "1", 1}),
        make_shared<Token>(Token{TokenType::PLUS, "+", 1}),
        make_shared<Token>(Token{TokenType::INTEGER, "2", 1}),
    };
    auto actual = lexer.Tokenize({
        "1", " ", "/", "*", " ", "주", "석", " ", "*", "/", " ", "+", " ", "2",
    });

    ExpectTokensEqual(actual, expected);
}

// ===== 빈 줄과 들여쓰기 (회귀: 블록 안 빈 줄이 END_BLOCK/START_BLOCK을 만들던 문제) =====

namespace {
    vector<TokenType> blockTokenTypes(const string& src) {
        Lexer lexer;
        vector<TokenType> types;
        for (const auto& token : lexer.Tokenize(Utf8Converter::Convert(src))) {
            if (token->type == TokenType::START_BLOCK || token->type == TokenType::END_BLOCK
                || token->type == TokenType::NEW_LINE) {
                types.push_back(token->type);
            }
        }
        return types;
    }
} // namespace

TEST(LexerBlankLineTest, BlankLinesInsideBlockDoNotChangeIndent) {
    const vector<TokenType> expected = {
        TokenType::NEW_LINE,
        TokenType::START_BLOCK,
        TokenType::NEW_LINE,
        TokenType::NEW_LINE,
        TokenType::END_BLOCK,
        TokenType::NEW_LINE,
    };
    // 빈 줄 · 공백/탭만 있는 줄 · 열 0의 주석 줄 · CRLF 빈 줄 모두 빈 줄 없는 소스와 같은 토큰열
    EXPECT_EQ(blockTokenTypes("만약 a 라면:\n    b\n    c\nd\n"), expected);
    EXPECT_EQ(blockTokenTypes("만약 a 라면:\n\n    b\n\n  \t\n// 주석\n    c\n\nd\n"), expected);
    EXPECT_EQ(blockTokenTypes("만약 a 라면:\r\n\r\n    b\r\n\r\n    c\r\n\r\nd\r\n"), expected);
}

TEST(LexerBlankLineTest, SkippedBlankLinesStillAdvanceLineNumbers) {
    Lexer lexer;
    auto tokens = lexer.Tokenize(Utf8Converter::Convert("만약 a 라면:\n\n  \n    b\n"));
    bool found  = false;
    for (const auto& token : tokens) {
        if (token->type == TokenType::IDENTIFIER && token->text == "b") {
            found = true;
            EXPECT_EQ(token->line, 4);
            EXPECT_EQ(token->column, 4);
        }
    }
    EXPECT_TRUE(found);
}

TEST(LexerBlankLineTest, TrailingBlankLineClosesBlocksButNeverOpensOne) {
    // 입력 끝의 빈 줄은 기존대로 블록을 닫는다 (REPL이 빈 줄로 여러 줄 입력을 끝내는 동작)
    const vector<TokenType> closed = {
        TokenType::NEW_LINE,
        TokenType::START_BLOCK,
        TokenType::NEW_LINE,
        TokenType::END_BLOCK,
        TokenType::NEW_LINE,
    };
    EXPECT_EQ(blockTokenTypes("만약 a 라면:\n    b\n\n"), closed);
    // 더 깊이 들여쓴 공백 줄이 입력 끝에 있어도 새 블록을 열지 않는다
    const vector<TokenType> open = {
        TokenType::NEW_LINE,
        TokenType::START_BLOCK,
        TokenType::NEW_LINE,
        TokenType::NEW_LINE,
    };
    EXPECT_EQ(blockTokenTypes("만약 a 라면:\n    b\n            \n"), open);
}

TEST(LexerBlankLineTest, ReplStyleLineByLineEmptyLineClosesBlock) {
    Lexer lexer;
    lexer.Tokenize(Utf8Converter::Convert("만약 a 라면:\n"));
    lexer.Tokenize(Utf8Converter::Convert("    b\n"));
    auto tokens = lexer.Tokenize(Utf8Converter::Convert("\n"));
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens.front()->type, TokenType::END_BLOCK);
}
