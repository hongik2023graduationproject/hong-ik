#include "lexer/lexer.h"
#include "utf8_converter/utf8_converter.h"
#include <gtest/gtest.h>

class Utf8ConverterTest : public ::testing::Test {
protected:
    Utf8Converter converter;

    void ExpectStringsEqual(const std::vector<std::string>& actual, const std::vector<std::string>& expected) {
        ASSERT_EQ(actual.size(), expected.size()) << "벡터 길이가 일치하지 않습니다.!";
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(actual[i], expected[i]) << "예측과 다른 결과 발생, 예측: " << expected[i] << ", 결과: " << actual[i];
        }
    }
};


TEST_F(Utf8ConverterTest, OperatorTest) {
    std::vector<std::string> expected = {"+", "-", "*", "/"};
    std::vector<std::string> actual   = converter.Convert("+-*/");

    ExpectStringsEqual(actual, expected);
}

TEST_F(Utf8ConverterTest, KoreanTest) {
    std::vector<std::string> expected = {"가", "나", "다", "라"};
    std::vector<std::string> actual   = converter.Convert("가나다라");

    ExpectStringsEqual(actual, expected);
}

TEST_F(Utf8ConverterTest, EmptyVectorTest) {
    auto actual = converter.Convert("");
    EXPECT_TRUE(actual.empty());
}