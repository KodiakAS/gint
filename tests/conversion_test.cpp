#include <array>
#include <limits>
#include <sstream>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerOps, Int128NegativeConversion)
{
    using gint::Int256;
    __int128 small = -5;
    Int256 a = small;
    EXPECT_EQ(a, Int256(-5));

    Int256 b;
    b = small;
    EXPECT_EQ(b, Int256(-5));

    __int128 big = -((static_cast<__int128>(1) << 100));
    Int256 c = big;
    EXPECT_EQ(c, -(Int256(1) << 100));

    Int256 d;
    d = big;
    EXPECT_EQ(d, -(Int256(1) << 100));
}

TEST(WideIntegerConversion, BuiltinToWide)
{
    int a = -42;
    gint::integer<128, signed> b = a; // implicit from int
    EXPECT_EQ(gint::to_string(b), "-42");

    unsigned long long c = 42;
    gint::integer<128, unsigned> d = c; // implicit from unsigned long long
    EXPECT_EQ(gint::to_string(d), "42");
}

TEST(WideIntegerConversion, WideToBuiltin)
{
    gint::integer<128, unsigned> a = 100;
    auto b = static_cast<unsigned int>(a); // explicit conversion to builtin
    EXPECT_EQ(b, 100u);

    gint::integer<128, signed> c = -100;
    auto d = static_cast<int>(c);
    EXPECT_EQ(d, -100);
}

TEST(WideIntegerConversion, MixedArithmetic)
{
    gint::integer<128, unsigned> a = 100;
    unsigned int b = 50;
    auto c = a + b; // builtin implicitly converted to wide integer
    EXPECT_EQ(gint::to_string(c), "150");
}

TEST(WideIntegerConversion, Conditional)
{
    const bool cond = true;
    const gint::Int256 value = 2;
    gint::Int256 result = cond ? 1 : value;
    EXPECT_EQ(result, gint::Int256(1));
}

TEST(WideIntegerConversion, ConditionalConstLvalue)
{
    const bool cond = true;
    const std::array<const gint::Int256, 1> arr = {gint::Int256(2)};
    const gint::Int256 & ref = arr[0];
    gint::Int256 result = cond ? 1 : ref;
    EXPECT_EQ(result, gint::Int256(1));
}

TEST(WideIntegerConversion, SmallIntegral)
{
    int8_t i8 = -7;
    gint::integer<128, signed> a = i8;
    EXPECT_EQ(static_cast<int8_t>(a), i8);
    uint16_t u16 = 60000;
    gint::integer<128, unsigned> b = u16;
    EXPECT_EQ(static_cast<uint16_t>(b), u16);
}

TEST(WideIntegerConversion, FloatingPoint)
{
    gint::integer<128, unsigned> v = 123;
    double d = static_cast<double>(v);
    EXPECT_NEAR(d, 123.0, 1e-9);
    float f = static_cast<float>(v);
    EXPECT_NEAR(f, 123.0f, 1e-6f);

    float f2 = 456.0f;
    gint::integer<128, unsigned> w = f2;
    EXPECT_EQ(w, 456U);
    double d2 = -789.0;
    gint::integer<128, signed> s = d2;
    EXPECT_EQ(s, -789);
}

TEST(WideIntegerConversion, FloatCtorAndAssignLongDouble)
{
    long double ld = 1234.75L;
    gint::integer<128, unsigned> a = ld; // ctor from floating
    EXPECT_EQ(gint::to_string(a), "1234");

    gint::integer<128, unsigned> b = 0;
    b = 56.9L; // assignment from floating
    EXPECT_EQ(gint::to_string(b), "56");
}

TEST(WideIntegerConversion, LongDoubleConversion256)
{
    gint::integer<256, signed> z = 0;
    EXPECT_EQ(static_cast<long double>(z), 0.0L);

    gint::integer<256, signed> n = -123;
    EXPECT_EQ(static_cast<long double>(n), -123.0L);
}

TEST(WideIntegerConversion, ToStringZero)
{
    EXPECT_EQ(gint::to_string(gint::integer<128, unsigned>(0)), "0");
    EXPECT_EQ(gint::to_string(gint::integer<128, signed>(0)), "0");
}

TEST(WideIntegerConversion, FloatingDivisionBothWays)
{
    using S256 = gint::integer<256, signed>;
    S256 a = 1000;
    double b = 3.5;
    EXPECT_EQ(S256(a / b), S256(static_cast<long double>(1000) / 3.5L));
    EXPECT_EQ(S256(b / a), S256(3.5L / static_cast<long double>(1000)));
}

TEST(WideIntegerConversion, FloatingModuloBothWays)
{
    using S256 = gint::integer<256, signed>;
    S256 a = 1000;
    double b = 3.5;
    EXPECT_EQ(S256(a % b), S256(a % S256(b)));
    EXPECT_EQ(S256(b % a), S256(3));
}

TEST(WideIntegerConversion, Int128)
{
    gint::integer<128, unsigned> a = 0;
    a = static_cast<unsigned __int128>(1) << 100;
    EXPECT_EQ(gint::to_string(a), "1267650600228229401496703205376");
}

TEST(WideIntegerConversion, UnsignedRoundtrip)
{
    gint::integer<128, unsigned> w = 42;
    auto u = static_cast<uint64_t>(w);
    EXPECT_EQ(u, 42u);
    gint::integer<128, unsigned> w2 = u;
    EXPECT_EQ(w2, w);
}

TEST(WideIntegerConversion, SignedRoundtrip)
{
    gint::integer<128, signed> w = -123;
    auto i = static_cast<int64_t>(w);
    EXPECT_EQ(i, -123);
    gint::integer<128, signed> w2 = i;
    EXPECT_EQ(w2, w);
}

TEST(WideIntegerConversion, ArithmeticWithBuiltin)
{
    gint::integer<128, unsigned> a = 100;
    uint64_t b = 20;
    auto c = a + b;
    auto d = b + a;
    auto e = a * b;
    EXPECT_EQ(gint::to_string(c), "120");
    EXPECT_EQ(gint::to_string(d), "120");
    EXPECT_EQ(gint::to_string(e), "2000");
}

TEST(WideIntegerInt128, UnsignedRoundtrip)
{
    unsigned __int128 value = (static_cast<unsigned __int128>(1) << 80) + 42;
    gint::integer<256, unsigned> w = value;
    auto back = static_cast<unsigned __int128>(w);
    EXPECT_EQ(back, value);
}

TEST(WideIntegerInt128, SignedRoundtrip)
{
    __int128 value = -((static_cast<__int128>(1) << 90) + 77);
    gint::integer<256, signed> w = value;
    auto back = static_cast<__int128>(w);
    EXPECT_EQ(back, value);
}

TEST(WideIntegerInt128, Arithmetic)
{
    gint::integer<256, unsigned> w = 100;
    unsigned __int128 b = 20;
    auto c = w + b;
    auto d = b + w;
    auto e = w * b;
    EXPECT_EQ(gint::to_string(c), "120");
    EXPECT_EQ(gint::to_string(d), "120");
    EXPECT_EQ(gint::to_string(e), "2000");
}

TEST(WideIntegerInt128, SignedToUnsignedConversion)
{
    gint::integer<256, signed> w = 123;
    auto via_explicit = static_cast<unsigned __int128>(w);
    EXPECT_TRUE(via_explicit == static_cast<unsigned __int128>(123));

    gint::integer<256, signed> negative = -1;
    auto neg_explicit = static_cast<unsigned __int128>(negative);
    EXPECT_TRUE(neg_explicit == static_cast<unsigned __int128>(-1));
}

TEST(WideIntegerInt128, SignedConversion)
{
    gint::integer<256, signed> w = 123;
    auto via_explicit = static_cast<__int128>(w);
    EXPECT_TRUE(via_explicit == static_cast<__int128>(123));

    gint::integer<256, signed> negative = -1;
    auto neg_explicit = static_cast<__int128>(negative);
    EXPECT_TRUE(neg_explicit == static_cast<__int128>(-1));
}

template <typename T>
static void test_integral_ops()
{
    using W = gint::integer<256, signed>;
    const __int128 ai = 1000;
    W a = ai;
    T b = static_cast<T>(123);
    __int128 bi = static_cast<__int128>(b);
    EXPECT_EQ(a + b, W(ai + bi));
    EXPECT_EQ(b + a, W(bi + ai));
    EXPECT_EQ(a - b, W(ai - bi));
    EXPECT_EQ(b - a, W(bi - ai));
    EXPECT_EQ(a * b, W(ai * bi));
    EXPECT_EQ(b * a, W(bi * ai));
    EXPECT_EQ(a / b, W(ai / bi));
    EXPECT_EQ(b / a, W(bi / ai));
    EXPECT_EQ(a % b, W(ai % bi));
    EXPECT_EQ(b % a, W(bi % ai));
    EXPECT_EQ(a & b, W(ai & bi));
    EXPECT_EQ(b & a, W(ai & bi));
    EXPECT_EQ(a | b, W(ai | bi));
    EXPECT_EQ(b | a, W(ai | bi));
    EXPECT_EQ(a ^ b, W(ai ^ bi));
    EXPECT_EQ(b ^ a, W(ai ^ bi));
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(b <= a);
    EXPECT_TRUE(a != b);
}

template <typename T>
static void test_float_ops()
{
    using W = gint::integer<256, signed>;
    const __int128 ai = 1000;
    W a = ai;
    T b = static_cast<T>(123.5);
    long double bd = static_cast<long double>(b);
    __int128 expected_add = static_cast<__int128>(static_cast<long double>(ai) + bd);
    __int128 expected_sub = static_cast<__int128>(static_cast<long double>(ai) - bd);
    __int128 expected_rsub = static_cast<__int128>(bd - static_cast<long double>(ai));
    __int128 expected_mul = static_cast<__int128>(static_cast<long double>(ai) * bd);
    __int128 expected_div = static_cast<__int128>(static_cast<long double>(ai) / bd);
    __int128 expected_rdiv = static_cast<__int128>(bd / static_cast<long double>(ai));
    EXPECT_EQ(W(a + b), W(expected_add));
    EXPECT_EQ(W(b + a), W(expected_add));
    EXPECT_EQ(W(a - b), W(expected_sub));
    EXPECT_EQ(W(b - a), W(expected_rsub));
    EXPECT_EQ(W(a * b), W(expected_mul));
    EXPECT_EQ(W(b * a), W(expected_mul));
    EXPECT_EQ(W(a / b), W(expected_div));
    EXPECT_EQ(W(b / a), W(expected_rdiv));
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(b <= a);
    EXPECT_TRUE(a != b);
}

TEST(WideIntegerBuiltin, IntegralTypes)
{
    test_integral_ops<int8_t>();
    test_integral_ops<uint8_t>();
    test_integral_ops<int16_t>();
    test_integral_ops<uint16_t>();
    test_integral_ops<int32_t>();
    test_integral_ops<uint32_t>();
    test_integral_ops<int64_t>();
    test_integral_ops<uint64_t>();
    test_integral_ops<__int128>();
    test_integral_ops<unsigned __int128>();
}

TEST(WideIntegerBuiltin, FloatingTypes)
{
    test_float_ops<float>();
    test_float_ops<double>();
}

TEST(WideIntegerConversion, LongDoubleZero)
{
    gint::integer<128, signed> z = 0;
    EXPECT_EQ(static_cast<long double>(z), 0.0L);
    gint::integer<128, unsigned> from_zero = 0.0f;
    EXPECT_EQ(from_zero, (gint::integer<128, unsigned>(0)));
}

TEST(WideIntegerExtra, FloatConversion)
{
    gint::integer<128, unsigned> v = 123;
    double d = static_cast<double>(v);
    EXPECT_NEAR(d, 123.0, 1e-9);
    float f = static_cast<float>(v);
    EXPECT_NEAR(f, 123.0f, 1e-6f);

    float f2 = 456.0f;
    gint::integer<128, unsigned> w = f2;
    EXPECT_EQ(w, 456U);
    double d2 = -789.0;
    gint::integer<128, signed> s = d2;
    EXPECT_EQ(s, -789);
}
