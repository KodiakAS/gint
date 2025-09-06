#define GINT_ENABLE_DIVZERO_CHECKS
#define private public
#include <gint/gint.h>
#undef private
#include <gtest/gtest.h>

TEST(WideIntegerDivision, SmallDivMod)
{
    using UInt256 = gint::integer<256, unsigned>;
    UInt256 a = (UInt256(1) << 128) + 5;
    UInt256 b = (a << 1) + a;
    EXPECT_EQ(b / 3ULL, a);
    EXPECT_EQ(b % 3ULL, UInt256(0));

    UInt256 c = UInt256(123456789);
    UInt256 d = UInt256(864197523);
    EXPECT_EQ(d / 7ULL, c);
    EXPECT_EQ(d % 7ULL, UInt256(0));
}

TEST(WideIntegerDivision, LargeLimbDivMod)
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

TEST(WideIntegerDivision, SignedSmallDivMod)
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

TEST(WideIntegerDivision, SignedInt128DivMod)
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

TEST(WideIntegerDivision, LongDivisionCorrection)
{
    using UInt256 = gint::integer<256, unsigned>;
    UInt256 dividend = (UInt256(1ULL << 63) << 192) | (UInt256(12345) << 128) | (UInt256(98764) << 64) | UInt256(42);
    UInt256 divisor = (UInt256(1ULL << 63) << 128) | (UInt256(12345) << 64) | UInt256(98765);
    UInt256 expected_q = UInt256(0xFFFFFFFFFFFFFFFFULL);
    UInt256 expected_r = (UInt256(1ULL << 63) << 128) | (UInt256(12344) << 64) | UInt256(98807);
    EXPECT_EQ(dividend / divisor, expected_q);
    EXPECT_EQ(dividend % divisor, expected_r);
    EXPECT_EQ((dividend / divisor) * divisor + (dividend % divisor), dividend);
}

// --- Division/mod algorithm coverage formerly in division_algorithms_test.cpp ---

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

TEST(WideIntegerDivision, PowerOfTwoMultiLimb)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(1) << 200);
    U256 divisor = U256(1) << 130;
    U256 q = lhs / divisor;
    EXPECT_EQ(q, U256(1) << 70);
}

TEST(WideIntegerDivision, PowerOfTwoSigned)
{
    using S256 = gint::integer<256, signed>;
    S256 lhs = S256(1) << 200;
    S256 divisor = S256(1) << 130;
    S256 q = lhs / divisor;
    EXPECT_EQ(q, S256(1) << 70);
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

TEST(WideIntegerDivision, TwoLimbFastPathMatchesGeneric)
{
    using U256 = gint::integer<256, unsigned>;
    auto make_u256 = [](uint64_t w3, uint64_t w2, uint64_t w1, uint64_t w0)
    {
        U256 x = U256(w0);
        x |= (U256(w1) << 64);
        x |= (U256(w2) << 128);
        x |= (U256(w3) << 192);
        return x;
    };

    // Two-limb divisor with high limb close to 2^63 to stress qhat estimation
    U256 divisor = (U256(1) << 127) | U256(0x4F5EAF123456789BULL);
    ASSERT_NE(divisor, U256(0));

    // A small set of diverse dividends
    U256 dividends[] = {
        make_u256(0x0123456789ABCDEFULL, 0x0FEDCBA987654321ULL, 0x0000000000000001ULL, 0xF00DFACE12345678ULL),
        make_u256(0x8000000000000000ULL, 0x7FFFFFFFFFFFFFFFULL, 0xDEADBEEFDEADBEEFULL, 0x0000000000000003ULL),
        make_u256(0x0000000000000000ULL, 0x0000000000000001ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL),
        make_u256(0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL, 0x1337133713371337ULL, 0xBADC0FFEE0DDF00DULL),
    };

    for (const auto & lhs : dividends)
    {
        // Reference via generic Knuth D path with div_limbs=2
        U256 q_generic = U256::div_large(lhs, divisor, 2);
        // Fast path result via operator/ (which dispatches to div_large_2)
        U256 q_fast = lhs / divisor;
        // Also call the fast specialization directly
        U256 q_direct = U256::div_large_2(lhs, divisor);

        EXPECT_EQ(q_fast, q_generic);
        EXPECT_EQ(q_direct, q_generic);
    }
}

// Regression: borrow-high-bit (borrow == 2^64) corner case in two-limb fast path
TEST(WideIntegerDivision, TwoLimbBorrowHighBitCritical)
{
    using U256 = gint::integer<256, unsigned>;
    auto make_u256 = [](uint64_t w3, uint64_t w2, uint64_t w1, uint64_t w0)
    {
        U256 x = U256(w0);
        x |= (U256(w1) << 64);
        x |= (U256(w2) << 128);
        x |= (U256(w3) << 192);
        return x;
    };
    // From P1: lhs / rhs where rhs is two limbs; fast path previously returned q-1
    U256 lhs = make_u256(0x8981774138e1beaeULL, 0x7e526748118bbd43ULL, 0x13b42ddfe75113d8ULL, 0x48d352f272ea0f83ULL);
    U256 rhs = make_u256(0x0000000000000000ULL, 0x0000000000000000ULL, 0xc78b9c3b4b3ccf86ULL, 0x336746e82c1b0b1bULL);

    // Reference via generic path with div_limbs=2
    U256 q_generic = U256::div_large(lhs, rhs, 2);
    // Fast path result
    U256 q_fast = lhs / rhs;
    U256 q_direct = U256::div_large_2(lhs, rhs);

    EXPECT_EQ(q_fast, q_generic);
    EXPECT_EQ(q_direct, q_generic);
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

TEST(WideIntegerDivision, SingleLimbZero)
{
    using U128 = gint::integer<128, unsigned>;
    U128 one = 1;
    U128 zero = 0;
    auto q = one / (one + one);
    (void)q;
    EXPECT_EQ(zero, U128(0));
}

TEST(WideIntegerDivision, SingleLimbBasic)
{
    using U128 = gint::integer<128, unsigned>;
    U128 a = (U128(1) << 64) + 123;
    U128 b = 7;
    U128 q = a / b;
    U128 r = a % b;
    EXPECT_EQ(q * b + r, a);
}

TEST(WideIntegerDivision, MultiLimbZero)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = 0;
    U256 b = 123;
    EXPECT_EQ(a / b, U256(0));
    EXPECT_EQ(a % b, U256(0));
}

TEST(WideIntegerDivision, SmallDivisorShiftSub512)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs = (U512(1) << 400) + 12345;
    U512 divisor = 3;
    U512 q = lhs / divisor;
    U512 r = lhs % divisor;
    EXPECT_EQ(q * divisor + r, lhs);
}

TEST(WideIntegerDivision, ZeroNumeratorLargeDivisor)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = 0;
    U256 divisor = (U256(1) << 200) + 7;
    EXPECT_EQ(lhs / divisor, U256(0));
    EXPECT_EQ(lhs % divisor, U256(0));
}

TEST(WideIntegerDivision, ZeroNumeratorLargeDivisor512)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs = 0;
    U512 divisor = (U512(1) << 400) + (U512(1) << 200) + 111;
    EXPECT_EQ(lhs / divisor, U512(0));
    EXPECT_EQ(lhs % divisor, U512(0));
}

TEST(WideIntegerDivision, LimbGreaterThanMaxSigned)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = (U256(1) << 200) + (U256(1) << 120) + 123456789ULL;
    uint64_t rhs = (1ULL << 63) + 5ULL; // > max signed 64-bit
    U256 q1 = a / rhs; // path via limb overload -> fallback to integer(rhs)
    U256 q2 = a / U256(rhs); // explicit wide division
    EXPECT_EQ(q1, q2);
    U256 r1 = a % rhs;
    U256 r2 = a % U256(rhs);
    EXPECT_EQ(r1, r2);
    EXPECT_EQ(q1 * rhs + r1, a);
}

TEST(WideIntegerDivision, U64ZeroAndNonZero)
{
    using U64 = gint::integer<64, unsigned>;
    // Zero fast-path
    U64 z = 0;
    EXPECT_EQ(z / 7ULL, U64(0));
    EXPECT_EQ(z % 7ULL, U64(0));

    // Non-zero single-limb fast path
    U64 a = (U64(1) << 63) + U64(123);
    unsigned __int128 big = (static_cast<unsigned __int128>(1) << 63) + 123;
    unsigned __int128 q = big / 7u;
    unsigned __int128 r = big % 7u;
    EXPECT_EQ(a / 7u, U64(static_cast<uint64_t>(q)));
    EXPECT_EQ(a % 7u, U64(static_cast<uint64_t>(r)));
}

TEST(WideIntegerDivision, S64DivByLimb)
{
    using S64 = gint::integer<64, signed>;
    S64 a = -((S64(1) << 62) + S64(5));
    long long d = -5;
    __int128 ai = -((static_cast<__int128>(1) << 62) + 5);
    __int128 qi = ai / d;
    __int128 ri = ai % d;
    EXPECT_EQ(a / d, S64(qi));
    EXPECT_EQ(a % d, S64(ri));
}

TEST(WideIntegerDivision, SmallOverLarge)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = 7;
    U256 divisor = (U256(1) << 200) + 3;
    EXPECT_EQ(lhs / divisor, U256(0));
    EXPECT_EQ(lhs % divisor, lhs);
}

TEST(WideIntegerDivision, QhatRhatOverflow)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(0xffffffffffffffffULL) << 192) + (U256(0xffffffffffffffffULL) << 128) + (U256(0xffffffffffffffffULL) << 64)
        + U256(0xffffffffffffffffULL);
    U256 divisor = (U256(1) << 128) + 7;
    U256 q = lhs / divisor;
    U256 r = lhs % divisor;
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

TEST(WideIntegerDivision, QhatAdjustmentBreak)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(0xffffffffffffffffULL) << 192) + (U256(0) << 128) + (U256(0xffffffffffffffffULL) << 64) + U256(0);
    U256 rhs = (U256(1) << 128) + U256(1);
    U256 q = lhs / rhs;
    U256 r = lhs % rhs;
    EXPECT_EQ(q * rhs + r, lhs);
}

TEST(WideIntegerDivision, UInt256Division)
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

TEST(WideIntegerDivision, ShiftSubtractLarge)
{
    using U320 = gint::integer<320, unsigned>;
    U320 lhs = (U320(1) << 256) + (U320(1) << 128) + U320(12345);
    U320 divisor = (U320(1) << 64) + U320(3);
    U320 q = lhs / divisor;
    U320 r = lhs % divisor;
    EXPECT_EQ(q * divisor + r, lhs);
}

TEST(WideIntegerDivision, SignedLimbDiv)
{
    using S256 = gint::integer<256, signed>;
    S256 lhs = (S256(1) << 200) + S256(12345);
    int64_t rhs = -7;
    S256 q = lhs / rhs;
    S256 r = lhs % rhs;
    EXPECT_EQ(q * rhs + r, lhs);
}

TEST(WideIntegerDivision, Div128SingleLimbPath)
{
    using U64 = gint::integer<64, unsigned>;
    U64 a = 123456789ULL;
    U64 b = 12345ULL;
    U64 q = U64::div_128(a, b);
    EXPECT_EQ(static_cast<uint64_t>(q), static_cast<uint64_t>(a) / static_cast<uint64_t>(b));
}

TEST(WideIntegerDivision, DivLargeBreak)
{
    using U192 = gint::integer<192, unsigned>;
    U192 lhs;
    lhs.data_[0] = 0;
    lhs.data_[1] = 0xffffffffffffffffULL;
    lhs.data_[2] = 1;
    U192 divisor;
    divisor.data_[0] = 0xffffffffffffffffULL;
    divisor.data_[1] = 0xffffffffffffffffULL;
    divisor.data_[2] = 0;
    U192 q = U192::div_large(lhs, divisor, 2);
    EXPECT_EQ(static_cast<uint64_t>(q), 1ULL);
}

// Exercise div_mod_small 64-bit divisor generic path for limbs != 4 (here: 192-bit)
TEST(WideIntegerDivision, DivModSmall64GenericU192)
{
    using U192 = gint::integer<192, unsigned>;
    U192 lhs = (U192(1) << 190) + (U192(1) << 128) + U192(123456789ULL);
    uint64_t div = (1ULL << 63) + 123ULL; // > 32-bit -> 64-bit reciprocal branch

    U192 q = lhs / div; // goes through integer(rhs) -> small divisor -> div_mod_small generic
    U192 r = lhs % div;
    EXPECT_EQ(q * U192(div) + r, lhs);
}
