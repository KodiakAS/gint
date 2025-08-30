#include <cmath>
#include <limits>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(FloatInteropEdges, NaNComparisons)
{
    using S256 = gint::integer<256, signed>;
    using U256 = gint::integer<256, unsigned>;
    S256 s = -5;
    U256 u = 5;
    double nan = std::numeric_limits<double>::quiet_NaN();

    // All relational comparisons with NaN should be false
    EXPECT_FALSE(s < nan);
    EXPECT_FALSE(s <= nan);
    EXPECT_FALSE(s > nan);
    EXPECT_FALSE(s >= nan);
    EXPECT_FALSE(u < nan);
    EXPECT_FALSE(u <= nan);
    EXPECT_FALSE(u > nan);
    EXPECT_FALSE(u >= nan);

    EXPECT_FALSE(nan < s);
    EXPECT_FALSE(nan <= s);
    EXPECT_FALSE(nan > s);
    EXPECT_FALSE(nan >= s);
    EXPECT_FALSE(nan < u);
    EXPECT_FALSE(nan <= u);
    EXPECT_FALSE(nan > u);
    EXPECT_FALSE(nan >= u);

    // Equality with NaN is false; inequality true
    EXPECT_FALSE(s == nan);
    EXPECT_FALSE(nan == s);
    EXPECT_TRUE(s != nan);
    EXPECT_TRUE(nan != s);
}

TEST(FloatInteropEdges, InfComparisons)
{
    using S256 = gint::integer<256, signed>;
    using U256 = gint::integer<256, unsigned>;
    S256 s = -5;
    U256 u = 5;
    double pinf = std::numeric_limits<double>::infinity();
    double ninf = -std::numeric_limits<double>::infinity();

    // +inf compares greater than any finite integer
    EXPECT_TRUE(s < pinf);
    EXPECT_TRUE(u < pinf);
    EXPECT_TRUE(pinf > s);
    EXPECT_TRUE(pinf > u);
    EXPECT_TRUE(s <= pinf);
    EXPECT_TRUE(u <= pinf);
    EXPECT_TRUE(pinf >= s);
    EXPECT_TRUE(pinf >= u);
    EXPECT_FALSE(s == pinf);
    EXPECT_FALSE(u == pinf);

    // -inf compares less than any finite integer
    EXPECT_TRUE(s > ninf);
    EXPECT_TRUE(u > ninf);
    EXPECT_TRUE(ninf < s);
    EXPECT_TRUE(ninf < u);
    EXPECT_TRUE(s >= ninf);
    EXPECT_TRUE(u >= ninf);
    EXPECT_TRUE(ninf <= s);
    EXPECT_TRUE(ninf <= u);
    EXPECT_FALSE(s == ninf);
    EXPECT_FALSE(u == ninf);
}

TEST(FloatInteropEdges, SubnormalComparisons)
{
    using S256 = gint::integer<256, signed>;
    using U256 = gint::integer<256, unsigned>;
    S256 s0 = 0;
    U256 u0 = 0;
    U256 u1 = 1;
    double den = std::numeric_limits<double>::denorm_min();
    ASSERT_GT(den, 0.0);

    // zero vs tiny positive
    EXPECT_TRUE(s0 < den);
    EXPECT_TRUE(u0 < den);
    EXPECT_TRUE(den > s0);
    EXPECT_TRUE(den > u0);

    // one vs tiny positive
    EXPECT_TRUE(u1 > den);
    EXPECT_TRUE(den < u1);
}

TEST(FloatInteropEdges, EqualityPrecision)
{
    using U256 = gint::integer<256, unsigned>;
    // 2^53 + 1 is not exactly representable as double
    U256 a = U256(1);
    a <<= 53;
    U256 b = a + U256(1);
    double da = static_cast<double>(a);
    double db = static_cast<double>(b);
    EXPECT_TRUE(a == da);
    EXPECT_FALSE(b == db); // db rounds to da, should not be equal
    EXPECT_TRUE(b > db);
}
