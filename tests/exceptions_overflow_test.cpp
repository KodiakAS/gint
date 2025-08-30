#include <limits>
#include <stdexcept>
#define GINT_ENABLE_DIVZERO_CHECKS
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerDivMod, ZeroDivisor)
{
    using U256 = gint::integer<256, unsigned>;
    U256 value = (U256(1) << 100) + 123;
    U256 zero = 0;
    EXPECT_THROW(value / zero, std::domain_error);
    EXPECT_THROW(value % zero, std::domain_error);

    EXPECT_THROW(value / 0, std::domain_error);
    EXPECT_THROW(value % 0, std::domain_error);
    EXPECT_THROW(value / 0.0, std::domain_error);
    EXPECT_THROW(value % 0.0, std::domain_error);

    using S256 = gint::integer<256, signed>;
    S256 sval = S256(-456);
    S256 szero = 0;
    EXPECT_THROW(sval / szero, std::domain_error);
    EXPECT_THROW(sval % szero, std::domain_error);
}

TEST(WideIntegerExceptions, ConstructFromNegative)
{
    gint::integer<128, unsigned> u = -1;
    EXPECT_EQ(gint::to_string(u), "340282366920938463463374607431768211455");
}

TEST(WideIntegerExceptions, UnsignedSubtractionUnderflow)
{
    gint::integer<128, unsigned> a = 5;
    gint::integer<128, unsigned> b = 10;
    auto c = a - b;
    EXPECT_EQ(gint::to_string(c), "340282366920938463463374607431768211451");
}

TEST(WideIntegerExceptions, BitwiseOnNegative)
{
    gint::integer<128, signed> a = -5;
    gint::integer<128, signed> b = 3;
    auto c = a & b;
    EXPECT_EQ(gint::to_string(c), "3");
}

TEST(WideIntegerOverflow, UnsignedAdditionWrap)
{
    using U128 = gint::integer<128, unsigned>;
    U128 max = U128(-1);
    U128 result = max + U128(1);
    EXPECT_EQ(result, U128(0));
}

TEST(WideIntegerOverflow, SignedMultiplicationWrap)
{
    using S128 = gint::integer<128, signed>;
    S128 max = (S128(1) << 127) - S128(1);
    S128 result = max * 2;
    EXPECT_EQ(result, S128(-2));
}

TEST(WideIntegerConversionOverflow, ToUint64)
{
    using U256 = gint::integer<256, unsigned>;
    U256 val = (U256(1) << 200) + 123456789ULL;
    uint64_t truncated = static_cast<uint64_t>(val);
    EXPECT_EQ(truncated, 123456789ULL);
}

TEST(WideIntegerConversionOverflow, ToInt64)
{
    using S256 = gint::integer<256, signed>;
    S256 val = (S256(1) << 200) + (S256(1) << 63);
    int64_t truncated = static_cast<int64_t>(val);
    EXPECT_EQ(truncated, std::numeric_limits<int64_t>::min());
}

TEST(WideIntegerConversionOverflow, FromLargeNative)
{
    using U64 = gint::integer<64, unsigned>;
    unsigned __int128 big = (static_cast<unsigned __int128>(1) << 100) + 7;
    U64 v = big;
    EXPECT_EQ(v, U64(7));
}
