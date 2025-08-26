#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <fmt/format.h>
#include <gtest/gtest.h>

#define GINT_ENABLE_DIVZERO_CHECKS
#include <gint/gint.h>

struct A
{
};
struct B
{
};

template <typename, typename>
struct Base;

template <>
struct Base<A, B>
{
    static constexpr gint::integer<256, signed> val = 1;
};

static_assert(Base<A, B>::val == 1, "template static constexpr initialization failed");

TEST(WideIntegerConstexpr, Construction)
{
    constexpr gint::integer<128, unsigned> a = 42;
    static_assert(a == 42, "constexpr constructor failed");
    constexpr gint::integer<128, signed> b = -42;
    static_assert(b == -42, "constexpr constructor failed");
    constexpr gint::integer<256, unsigned> c = 1;
    static_assert(c == 1, "constexpr construction failed");
    constexpr gint::integer<256, signed> d = -1;
    static_assert(d == -1, "constexpr construction failed");
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}

TEST(WideIntegerOps, SmallMulDiv)
{
    using UInt256 = gint::integer<256, unsigned>;
    UInt256 a = (UInt256(1) << 128) + 5;
    UInt256 b = a * 3ULL;
    EXPECT_EQ(b / 3ULL, a);
    EXPECT_EQ(b % 3ULL, UInt256(0));

    UInt256 c = UInt256(123456789);
    EXPECT_EQ(c * 7ULL, UInt256(864197523));
    EXPECT_EQ((c * 7ULL) / 7ULL, c);
}

TEST(WideIntegerOps, LargeLimbDivMod)
{
    using UInt256 = gint::integer<256, unsigned>;
    UInt256 u = UInt256(1);
    u <<= 64;
    uint64_t div = 1ULL << 63;
    EXPECT_EQ(u / div, UInt256(2));
    EXPECT_EQ(u % div, UInt256(0));

    using Int256 = gint::integer<256, signed>;
    Int256 s = Int256(1);
    s <<= 64;
    EXPECT_EQ(s / div, Int256(2));
    EXPECT_EQ(s % div, Int256(0));

    s = -s;
    EXPECT_EQ(s / div, Int256(-2));
    EXPECT_EQ(s % div, Int256(0));
}

TEST(WideIntegerOps, SignedSmallDivMod)
{
    using Int256 = gint::integer<256, signed>;
    Int256 a = 123;
    EXPECT_EQ(a / 5, Int256(24));
    EXPECT_EQ(a % 5, Int256(3));
    EXPECT_EQ(a / -5, Int256(-24));
    EXPECT_EQ(a % -5, Int256(3));
    Int256 b = -123;
    EXPECT_EQ(b / 5, Int256(-24));
    EXPECT_EQ(b % 5, Int256(-3));
    EXPECT_EQ(b / -5, Int256(24));
    EXPECT_EQ(b % -5, Int256(-3));
}

TEST(WideIntegerOps, SignedInt128DivMod)
{
    using Int256 = gint::integer<256, signed>;
    Int256 pos = 123;
    __int128 neg = -5;
    EXPECT_EQ(pos / neg, Int256(-24));
    EXPECT_EQ(pos % neg, Int256(3));
    Int256 neg_val = -123;
    EXPECT_EQ(neg_val / neg, Int256(24));
    EXPECT_EQ(neg_val % neg, Int256(-3));

    __int128 lhs = -123;
    Int256 rhs = 5;
    EXPECT_EQ(lhs / rhs, Int256(-24));
    EXPECT_EQ(lhs % rhs, Int256(-3));

    Int256 big = (Int256(1) << 200) + 12345;
    __int128 big_div = -((static_cast<__int128>(1) << 100) + 7);
    Int256 q = big / big_div;
    Int256 r = big % big_div;
    EXPECT_EQ(q * big_div + r, big);
}

enum class ArithOp
{
    Add,
    Sub,
    Mul,
    Div,
    Mod
};

struct ArithCase
{
    ArithOp op;
    unsigned a;
    unsigned b;
    const char * expected;
};

static std::string arith_case_name(const testing::TestParamInfo<ArithCase> & info)
{
    std::string op;
    switch (info.param.op)
    {
        case ArithOp::Add:
            op = "Add";
            break;
        case ArithOp::Sub:
            op = "Sub";
            break;
        case ArithOp::Mul:
            op = "Mul";
            break;
        case ArithOp::Div:
            op = "Div";
            break;
        case ArithOp::Mod:
            op = "Mod";
            break;
    }
    return fmt::format("{}_{}_{}", op, info.param.a, info.param.b);
}

class WideIntegerArithmeticTest : public ::testing::TestWithParam<ArithCase>
{
};

TEST_P(WideIntegerArithmeticTest, BasicOps)
{
    const auto param = GetParam();
    gint::integer<128, unsigned> a = param.a;
    gint::integer<128, unsigned> b = param.b;
    gint::integer<128, unsigned> c = 0;
    switch (param.op)
    {
        case ArithOp::Add:
            c = a + b;
            break;
        case ArithOp::Sub:
            c = a - b;
            break;
        case ArithOp::Mul:
            c = a * b;
            break;
        case ArithOp::Div:
            c = a / b;
            break;
        case ArithOp::Mod:
            c = a % b;
            break;
    }
    EXPECT_EQ(gint::to_string(c), param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    WideIntegerBasic,
    WideIntegerArithmeticTest,
    ::testing::Values(
        ArithCase{ArithOp::Add, 1, 2, "3"},
        ArithCase{ArithOp::Sub, 100, 40, "60"},
        ArithCase{ArithOp::Mul, 10, 20, "200"},
        ArithCase{ArithOp::Div, 200, 10, "20"},
        ArithCase{ArithOp::Mod, 200, 30, "20"}),
    arith_case_name);

TEST(WideIntegerBasic, SignedArithmetic)
{
    gint::integer<128, signed> a = -5;
    gint::integer<128, signed> b = 2;
    auto c = a + b;
    EXPECT_EQ(gint::to_string(c), "-3");
}

TEST(WideIntegerBasic, Shift)
{
    gint::integer<128, unsigned> a = 1;
    auto left = a << 100;
    EXPECT_EQ(gint::to_string(left), "1267650600228229401496703205376");
    auto b = gint::integer<128, unsigned>(1) << 127;
    auto right = b >> 64;
    EXPECT_EQ(gint::to_string(right), "9223372036854775808");
}

enum class BitOp
{
    And,
    Or,
    Xor
};

struct BitCase
{
    BitOp op;
    unsigned a;
    unsigned b;
    const char * expected;
};

static std::string bit_case_name(const testing::TestParamInfo<BitCase> & info)
{
    std::string op;
    switch (info.param.op)
    {
        case BitOp::And:
            op = "And";
            break;
        case BitOp::Or:
            op = "Or";
            break;
        case BitOp::Xor:
            op = "Xor";
            break;
    }
    return fmt::format("{}_{}_{}", op, info.param.a, info.param.b);
}

class WideIntegerBitwiseTest : public ::testing::TestWithParam<BitCase>
{
};

TEST_P(WideIntegerBitwiseTest, BasicOps)
{
    const auto param = GetParam();
    gint::integer<128, unsigned> a = param.a;
    gint::integer<128, unsigned> b = param.b;
    gint::integer<128, unsigned> c = 0;
    switch (param.op)
    {
        case BitOp::And:
            c = a & b;
            break;
        case BitOp::Or:
            c = a | b;
            break;
        case BitOp::Xor:
            c = a ^ b;
            break;
    }
    EXPECT_EQ(gint::to_string(c), param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    WideIntegerBasic,
    WideIntegerBitwiseTest,
    ::testing::Values(BitCase{BitOp::And, 10, 12, "8"}, BitCase{BitOp::Or, 10, 12, "14"}, BitCase{BitOp::Xor, 10, 12, "6"}),
    bit_case_name);

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
    unsigned int b = a; // implicit conversion to builtin
    EXPECT_EQ(b, 100u);

    gint::integer<128, signed> c = -100;
    int d = c;
    EXPECT_EQ(d, -100);
}

TEST(WideIntegerConversion, MixedArithmetic)
{
    gint::integer<128, unsigned> a = 100;
    unsigned int b = 50;
    auto c = a + b; // builtin implicitly converted to wide integer
    EXPECT_EQ(gint::to_string(c), "150");
}

TEST(WideIntegerAdditional, BitwiseNot)
{
    gint::integer<128, unsigned> a = 0;
    auto b = ~a;
    EXPECT_EQ(gint::to_string(b), "340282366920938463463374607431768211455");
}

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
    float f = 42.5f;
    gint::integer<128, unsigned> a = f;
    EXPECT_EQ(gint::to_string(a), "42");
    gint::integer<128, unsigned> b = 100;
    EXPECT_FLOAT_EQ(static_cast<float>(b), 100.0f);
    double d = -100.25;
    gint::integer<128, signed> c = d;
    EXPECT_EQ(gint::to_string(c), "-100");
    gint::integer<128, signed> e = -50;
    EXPECT_DOUBLE_EQ(static_cast<double>(e), -50.0);
}

#if defined(__SIZEOF_INT128__)
TEST(WideIntegerConversion, Int128)
{
    __int128 v = (static_cast<__int128>(1) << 100) + 123;
    gint::integer<256, unsigned> a = v;
    EXPECT_EQ(static_cast<__int128>(a), v);
    EXPECT_TRUE(a == v);
    EXPECT_TRUE(v == a);

    __int128 neg = -v;
    gint::integer<256, signed> b = neg;
    EXPECT_EQ(static_cast<__int128>(b), neg);
    EXPECT_TRUE(b == neg);
    EXPECT_TRUE(neg == b);

    unsigned __int128 u = (static_cast<unsigned __int128>(1) << 100) + 123;
    gint::integer<256, unsigned> c = u;
    EXPECT_EQ(static_cast<unsigned __int128>(c), u);
    EXPECT_TRUE(c == u);
    EXPECT_TRUE(u == c);
}

TEST(WideIntegerConversion, NegativeInt128)
{
    __int128 neg = -((static_cast<__int128>(1) << 100) + 7);
    gint::integer<128, signed> a = neg;
    EXPECT_EQ(static_cast<__int128>(a), neg);
}
TEST(WideIntegerConversion, ConstexprNegativeInt128)
{
    constexpr __int128 neg = -((static_cast<__int128>(1) << 100) + 7);
    constexpr gint::integer<128, signed> a = neg;
    static_assert(a == gint::integer<128, signed>(neg), "value mismatch");
    (void)a;
}

#endif

TEST(WideIntegerBoundary, Unsigned256)
{
    gint::integer<256, unsigned> a = gint::integer<256, unsigned>(1) << 255;
    EXPECT_EQ(gint::to_string(a), "57896044618658097711785492504343953926634992332820282019728792003956564819968");
    gint::integer<256, unsigned> max = ~gint::integer<256, unsigned>(0);
    EXPECT_EQ(gint::to_string(max), "115792089237316195423570985008687907853269984665640564039457584007913129639935");
    auto zero = max + gint::integer<256, unsigned>(1);
    EXPECT_EQ(gint::to_string(zero), "0");
}

TEST(WideIntegerBoundary, Signed256)
{
    gint::integer<256, signed> a = -(gint::integer<256, signed>(1) << 255);
    EXPECT_EQ(gint::to_string(a), "-57896044618658097711785492504343953926634992332820282019728792003956564819968");
}

TEST(WideIntegerBoundary, Unsigned512)
{
    gint::integer<512, unsigned> a = gint::integer<512, unsigned>(1) << 511;
    EXPECT_EQ(
        gint::to_string(a),
        "6703903964971298549787012499102923063739682910296196688861780721860882015036773488400937149083451713845015929093243025426876941405"
        "973284973216824503042048");
}

TEST(WideIntegerBoundary, Signed512)
{
    gint::integer<512, signed> a = -(gint::integer<512, signed>(1) << 511);
    EXPECT_EQ(
        gint::to_string(a),
        "-670390396497129854978701249910292306373968291029619668886178072186088201503677348840093714908345171384501592909324302542687694140"
        "5973284973216824503042048");
}

TEST(WideInteger256, Arithmetic)
{
    gint::integer<256, unsigned> base = gint::integer<256, unsigned>(1) << 200;
    gint::integer<256, unsigned> small = 123456789U;

    auto add = base + small;
    EXPECT_EQ(gint::to_string(add), "1606938044258990275541962092341162602522202993782792958758165");

    auto sub = base - small;
    EXPECT_EQ(gint::to_string(sub), "1606938044258990275541962092341162602522202993782792711844587");

    auto mul = small * 20U;
    EXPECT_EQ(gint::to_string(mul), "2469135780");

    auto div = mul / 10U;
    EXPECT_EQ(gint::to_string(div), "246913578");
}

TEST(WideInteger256, BitwiseAndShift)
{
    gint::integer<256, unsigned> a = gint::integer<256, unsigned>(1) << 200;
    auto left = a << 50;
    EXPECT_EQ(gint::to_string(left), "1809251394333065553493296640760748560207343510400633813116524750123642650624");

    auto right = a >> 100;
    EXPECT_EQ(gint::to_string(right), "1267650600228229401496703205376");

    auto b = a | (gint::integer<256, unsigned>(1) << 199);
    EXPECT_EQ(gint::to_string(b), "2410407066388485413312943138511743903783304490674189252952064");
}

TEST(WideInteger256, Comparison)
{
    gint::integer<256, unsigned> a = gint::integer<256, unsigned>(1) << 200;
    gint::integer<256, unsigned> b = gint::integer<256, unsigned>(1) << 199;

    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a >= a);
    EXPECT_TRUE(a != b);
}

TEST(WideInteger512, Arithmetic)
{
    gint::integer<512, unsigned> base = gint::integer<512, unsigned>(1) << 400;
    gint::integer<512, unsigned> small = 123456789U;

    auto add = base + small;
    EXPECT_EQ(
        gint::to_string(add),
        "258224987808690858965591917200301187432970579282922351283065935654064762"
        "2016841194629645353280137831435903171972870950165");

    auto sub = base - small;
    EXPECT_EQ(
        gint::to_string(sub),
        "258224987808690858965591917200301187432970579282922351283065935654064762"
        "2016841194629645353280137831435903171972624036587");

    auto mul = base * small;
    EXPECT_EQ(
        gint::to_string(mul),
        "318796278344251197415438395819030544333617004497395160257373504911314503"
        "394684917811900059524736434146499864926670201039999729664");

    auto div = base / small;
    EXPECT_EQ(
        gint::to_string(div),
        "209162242028415998220715036740750796161538414289166674570700146473163790"
        "28915462839630839202209753174739569583109");

    auto lshift = base << 10;
    EXPECT_EQ(
        gint::to_string(lshift),
        "264422387516099439580766123213108415931361873185712487713859518109762316"
        "4945245383300756841758861139390364848100093433217024");

    auto rshift = base >> 10;
    EXPECT_EQ(
        gint::to_string(rshift),
        "252172839656924666958585856640919128352510331330978858674869077787172619"
        "3375821479130513040312634601011624191379636224");
}

TEST(WideInteger512, BitwiseOps)
{
    gint::integer<512, unsigned> a = gint::integer<512, unsigned>(1) << 500;
    gint::integer<512, unsigned> b = gint::integer<512, unsigned>(1) << 400;

    auto orv = a | b;
    EXPECT_EQ(
        gint::to_string(orv),
        "327339060789614187001318969683018140209472895463272070865529437997046350"
        "2197503778396100751682444804772903525322189716362497394377321296225301275082752");

    auto andv = a & b;
    EXPECT_EQ(gint::to_string(andv), "0");

    auto xorv = a ^ b;
    EXPECT_EQ(
        gint::to_string(xorv),
        "327339060789614187001318969683018140209472895463272070865529437997046350"
        "2197503778396100751682444804772903525322189716362497394377321296225301275082752");
}

TEST(WideInteger512, Comparison)
{
    gint::integer<512, unsigned> a = gint::integer<512, unsigned>(1) << 500;
    gint::integer<512, unsigned> b = gint::integer<512, unsigned>(1) << 400;

    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a >= a);
}

TEST(WideIntegerExtra, CompoundOperators)
{
    gint::integer<128, unsigned> v = 1;
    v += 2;
    EXPECT_EQ(v, 3U);
    v *= 5;
    EXPECT_EQ(v, 15U);
    v -= 5;
    EXPECT_EQ(v, 10U);
    v /= 2;
    EXPECT_EQ(v, 5U);
    v %= 2U;
    EXPECT_EQ(v, 1U);
    v |= gint::integer<128, unsigned>(2U);
    EXPECT_EQ(v, 3U);
    v &= gint::integer<128, unsigned>(1U);
    EXPECT_EQ(v, 1U);
    v ^= gint::integer<128, unsigned>(3U);
    EXPECT_EQ(v, 2U);
    v <<= 4;
    EXPECT_EQ(v, 32U);
    v >>= 1;
    EXPECT_EQ(v, 16U);
}

TEST(WideIntegerExtra, IncDecAndBool)
{
    gint::integer<128, unsigned> v = 0;
    EXPECT_FALSE(static_cast<bool>(v));
    ++v;
    EXPECT_EQ(v, 1U);
    v++;
    EXPECT_EQ(v, 2U);
    --v;
    EXPECT_EQ(v, 1U);
    v--;
    EXPECT_EQ(v, 0U);
    EXPECT_FALSE(static_cast<bool>(v));
}

TEST(WideIntegerExtra, UnaryAndToString)
{
    gint::integer<128, signed> a = -1;
    auto b = -a;
    EXPECT_EQ(b, 1);

    auto c = +b;
    EXPECT_EQ(c, 1);
    auto d = ~gint::integer<128, unsigned>(0);
    EXPECT_EQ(d, (gint::integer<128, unsigned>(-1)));

    std::ostringstream oss;
    oss << gint::to_string(b);
    EXPECT_EQ(oss.str(), "1");

    EXPECT_EQ(fmt::format("{}", b), "1");
}

TEST(WideIntegerExtra, LargeToString)
{
    auto v = (gint::integer<128, unsigned>(1) << 127) - 1;
    EXPECT_EQ(gint::to_string(v), "170141183460469231731687303715884105727");
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

TEST(WideIntegerConversion, UnsignedRoundtrip)
{
    gint::integer<128, unsigned> w = 42;
    uint64_t u = w;
    EXPECT_EQ(u, 42u);
    gint::integer<128, unsigned> w2 = u;
    EXPECT_EQ(w2, w);
}

TEST(WideIntegerConversion, SignedRoundtrip)
{
    gint::integer<128, signed> w = -123;
    int64_t i = w;
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
    unsigned __int128 back = w;
    EXPECT_EQ(back, value);
}
TEST(WideIntegerInt128, SignedRoundtrip)
{
    __int128 value = -((static_cast<__int128>(1) << 90) + 77);
    gint::integer<256, signed> w = value;
    __int128 back = w;
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
    unsigned __int128 via_static = static_cast<unsigned __int128>(w);
    unsigned __int128 via_implicit = w;
    EXPECT_TRUE(via_static == static_cast<unsigned __int128>(123));
    EXPECT_TRUE(via_implicit == static_cast<unsigned __int128>(123));

    gint::integer<256, signed> negative = -1;
    unsigned __int128 static_neg = static_cast<unsigned __int128>(negative);
    unsigned __int128 implicit_neg = negative;
    EXPECT_TRUE(static_neg == static_cast<unsigned __int128>(-1));
    EXPECT_TRUE(implicit_neg == static_cast<unsigned __int128>(-1));
}

TEST(WideIntegerInt128, SignedConversion)
{
    gint::integer<256, signed> w = 123;
    __int128 via_static = static_cast<__int128>(w);
    __int128 via_implicit = w;
    EXPECT_TRUE(via_static == static_cast<__int128>(123));
    EXPECT_TRUE(via_implicit == static_cast<__int128>(123));

    gint::integer<256, signed> negative = -1;
    __int128 static_neg = static_cast<__int128>(negative);
    __int128 implicit_neg = negative;
    EXPECT_TRUE(static_neg == static_cast<__int128>(-1));
    EXPECT_TRUE(implicit_neg == static_cast<__int128>(-1));
}

template <typename T>
void test_integral_ops()
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
void test_float_ops()
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
}

TEST(WideIntegerBuiltin, FloatingTypes)
{
    test_float_ops<float>();
    test_float_ops<double>();
}

TEST(WideIntegerDivision, NegativeOperands)
{
    using W = gint::integer<128, signed>;
    auto check = [](long long lhs, long long rhs)
    {
        W wl = lhs;
        W wr = rhs;
        W q = wl / wr;
        __int128 expected = static_cast<__int128>(lhs) / static_cast<__int128>(rhs);
        EXPECT_EQ(q, W(expected));
    };
    check(-7, 3);
    check(7, -3);
    check(-8, 2);
    check(-8, -2);
    check(-1, 2);
}

TEST(WideInteger256, Division)
{
    using W = gint::integer<256, unsigned>;
    W a = (W{1} << 200) + 123456789ULL;
    uint64_t div = 987654321ULL;
    W q = a / div;
    W r = a % div;
    EXPECT_EQ(q * div + r, a);
    EXPECT_EQ(gint::to_string(q), "1627024769791889844363837995440879160110719541703693");
    EXPECT_EQ(static_cast<uint64_t>(r), 865650712ULL);
}

TEST(WideIntegerShift, NonPositive)
{
    gint::integer<128, unsigned> a = 42;
    auto b = a;
    b <<= 0;
    EXPECT_EQ(b, a);
    b <<= -5;
    EXPECT_EQ(b, a);
    b >>= 0;
    EXPECT_EQ(b, a);
    b >>= -7;
    EXPECT_EQ(b, a);
}

TEST(WideIntegerShift, LargeShiftAmounts)
{
    using U256 = gint::integer<256, unsigned>;
    U256 v = U256(1) << 128;

    U256 tmp = v;
    tmp <<= 256;
    EXPECT_EQ(tmp, U256(0));
    tmp = v;
    tmp <<= 320;
    EXPECT_EQ(tmp, U256(0));

    tmp = v;
    tmp >>= 256;
    EXPECT_EQ(tmp, U256(0));
    tmp = v;
    tmp >>= 320;
    EXPECT_EQ(tmp, U256(0));

    using U128 = gint::integer<128, unsigned>;
    U128 small = U128(1);
    small <<= 192;
    EXPECT_EQ(small, U128(0));
    small = U128(1);
    small >>= 192;
    EXPECT_EQ(small, U128(0));
}

TEST(WideIntegerConversion, LongDoubleZero)
{
    gint::integer<128, signed> z = 0;
    EXPECT_EQ(static_cast<long double>(z), 0.0L);
    gint::integer<128, unsigned> from_zero = 0.0f;
    EXPECT_EQ(from_zero, (gint::integer<128, unsigned>(0)));
}

TEST(WideIntegerStream, Output)
{
    gint::integer<128, unsigned> v = 42;
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "42");
}

TEST(WideIntegerMulLimbOverflow, AllOnes)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = 0;
    a += U256(0x46266a917dbcd870ULL);
    a += U256(0x90b7613918e3e357ULL) << 64;
    a += U256(0xbbc572685860e1c7ULL) << 128;
    a += U256(0xb2670508acb68230ULL) << 192;
    uint64_t rhs = 0xf2502093fcb85e1fULL;
    U256 res = a * rhs;
    EXPECT_EQ(gint::to_string(res), "38165250106338254442706927385283291263099041807018295318034436735252813010320");
}

TEST(WideIntegerDivision, PowerOfTwoMultiLimb)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(1) << 200);
    U256 divisor = U256(1) << 130;
    U256 q = lhs / divisor;
    EXPECT_EQ(q, U256(1) << 70);
}

TEST(WideIntegerDivision, UInt128Operands)
{
    using U128 = gint::integer<128, unsigned>;
    unsigned __int128 a = (static_cast<unsigned __int128>(1) << 100) + 123;
    unsigned __int128 b = (static_cast<unsigned __int128>(1) << 80) + 7;
    U128 lhs = a;
    U128 rhs = b;
    U128 q = lhs / rhs;
    unsigned __int128 expected = a / b;
    EXPECT_EQ(q, U128(expected));
}

TEST(WideIntegerDivision, SmallOperandsIn256Type)
{
    using U256 = gint::integer<256, unsigned>;
    unsigned __int128 a = (static_cast<unsigned __int128>(1) << 120) + 5;
    unsigned __int128 b = (static_cast<unsigned __int128>(1) << 90) + 3;
    U256 lhs = U256(a);
    U256 rhs = U256(b);
    U256 q = lhs / rhs;
    U256 r = lhs % rhs;
    EXPECT_EQ(q * rhs + r, lhs);
}

TEST(WideIntegerDivision, LargeDivisor256)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(1) << 200) + (U256(1) << 120) + 12345;
    U256 divisor = (U256(1) << 190) + (U256(1) << 10);
    U256 q = lhs / divisor;
    U256 r = lhs % divisor;
    EXPECT_EQ(q * divisor + r, lhs);
}

TEST(WideIntegerDivision, LargeShiftSubtract512)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs = (U512(1) << 400) + (U512(1) << 200) + 123456789;
    U512 divisor = (U512(1) << 350) + (U512(1) << 100) + 98765;
    U512 q = lhs / divisor;
    U512 r = lhs % divisor;
    EXPECT_EQ(q * divisor + r, lhs);
}

TEST(WideIntegerDivModSmall, SingleLimbZero)
{
    using U64 = gint::integer<64, unsigned>;
    U64 z = 0;
    EXPECT_EQ(z / 5, U64(0));
}

TEST(WideIntegerDivModSmall, SingleLimbBasic)
{
    using U64 = gint::integer<64, unsigned>;
    U64 v = 123456789ULL;
    EXPECT_EQ(v / 3, U64(41152263ULL));
}

TEST(WideIntegerDivModSmall, MultiLimbZero)
{
    using U256 = gint::integer<256, unsigned>;
    U256 z = 0;
    EXPECT_EQ(z / 7, U256(0));
}

TEST(WideIntegerDivision, SmallDivisorShiftSub512)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs = (U512(1) << 300) + (U512(1) << 250);
    U512 divisor = (U512(1) << 120) + 5;
    U512 q = lhs / divisor;
    U512 r = lhs % divisor;
    EXPECT_EQ(q * divisor + r, lhs);
}

TEST(WideIntegerDivision, ZeroNumeratorLargeDivisor)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs = 0;
    U512 divisor = (U512(1) << 350) + (U512(1) << 100) + 7;
    EXPECT_EQ(lhs / divisor, U512(0));
}

TEST(WideIntegerDivision, SmallOverLarge)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = U256(1) << 100;
    U256 divisor = (U256(1) << 190) + (U256(1) << 120) + (U256(1) << 60);
    EXPECT_EQ(lhs / divisor, U256(0));
}

TEST(WideIntegerDivision, QhatRhatOverflow)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(0xea03b273ac950e5eULL) << 128) + (U256(0x1a2b8f1ff1fd42a2ULL) << 64) + U256(0x51431193e6c3f339ULL);
    U256 divisor = (U256(0xc42e3d437204e52dULL) << 64) + U256(0xcd447e35b8b6d8feULL);
    U256 q = lhs / divisor;
    U256 r = lhs % divisor;
    U256 expected_q = (U256(1) << 64) + U256(0x315ebf2644616d28ULL);
    U256 expected_r = (U256(0x55ed99d81fa37e25ULL) << 64) + U256(0xdb1932e97f8fe589ULL);
    EXPECT_EQ(q, expected_q);
    EXPECT_EQ(r, expected_r);
    EXPECT_EQ(q * divisor + r, lhs);
}

TEST(WideIntegerDivision, QhatBorrowCorrection)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(0xeaea5898d5276ee7ULL) << 192) + (U256(0xb5816b74a985ab61ULL) << 128) + (U256(0x2a69acc70bf9c0efULL) << 64)
        + U256(0x105ada6b720299e3ULL);
    U256 divisor = (U256(0x88135d586a1689adULL) << 128) + (U256(0xdf26f51766faf989ULL) << 64) + U256(0x9145de05b3ab1b2cULL);
    U256 q = lhs / divisor;
    U256 r = lhs % divisor;
    U256 expected_q = (U256(1) << 64) + U256(0xb9f2aa3d006a0b15ULL);
    U256 expected_r = (U256(0x25b8b5a8f033df51ULL) << 128) + (U256(0xa12f6cbfc6b8ee40ULL) << 64) + U256(0x4b504ee61a967b47ULL);
    EXPECT_EQ(q, expected_q);
    EXPECT_EQ(r, expected_r);
    EXPECT_EQ(q * divisor + r, lhs);
}

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

TEST(WideIntegerShift, Boundary)
{
    using U256 = gint::integer<256, unsigned>;
    U256 v = 1;
    EXPECT_EQ(v << 0, v);
    EXPECT_EQ(v >> 0, v);
    EXPECT_EQ(v << 256, U256(0));
    EXPECT_EQ(v >> 256, U256(0));
    EXPECT_EQ(v << -1, v);
    EXPECT_EQ(v >> -1, v);
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
