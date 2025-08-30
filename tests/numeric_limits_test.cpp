#include <limits>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerNumericLimits, Basic)
{
    using U = gint::integer<128, unsigned>;
    using S = gint::integer<128, signed>;
    // general properties (compile-time checks avoid odr-use of static members)
    static_assert(std::numeric_limits<U>::is_specialized, "numeric_limits<U>::is_specialized");
    static_assert(std::numeric_limits<S>::is_specialized, "numeric_limits<S>::is_specialized");
    static_assert(std::numeric_limits<U>::is_integer, "numeric_limits<U>::is_integer");
    static_assert(std::numeric_limits<S>::is_integer, "numeric_limits<S>::is_integer");
    static_assert(!std::numeric_limits<U>::is_signed, "numeric_limits<U>::is_signed");
    static_assert(std::numeric_limits<S>::is_signed, "numeric_limits<S>::is_signed");
    static_assert(std::numeric_limits<U>::is_exact, "numeric_limits<U>::is_exact");
    static_assert(std::numeric_limits<S>::is_exact, "numeric_limits<S>::is_exact");
    static_assert(!std::numeric_limits<U>::has_infinity, "numeric_limits<U>::has_infinity");
    static_assert(!std::numeric_limits<S>::has_infinity, "numeric_limits<S>::has_infinity");
    static_assert(!std::numeric_limits<U>::has_quiet_NaN, "numeric_limits<U>::has_quiet_NaN");
    static_assert(!std::numeric_limits<S>::has_quiet_NaN, "numeric_limits<S>::has_quiet_NaN");
    static_assert(!std::numeric_limits<U>::has_signaling_NaN, "numeric_limits<U>::has_signaling_NaN");
    static_assert(!std::numeric_limits<S>::has_signaling_NaN, "numeric_limits<S>::has_signaling_NaN");
    static_assert(!std::numeric_limits<U>::has_denorm_loss, "numeric_limits<U>::has_denorm_loss");
    static_assert(!std::numeric_limits<S>::has_denorm_loss, "numeric_limits<S>::has_denorm_loss");
    static_assert(std::numeric_limits<U>::round_style == std::round_toward_zero, "round_style U");
    static_assert(std::numeric_limits<S>::round_style == std::round_toward_zero, "round_style S");
    static_assert(!std::numeric_limits<U>::is_iec559, "numeric_limits<U>::is_iec559");
    static_assert(!std::numeric_limits<S>::is_iec559, "numeric_limits<S>::is_iec559");
    static_assert(std::numeric_limits<U>::is_bounded, "numeric_limits<U>::is_bounded");
    static_assert(std::numeric_limits<S>::is_bounded, "numeric_limits<S>::is_bounded");
    static_assert(std::numeric_limits<U>::is_modulo, "numeric_limits<U>::is_modulo");
    static_assert(std::numeric_limits<S>::is_modulo, "numeric_limits<S>::is_modulo");

    // digit counts
    const int U_bits = static_cast<int>(sizeof(U) * 8);
    const int S_bits = static_cast<int>(sizeof(S) * 8);
    static_assert(std::numeric_limits<U>::digits == sizeof(U) * 8, "digits U");
    static_assert(std::numeric_limits<S>::digits == sizeof(S) * 8 - 1, "digits S");
    static_assert(std::numeric_limits<U>::digits10 == (sizeof(U) * 8) * 30103 / 100000, "digits10 U");
    static_assert(std::numeric_limits<S>::digits10 == ((sizeof(S) * 8 - 1) * 30103 / 100000), "digits10 S");
    static_assert(std::numeric_limits<U>::max_digits10 == 0, "max_digits10 U");
    static_assert(std::numeric_limits<S>::max_digits10 == 0, "max_digits10 S");
    static_assert(std::numeric_limits<U>::radix == 2, "radix U");
    static_assert(std::numeric_limits<S>::radix == 2, "radix S");
    static_assert(std::numeric_limits<U>::min_exponent == 0, "min_exponent U");
    static_assert(std::numeric_limits<S>::min_exponent == 0, "min_exponent S");
    static_assert(std::numeric_limits<U>::min_exponent10 == 0, "min_exponent10 U");
    static_assert(std::numeric_limits<S>::min_exponent10 == 0, "min_exponent10 S");
    static_assert(std::numeric_limits<U>::max_exponent == 0, "max_exponent U");
    static_assert(std::numeric_limits<S>::max_exponent == 0, "max_exponent S");
    static_assert(std::numeric_limits<U>::max_exponent10 == 0, "max_exponent10 U");
    static_assert(std::numeric_limits<S>::max_exponent10 == 0, "max_exponent10 S");
    static_assert(std::numeric_limits<U>::traps, "traps U");
    static_assert(std::numeric_limits<S>::traps, "traps S");
    static_assert(!std::numeric_limits<U>::tinyness_before, "tinyness_before U");
    static_assert(!std::numeric_limits<S>::tinyness_before, "tinyness_before S");
    EXPECT_EQ(std::numeric_limits<U>::min(), U(0));
    EXPECT_EQ(std::numeric_limits<U>::max(), ~U(0));
    S smin = std::numeric_limits<S>::min();
    S smax = std::numeric_limits<S>::max();
    EXPECT_EQ(smax, ~smin);
    S expected = S(1);
    expected <<= 127;
    expected = -expected;
    EXPECT_EQ(smin, expected);

    EXPECT_EQ(std::numeric_limits<U>::lowest(), std::numeric_limits<U>::min());
    EXPECT_EQ(std::numeric_limits<U>::epsilon(), U(0));
    EXPECT_EQ(std::numeric_limits<U>::round_error(), U(0));
    EXPECT_EQ(std::numeric_limits<U>::infinity(), U(0));
    EXPECT_EQ(std::numeric_limits<U>::quiet_NaN(), U(0));
    EXPECT_EQ(std::numeric_limits<U>::signaling_NaN(), U(0));
    EXPECT_EQ(std::numeric_limits<U>::denorm_min(), U(0));

    EXPECT_EQ(std::numeric_limits<S>::lowest(), std::numeric_limits<S>::min());
    EXPECT_EQ(std::numeric_limits<S>::epsilon(), S(0));
    EXPECT_EQ(std::numeric_limits<S>::round_error(), S(0));
    EXPECT_EQ(std::numeric_limits<S>::infinity(), S(0));
    EXPECT_EQ(std::numeric_limits<S>::quiet_NaN(), S(0));
    EXPECT_EQ(std::numeric_limits<S>::signaling_NaN(), S(0));
    EXPECT_EQ(std::numeric_limits<S>::denorm_min(), S(0));
}
