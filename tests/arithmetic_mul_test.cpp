#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerMultiplication, SmallMul)
{
    using UInt256 = gint::integer<256, unsigned>;
    UInt256 a = (UInt256(1) << 128) + 5;
    UInt256 expected_b = (UInt256(3) << 128) + UInt256(15);
    EXPECT_EQ(a * 3ULL, expected_b);

    UInt256 c = UInt256(123456789);
    EXPECT_EQ(c * 7ULL, UInt256(864197523));
}
