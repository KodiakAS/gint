#include <cmath>
#include <cstdlib>
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

TEST(FloatInteropEdges, NegativeNegativeOrdering)
{
    using S256 = gint::integer<256, signed>;
    S256 m3 = -3;
    S256 m5 = -5;
    double f3 = -3.0;
    double f5 = -5.0;

    // Cross-type comparisons with both negative should reverse abs ordering
    EXPECT_FALSE(m3 < f5); // -3 < -5 is false
    EXPECT_TRUE(m3 > f5); // -3 > -5 is true
    EXPECT_TRUE(m5 < f3); // -5 < -3 is true
    EXPECT_FALSE(m5 > f3); // -5 > -3 is false

    // Symmetric operand order
    EXPECT_FALSE(f3 < m5);
    EXPECT_TRUE(f3 > m5);
    EXPECT_TRUE(f5 < m3);
    EXPECT_FALSE(f5 > m3);

    // Non-integer negative float
    double fn = -5.3;
    EXPECT_FALSE(m3 < fn); // -3 < -5.3 false
    EXPECT_TRUE(m3 > fn); // -3 > -5.3 true
    EXPECT_TRUE(fn < m3);
    EXPECT_FALSE(fn > m3);

    // Equality remains unaffected for equal magnitudes
    EXPECT_TRUE(m3 == f3);
    EXPECT_FALSE(m3 != f3);
}

TEST(FloatInteropEdges, ArithmeticWithInfAndNaN)
{
    using U256 = gint::integer<256, unsigned>;
    using S256 = gint::integer<256, signed>;
    U256 u = 10;
    S256 s = -10;
    volatile double pinf = std::numeric_limits<double>::max();
    pinf *= 2;
    volatile double ninf = -pinf;
    volatile double nan = pinf - pinf;

    // Division by ±inf yields 0
    EXPECT_EQ(u / pinf, U256(0));
    EXPECT_EQ(u / ninf, U256(0));
    EXPECT_EQ(s / pinf, S256(0));
    EXPECT_EQ(s / ninf, S256(0));

    // Modulo by ±inf yields the dividend unchanged
    EXPECT_EQ(u % pinf, u);
    EXPECT_EQ(u % ninf, u);
    EXPECT_EQ(s % pinf, s);
    EXPECT_EQ(s % ninf, s);

    // Division by NaN throws
    EXPECT_THROW((void)(u / nan), std::domain_error);
    EXPECT_THROW((void)(s / nan), std::domain_error);
    // Modulo by NaN throws
    EXPECT_THROW((void)(u % nan), std::domain_error);
    EXPECT_THROW((void)(s % nan), std::domain_error);

    // Infinite or NaN dividend (float ÷ int, float % int) throws
    U256 d = 3;
    EXPECT_THROW((void)(pinf / d), std::domain_error);
    EXPECT_THROW((void)(ninf / d), std::domain_error);
    EXPECT_THROW((void)(nan / d), std::domain_error);

    EXPECT_THROW((void)(pinf % d), std::domain_error);
    EXPECT_THROW((void)(ninf % d), std::domain_error);
    EXPECT_THROW((void)(nan % d), std::domain_error);
}

TEST(FloatInteropEdges, ZeroComparisonsRuntime)
{
    using U256 = gint::integer<256, unsigned>;
    U256 zero = 0;
    U256 one = 1;
    volatile double fzero = 1.0;
    fzero -= 1.0;
    volatile double fpos = fzero + 1.0;

    EXPECT_TRUE(zero == fzero);
    EXPECT_TRUE(zero < fpos);
    EXPECT_TRUE(one > fzero);
    EXPECT_FALSE(one < fzero);
}
