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

TEST(WideIntegerStream, ToStringSignedMinValues)
{
    using S128 = gint::integer<128, signed>;
    using S256 = gint::integer<256, signed>;

    S128 s128_min = std::numeric_limits<S128>::min();
    EXPECT_EQ(gint::to_string(s128_min), std::string("-170141183460469231731687303715884105728"));

    S256 s256_min = std::numeric_limits<S256>::min();
    EXPECT_EQ(gint::to_string(s256_min),
              std::string("-57896044618658097711785492504343953926634992332820282019728792003956564819968"));
}
