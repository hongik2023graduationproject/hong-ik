#include "lexer/lexer.h"
#include <gtest/gtest.h>

using namespace std;

class LexerTest : public ::testing::Test {
protected:
    Lexer lexer;

    void ExpectTokensEqual(const vector<Token*>& actual, const vector<Token*>& expected) {
        ASSERT_EQ(actual.size(), expected.size()) << "벡터 길이가 일치하지 않습니다.!";
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(*actual[i], *expected[i])
                << "예측과 다른 결과 발생, 예측: " << TokenTypeToString(expected[i]->type)
                << ", 결과: " << TokenTypeToString(actual[i]->type);
        }
    }
};

TEST_F(LexerTest, OperatorTest) {
    vector<Token*> expected = {
        new Token{TokenType::PLUS, "+", 1},
        new Token{TokenType::MINUS, "-", 1},
        new Token{TokenType::ASTERISK, "*", 1},
        new Token{TokenType::SLASH, "/", 1},
    };
    vector<Token*> actual = lexer.Tokenize({
        "+",
        "-",
        "*",
        "/",
    });

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, EscapeSequenceTest) {
    vector<Token*> expected = {
        new Token{TokenType::NEW_LINE, "\n", 1},
        new Token{TokenType::TAB, "\t", 1},
    };
    vector<Token*> actual = lexer.Tokenize({"\n", "\t"});

    ExpectTokensEqual(actual, expected);
}

TEST_F(LexerTest, IdentifierTest) {
    vector<Token*> expected = {
        new Token{TokenType::IDENTIFIER, "사과", 1},
        new Token{TokenType::IDENTIFIER, "_바나나", 1},
        new Token{TokenType::IDENTIFIER, "호박_보석", 1},
        new Token{TokenType::IDENTIFIER, "book", 1},
    };
    vector<Token*> actual = lexer.Tokenize(
        {"사", "과", " ", "_", "바", "나", "나", " ", "호", "박", "_", "보", "석", " ", "b", "o", "o", "k"});

    ExpectTokensEqual(actual, expected);
}


TEST_F(LexerTest, SeparatorTest) {
    vector<Token*> expected = {
        new Token{TokenType::LPAREN, "(", 1},
        new Token{TokenType::RPAREN, ")", 1},
        new Token{TokenType::LBRACE, "{", 1},
        new Token{TokenType::RBRACE, "}", 1},
        new Token{TokenType::LBRACKET, "[", 1},
        new Token{TokenType::RBRACKET, "]", 1},
        new Token{TokenType::COLON, ":", 1},
        new Token{TokenType::SEMICOLON, ";", 1},
        new Token{TokenType::COMMA, ",", 1},
        new Token{TokenType::RIGHT_ARROW, "->", 1},
    };
    vector<Token*> actual = lexer.Tokenize({
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
    vector<Token*> expected = {
        new Token{TokenType::EQUAL, "==", 1},
        new Token{TokenType::ASSIGN, "=", 1},
        new Token{TokenType::NOT_EQUAL, "!=", 1},
        new Token{TokenType::BANG, "!", 1},
        new Token{TokenType::LOGICAL_AND, "&&", 1},
        new Token{TokenType::BITWISE_AND, "&", 1},
        new Token{TokenType::LOGICAL_OR, "||", 1},
        new Token{TokenType::BITWISE_OR, "|", 1},
        new Token{TokenType::LESS_THAN, "<", 1},
        new Token{TokenType::LESS_EQUAL, "<=", 1},
        new Token{TokenType::GREATER_THAN, ">", 1},
        new Token{TokenType::GREATER_EQUAL, ">=", 1},
    };
    vector<Token*> actual =
        lexer.Tokenize({"=", "=", "=", "!", "=", "!", "&", "&", "&", "|", "|", "|", "<", "<", "=", ">", ">", "="});

    ExpectTokensEqual(actual, expected);
}


TEST_F(LexerTest, KeywordTest) {
    vector<Token*> expected = {
        new Token{TokenType::정수, "정수", 1},
        new Token{TokenType::실수, "실수", 1},
        new Token{TokenType::문자, "문자", 1},
        new Token{TokenType::RETURN, "return", 1},
        new Token{TokenType::만약, "만약", 1},
        new Token{TokenType::라면, "라면", 1},
        new Token{TokenType::함수, "함수", 1},
        new Token{TokenType::TRUE, "true", 1},
        new Token{TokenType::FALSE, "false", 1},
    };
    vector<Token*> actual = lexer.Tokenize({
        "정",
        "수",
        " ",
        "실",
        "수",
        " ",
        "문",
        "자",
        " ",
        "r",
        "e",
        "t",
        "u",
        "r",
        "n",
        " ",
        "만",
        "약",
        " ",
        "라",
        "면",
        " ",
        "함",
        "수",
        " ",
        "t",
        "r",
        "u",
        "e",
        " ",
        "f",
        "a",
        "l",
        "s",
        "e",
    });

    ExpectTokensEqual(actual, expected);
}

// TODO: 코드 블록 추가 시 테스트 코드 작성할 것
