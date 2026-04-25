#include <cstdint>
#include <limits>

#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerDivModUnchecked, UnsignedIntegerZeroDivisor)
{
    using U64 = gint::integer<64, unsigned>;
    using U128 = gint::integer<128, unsigned>;
    using U512 = gint::integer<512, unsigned>;

    const U64 u64 = 123;
    const U64 z64 = 0;
    EXPECT_EQ(u64 / z64, U64(0));
    EXPECT_EQ(u64 % z64, u64);

    const U128 u128 = (U128(1) << 100) + U128(123);
    const U128 z128 = 0;
    EXPECT_EQ(u128 / z128, U128(0));
    EXPECT_EQ(u128 % z128, u128);

    const U512 u512 = (U512(1) << 300) + U512(123);
    const U512 z512 = 0;
    EXPECT_EQ(u512 / z512, U512(0));
    EXPECT_EQ(u512 % z512, u512);
}

TEST(WideIntegerDivModUnchecked, SignedIntegerZeroDivisor)
{
    using S128 = gint::integer<128, signed>;
    using S512 = gint::integer<512, signed>;

    const S128 min128 = std::numeric_limits<S128>::min();
    const S128 z128 = 0;
    EXPECT_EQ(min128 / z128, S128(0));
    EXPECT_EQ(min128 % z128, min128);

    const S512 value = -((S512(1) << 300) + S512(123));
    const S512 z512 = 0;
    EXPECT_EQ(value / z512, S512(0));
    EXPECT_EQ(value % z512, value);
}

TEST(WideIntegerDivModUnchecked, ScalarZeroDivisor)
{
    using U512 = gint::integer<512, unsigned>;
    using S512 = gint::integer<512, signed>;

    const U512 u = (U512(1) << 300) + U512(123);
    const std::uint64_t u64_zero = 0;
    const int int_zero = 0;

    EXPECT_EQ(u / u64_zero, U512(0));
    EXPECT_EQ(u % u64_zero, u);
    EXPECT_EQ(u / int_zero, U512(0));
    EXPECT_EQ(u % int_zero, u);

    const S512 s = -((S512(1) << 300) + S512(123));
    const std::int64_t i64_zero = 0;
    EXPECT_EQ(s / i64_zero, S512(0));
    EXPECT_EQ(s % i64_zero, s);
}

TEST(WideIntegerDivModUnchecked, FloatingZeroDivisor)
{
    using U512 = gint::integer<512, unsigned>;
    using S512 = gint::integer<512, signed>;

    const U512 u = (U512(1) << 300) + U512(123);
    EXPECT_EQ(u / 0.0, U512(0));
    EXPECT_EQ(u % 0.0, u);

    const S512 s = -((S512(1) << 300) + S512(123));
    EXPECT_EQ(s / 0.0, S512(0));
    EXPECT_EQ(s % 0.0, s);
}
