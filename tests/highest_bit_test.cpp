#define private public
#include <gint/gint.h>
#undef private
#include <gtest/gtest.h>

TEST(InternalHelpers, HighestBitZero)
{
    using U256 = gint::integer<256, unsigned>;
    U256 z = 0;
    EXPECT_EQ(z.highest_bit(), -1);
}
