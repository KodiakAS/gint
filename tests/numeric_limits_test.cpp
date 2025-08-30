#include <limits>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerNumericLimits, Basic)
{
    using U = gint::integer<128, unsigned>;
    using S = gint::integer<128, signed>;
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
