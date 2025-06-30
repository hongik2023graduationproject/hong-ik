#include "lexer/lexer.h"
#include "utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>

using namespace std;

class Utf8ConverterTest : public ::testing::Test {
protected:
    Utf8Converter converter;

    void ExpectStringsEqual(const vector<string>& actual, const vector<string>& expected) {
        ASSERT_EQ(actual.size(), expected.size()) << "벡터 길이가 일치하지 않습니다.!";
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(actual[i], expected[i])
                << "예측과 다른 결과 발생, 예측: " << expected[i] << ", 결과: " << actual[i];
        }
    }
};


TEST_F(Utf8ConverterTest, OperatorTest) {
    vector<string> expected = {"+", "-", "*", "/"};
    vector<string> actual   = converter.Convert("+-*/");

    ExpectStringsEqual(actual, expected);
}

TEST_F(Utf8ConverterTest, Test) {
    vector<string> expected = {"_", "/", "*", "*", "<", "="};
    vector<string> actual   = converter.Convert("_/**<=");

    ExpectStringsEqual(actual, expected);
}

TEST_F(Utf8ConverterTest, KoreanTest) {
    vector<string> expected = {"가", "나", "다", "라", " ", "마", "바", "사"};
    vector<string> actual   = converter.Convert("가나다라 마바사");

    ExpectStringsEqual(actual, expected);
}

TEST_F(Utf8ConverterTest, EnglishTest) {
    vector<string> expected = {"a", "p", "p", "l", "e"};
    vector<string> actual   = converter.Convert("apple");

    ExpectStringsEqual(actual, expected);
}

TEST_F(Utf8ConverterTest, EmptyVectorTest) {
    auto actual = converter.Convert("");
    EXPECT_TRUE(actual.empty());
}

TEST_F(Utf8ConverterTest, NewLineTest) {
    vector<string> expected = {"a", "\n", "b"};
    auto actual             = converter.Convert("a\nb");

    ExpectStringsEqual(actual, expected);
}
