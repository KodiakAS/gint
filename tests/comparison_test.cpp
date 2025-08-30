#include <cstdint>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerAdditional, Comparison)
{
    gint::integer<128, unsigned> a = 5;
    gint::integer<128, unsigned> b = 10;
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a <= a);
    EXPECT_TRUE(b >= a);
}

TEST(WideIntegerAdditional, BuiltinComparison)
{
    gint::integer<128, unsigned> a = 5;
    int32_t iv = 10;
    int64_t lv = 3;
    EXPECT_TRUE(a < iv);
    EXPECT_TRUE(lv < a);
    EXPECT_TRUE(iv > a);
    EXPECT_TRUE(a >= lv);
    gint::integer<128, signed> b = -5;
    int32_t neg = -10;
    EXPECT_TRUE(b < 0);
    EXPECT_TRUE(0 > b);
    EXPECT_TRUE(neg < b);
    EXPECT_TRUE(b >= neg);
}

TEST(WideIntegerComparison, Floating)
{
    gint::integer<128, signed> a = -5;
    gint::integer<128, unsigned> b = 5;
    float f = -5.0f;
    double d = 10.0;
    EXPECT_TRUE(a == f);
    EXPECT_TRUE(f == a);
    EXPECT_TRUE(b < d);
    EXPECT_TRUE(d > b);
    EXPECT_TRUE(a <= 0.0);
    EXPECT_TRUE(0.0 >= a);
}

TEST(WideInteger256, Comparison)
{
    using U = gint::integer<256, unsigned>;
    U a = U(1) << 200;
    U b = U(1) << 100;
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a >= a);
}

TEST(WideInteger512, Comparison)
{
    using U = gint::integer<512, unsigned>;
    U a = U(1) << 500;
    U b = U(1) << 400;
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a >= a);
}
