#include "../lsp/position.h"
#include <gtest/gtest.h>

TEST(LspPositionTest, AsciiIsIdentity) {
    EXPECT_EQ(lsp::cpToUtf16("abc def", 4), 4);
    EXPECT_EQ(lsp::utf16ToCp("abc def", 4), 4);
}

TEST(LspPositionTest, HangulBmpIsOneUnitPerCodepoint) {
    EXPECT_EQ(lsp::cpToUtf16("정수 x = 10", 3), 3);  // '정','수',' ' → 'x' 앞
    EXPECT_EQ(lsp::utf16ToCp("정수 x = 10", 3), 3);
}

TEST(LspPositionTest, AstralCharTakesTwoUtf16Units) {
    const std::string s = "\xF0\x9F\x98\x80가";  // 😀가
    EXPECT_EQ(lsp::cpToUtf16(s, 1), 2);   // 😀 다음 = utf16으로 2
    EXPECT_EQ(lsp::utf16ToCp(s, 2), 1);
    EXPECT_EQ(lsp::utf16ToCp(s, 3), 2);
}

TEST(LspPositionTest, OutOfRangeClampsToEnd) {
    EXPECT_EQ(lsp::cpToUtf16("ab", 99), 2);
    EXPECT_EQ(lsp::utf16ToCp("ab", 99), 2);
    EXPECT_EQ(lsp::cpLength("정수 x"), 4);
}

TEST(LspPositionTest, SplitLinesHandlesLfAndCrLf) {
    auto lines = lsp::splitLines("a\r\nb\nc");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    EXPECT_EQ(lines[2], "c");
    EXPECT_TRUE(lsp::splitLines("").size() == 1u && lsp::splitLines("")[0].empty());
}
