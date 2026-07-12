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
    const auto u128_divmod = gint::divmod(u128, z128);
    EXPECT_EQ(u128_divmod.quotient, U128(0));
    EXPECT_EQ(u128_divmod.remainder, u128);

    const U512 u512 = (U512(1) << 300) + U512(123);
    const U512 z512 = 0;
    EXPECT_EQ(u512 / z512, U512(0));
    EXPECT_EQ(u512 % z512, u512);
    const auto u512_divmod = gint::divmod(u512, z512);
    EXPECT_EQ(u512_divmod.quotient, U512(0));
    EXPECT_EQ(u512_divmod.remainder, u512);
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

TEST(WideIntegerDivModUnchecked, PositiveIntegerLimbDivisor)
{
    using U256 = gint::integer<256, unsigned>;
    using S256 = gint::integer<256, signed>;

    const U256 u = (U256(1) << 130) + U256(12345);
    const U256 ud = 7;
    EXPECT_EQ((u / ud) * ud + (u % ud), u);

    const S256 s = -((S256(1) << 130) + S256(12345));
    const S256 sd = 7;
    EXPECT_EQ((s / sd) * sd + (s % sd), s);

    const S256 sp = (S256(1) << 130) + S256(12345);
    EXPECT_EQ((sp / sd) * sd + (sp % sd), sp);
}

TEST(WideIntegerDivModUnchecked, DirectLargeModuloShapes)
{
    using U64 = gint::integer<64, unsigned>;
    using U256 = gint::integer<256, unsigned>;
    using U512 = gint::integer<512, unsigned>;
    using S256 = gint::integer<256, signed>;

    EXPECT_EQ(U64(123) % U64(10), U64(3));

    const U256 zero = 0;
    EXPECT_EQ(zero % U256(7), U256(0));

    const U256 value = (U256(1) << 200) + (U256(1) << 129) + (U256(1) << 64) + U256(123);
    EXPECT_EQ(value % U256(8), U256(3));

    const U256 correction = U256(3) << 32;
    EXPECT_EQ(correction % U256(3), U256(0));

    const U256 d32 = 7;
    EXPECT_EQ((value / d32) * d32 + (value % d32), value);

    const U256 d64 = U256(0xF123456789ABCDEFULL);
    EXPECT_EQ((value / d64) * d64 + (value % d64), value);

    const U256 pow2 = U256(1) << 130;
    const U256 pow_value = pow2 + (U256(1) << 80) + U256(5);
    EXPECT_EQ(pow_value % pow2, (U256(1) << 80) + U256(5));

    const U256 two_limb = (U256(1) << 80) + U256(7);
    const U256 two_limb_value = two_limb * U256(12345) + U256(89);
    EXPECT_EQ(two_limb_value % two_limb, U256(89));

    const U256 three_limb = (U256(1) << 150) + (U256(1) << 70) + U256(11);
    const U256 three_limb_value = three_limb * U256(37) + U256(123);
    EXPECT_EQ(three_limb_value % three_limb, U256(123));

    const U512 four_limb = (U512(1) << 260) + (U512(1) << 129) + (U512(1) << 64) + U512(13);
    const U512 four_limb_value = four_limb * U512(19) + U512(17);
    EXPECT_EQ(four_limb_value % four_limb, U512(17));

    const S256 signed_lhs = -((S256(1) << 180) + S256(321));
    const S256 signed_rhs = -((S256(1) << 70) + S256(9));
    EXPECT_EQ((signed_lhs / signed_rhs) * signed_rhs + (signed_lhs % signed_rhs), signed_lhs);
}
