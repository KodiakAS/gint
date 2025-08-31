#include <sstream>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerStream, Output)
{
    gint::integer<128, unsigned> v = 42;
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "42");
}

TEST(WideIntegerStream, OutputNegative)
{
    gint::integer<128, signed> v = -42;
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "-42");
}

TEST(WideIntegerStream, OutputZero)
{
    gint::integer<128, unsigned> v = 0;
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "0");
}

TEST(WideIntegerStream, ToStringChunksWithZeros)
{
    using U = gint::integer<256, unsigned>;
    const U base = U(1000000000ULL); // 1e9
    U v = U(123);
    v *= base; // * 1e9
    v *= base; // * 1e18
    v *= base; // * 1e27
    v += U(45);
    // Expect three zero chunks (9 digits each) between 123 and 045
    EXPECT_EQ(gint::to_string(v), std::string("123000000000000000000000000045"));
}
