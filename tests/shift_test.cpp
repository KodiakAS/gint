#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerShift, Basic)
{
    gint::integer<128, unsigned> a = 1;
    auto left = a << 100;
    EXPECT_EQ(gint::to_string(left), "1267650600228229401496703205376");
    auto b = gint::integer<128, unsigned>(1) << 127;
    auto right = b >> 64;
    EXPECT_EQ(gint::to_string(right), "9223372036854775808");
}

TEST(WideIntegerShift, Boundary)
{
    using U256 = gint::integer<256, unsigned>;
    U256 v = 1;
    EXPECT_EQ(v << 0, v);
    EXPECT_EQ(v >> 0, v);
    EXPECT_EQ(v << 256, U256(0));
    EXPECT_EQ(v >> 256, U256(0));
    EXPECT_EQ(v << -1, v);
    EXPECT_EQ(v >> -1, v);
}

TEST(WideIntegerShift, NonPositive)
{
    gint::integer<128, unsigned> a = 42;
    auto b = a;
    b <<= 0;
    EXPECT_EQ(b, a);
    b <<= -5;
    EXPECT_EQ(b, a);
    b >>= 0;
    EXPECT_EQ(b, a);
    b >>= -7;
    EXPECT_EQ(b, a);
}

TEST(WideIntegerShift, LargeShiftAmounts)
{
    using U256 = gint::integer<256, unsigned>;
    U256 v = U256(1) << 128;

    U256 tmp = v;
    tmp <<= 256;
    EXPECT_EQ(tmp, U256(0));
    tmp = v;
    tmp <<= 320;
    EXPECT_EQ(tmp, U256(0));

    tmp = v;
    tmp >>= 256;
    EXPECT_EQ(tmp, U256(0));
    tmp = v;
    tmp >>= 320;
    EXPECT_EQ(tmp, U256(0));

    using U128 = gint::integer<128, unsigned>;
    U128 small = U128(1);
    small <<= 192;
    EXPECT_EQ(small, U128(0));
    small = U128(1);
    small >>= 192;
    EXPECT_EQ(small, U128(0));
}

TEST(WideIntegerShift, SignedRightShiftWrapper)
{
    using S128 = gint::integer<128, signed>;
    S128 v = 8;
    auto r = v >> 2; // wrapper delegates to >>= internally
    EXPECT_EQ(r, S128(2));
}

TEST(WideIntegerShift, SignedArithmeticRightShift)
{
    using S128 = gint::integer<128, signed>;
    // Basic sign-propagation within a limb
    EXPECT_EQ(S128(-8) >> 1, S128(-4));
    EXPECT_EQ(S128(-8) >> 2, S128(-2));
    EXPECT_EQ(S128(-1) >> 1, S128(-1));

    // Cross-limb: for 256-bit, shifting negative values must fill with ones
    using S256 = gint::integer<256, signed>;
    EXPECT_EQ(S256(-1) >> 65, S256(-1));
}

TEST(WideIntegerShift, EdgeCasesSigned512)
{
    using S512 = gint::integer<512, signed>;
    S512 x = 42;
    S512 t = x;
    t <<= 0;
    EXPECT_EQ(t, x);
    t >>= 0;
    EXPECT_EQ(t, x);
    t <<= 600; // >= total_bits
    EXPECT_EQ(t, S512(0));
    t = x;
    t >>= 600; // >= total_bits
    EXPECT_EQ(t, S512(0));
}

TEST(WideIntegerShift, EdgeCasesSigned512Negative)
{
    using S512 = gint::integer<512, signed>;
    S512 x = -42;
    S512 t = x;
    t >>= 600; // >= total_bits
    // Arithmetic shift of negative by >= width yields -1
    EXPECT_EQ(t, S512(-1));
}
