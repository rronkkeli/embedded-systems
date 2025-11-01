#include <gtest/gtest.h>
#include "../TimeParser.h"

TEST(TimeParserTest, TestCaseCorrectTimes) {
    char test_str1[] = "102030";
    char test_str2[] = "000500";
    char test_str3[] = "010030";

    ASSERT_EQ(time_parse(test_str1), 37230);
    ASSERT_EQ(time_parse(test_str2), 300);
    ASSERT_EQ(time_parse(test_str3), 3630);
}

TEST(TimeParserTest, TestCaseInvalidSyntaxes) {
    char test_str1[] = "--2001";
    char test_str2[] = "0010HH";
    char test_str3[] = "23-500";

    ASSERT_EQ(time_parse(test_str1), ERR_TIME(FLAG_TIME_VALUE_HOUR | FLAG_TIME_SYNTAX));
    ASSERT_EQ(time_parse(test_str2), ERR_TIME(FLAG_TIME_VALUE_SECOND | FLAG_TIME_SYNTAX));
    ASSERT_EQ(time_parse(test_str3), ERR_TIME(FLAG_TIME_VALUE_MINUTE | FLAG_TIME_SYNTAX));
}

TEST(TimeParserTest, TestCaseInvalidLengths) {
    char test_str1[] = "2001";
    char test_str2[] = "2020201";
    char test_str3[] = "22110020";

    ASSERT_EQ(time_parse(test_str1), ERR_TIME(FLAG_TIME_LEN));
    ASSERT_EQ(time_parse(test_str2), ERR_TIME(FLAG_TIME_LEN));
    ASSERT_EQ(time_parse(test_str3), ERR_TIME(FLAG_TIME_LEN));
}

TEST(TimeParserTest, TestCaseInvalidHours) {
    char test_str1[] = "240000";
    char test_str2[] = "602020";
    char test_str3[] = "995959";

    ASSERT_EQ(time_parse(test_str1), ERR_TIME(FLAG_TIME_VALUE_HOUR));
    ASSERT_EQ(time_parse(test_str2), ERR_TIME(FLAG_TIME_VALUE_HOUR));
    ASSERT_EQ(time_parse(test_str3), ERR_TIME(FLAG_TIME_VALUE_HOUR));
}

TEST(TimeParserTest, TestCaseInvalidMinutes) {
    char test_str1[] = "006000";
    char test_str2[] = "008955";
    char test_str3[] = "239959";

    ASSERT_EQ(time_parse(test_str1), ERR_TIME(FLAG_TIME_VALUE_MINUTE));
    ASSERT_EQ(time_parse(test_str2), ERR_TIME(FLAG_TIME_VALUE_MINUTE));
    ASSERT_EQ(time_parse(test_str3), ERR_TIME(FLAG_TIME_VALUE_MINUTE));
}

TEST(TimeParserTest, TestCaseInvalidSeconds) {
    char test_str1[] = "120060";
    char test_str2[] = "001070";
    char test_str3[] = "235999";

    ASSERT_EQ(time_parse(test_str1), ERR_TIME(FLAG_TIME_VALUE_SECOND));
    ASSERT_EQ(time_parse(test_str2), ERR_TIME(FLAG_TIME_VALUE_SECOND));
    ASSERT_EQ(time_parse(test_str3), ERR_TIME(FLAG_TIME_VALUE_SECOND));
}

TEST(TimeParserTest, TestCaseMultiErrorFlags) {
    char test_str1[] = "25-905";
    char test_str2[] = "0010700";
    char test_str3[] = "-999999";
    char test_str4[] = "60";
    char test_str5[] = "xx60pp";
 
    ASSERT_EQ(time_parse(test_str1), ERR_TIME(FLAG_TIME_SYNTAX | FLAG_TIME_VALUE_HOUR | FLAG_TIME_VALUE_MINUTE));
    ASSERT_EQ(time_parse(test_str2), ERR_TIME(FLAG_TIME_LEN | FLAG_TIME_VALUE_SECOND));
    ASSERT_EQ(time_parse(test_str3), ERR_TIME(FLAG_TIME_LEN | FLAG_TIME_SYNTAX | FLAG_TIME_VALUE_HOUR | FLAG_TIME_VALUE_MINUTE | FLAG_TIME_VALUE_SECOND));
    ASSERT_EQ(time_parse(test_str4), ERR_TIME(FLAG_TIME_LEN | FLAG_TIME_VALUE_HOUR));
    ASSERT_EQ(time_parse(test_str5), ERR_TIME(FLAG_TIME_SYNTAX | FLAG_TIME_VALUE_MINUTE | FLAG_TIME_VALUE_HOUR | FLAG_TIME_VALUE_SECOND));
}

// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html
