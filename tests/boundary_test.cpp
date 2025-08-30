#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerBoundary, Unsigned256)
{
    using U = gint::integer<256, unsigned>;
    U max = ~U(0);
    EXPECT_TRUE(max > U(0));
}

TEST(WideIntegerBoundary, Signed256)
{
    using S = gint::integer<256, signed>;
    S min = S(1) << 255;
    min = -min;
    S max = ~min;
    EXPECT_TRUE(max > min);
}

TEST(WideIntegerBoundary, Unsigned512)
{
    using U = gint::integer<512, unsigned>;
    U max = ~U(0);
    EXPECT_TRUE(max > U(0));
}

TEST(WideIntegerBoundary, Signed512)
{
    using S = gint::integer<512, signed>;
    S min = S(1) << 511;
    min = -min;
    S max = ~min;
    EXPECT_TRUE(max > min);
}
