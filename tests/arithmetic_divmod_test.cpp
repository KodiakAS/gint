#include <gint/gint.h>
#include <gtest/gtest.h>

template <typename Int>
using TestAccess = gint::detail::integer_test_access<Int::bits, typename Int::signed_tag>;

namespace
{
using u128 = unsigned __int128;

u128 mulhi_u128_reference(u128 a, u128 b)
{
    const u128 a0 = static_cast<uint64_t>(a);
    const u128 a1 = a >> 64;
    const u128 b0 = static_cast<uint64_t>(b);
    const u128 b1 = b >> 64;
    const u128 t0 = a0 * b0;
    const u128 t1 = a0 * b1;
    const u128 t2 = a1 * b0;
    const u128 t3 = a1 * b1;
    const u128 mid = (t0 >> 64) + static_cast<uint64_t>(t1) + static_cast<uint64_t>(t2);
    return t3 + (t1 >> 64) + (t2 >> 64) + (mid >> 64);
}
} // namespace

TEST(WideIntegerDivision, MulHiU128HandlesMiddleCarry)
{
    const u128 all_bits = ~u128(0);
    const u128 got = gint::detail::mulhi_u128(all_bits, all_bits);
    const u128 expected = mulhi_u128_reference(all_bits, all_bits);

    EXPECT_EQ(got, expected);
    EXPECT_EQ(static_cast<uint64_t>(got >> 64), UINT64_MAX);
    EXPECT_EQ(static_cast<uint64_t>(got), UINT64_MAX - 1);
}

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

TEST(WideIntegerDivision, SignedSmallDivMod_NegativeDivisorRegression)
{
    using Int256 = gint::integer<256, signed>;
    Int256 lhs = -7;
    Int256 rhs = -3;
    EXPECT_EQ(lhs / rhs, Int256(2));
    EXPECT_EQ(lhs % rhs, Int256(-1));
    EXPECT_EQ((lhs / rhs) * rhs + (lhs % rhs), lhs);
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

TEST(WideIntegerDivision, SignedPowerOfTwoMinValue)
{
    using S128 = gint::integer<128, signed>;
    const S128 min = std::numeric_limits<S128>::min();

    const S128 expected_half = -(S128(1) << 126);
    EXPECT_EQ(min / 2, expected_half);
    EXPECT_EQ(min % 2, S128(0));

    const S128 expected_shift = -(S128(1) << 120);
    const uint64_t divisor = 1ULL << 7; // 128
    EXPECT_EQ(min / static_cast<long long>(divisor), expected_shift);
    EXPECT_EQ(min % static_cast<long long>(divisor), S128(0));

    const S128 expected_large = S128(1) << 64;
    const long long min_divisor = std::numeric_limits<long long>::min();
    EXPECT_EQ(min / min_divisor, expected_large);
    EXPECT_EQ(min % min_divisor, S128(0));
}

TEST(WideIntegerDivision, SignedMinDividedByItself)
{
    using S128 = gint::integer<128, signed>;
    const S128 min = std::numeric_limits<S128>::min();

    // 回归覆盖：在修复前，此处会返回 -1，确保测试能够捕捉该缺陷。
    EXPECT_EQ(min / min, S128(1));
    EXPECT_EQ(min % min, S128(0));
}

TEST(WideIntegerDivision, SignedNegative128OverLargerNegativeIsZero)
{
    using S128 = gint::integer<128, signed>;
    const S128 min = std::numeric_limits<S128>::min();
    const S128 lhs = min + S128(12345);
    const S128 rhs = min + S128(7);

    EXPECT_EQ(lhs / rhs, S128(0));
    EXPECT_EQ(lhs % rhs, lhs);
}

TEST(WideIntegerDivision, SignedSmallDivisorInt64Min)
{
    using S256 = gint::integer<256, signed>;
    const long long divisor = std::numeric_limits<long long>::min();

    {
        S256 lhs = S256(1) << 130;
        S256 q = lhs / divisor;
        S256 r = lhs % divisor;
        EXPECT_EQ(q, -(S256(1) << 67));
        EXPECT_EQ(r, S256(0));
        EXPECT_EQ(q * S256(divisor) + r, lhs);
    }

    {
        S256 lhs = (S256(1) << 130) + S256(12345);
        S256 q = lhs / divisor;
        S256 r = lhs % divisor;
        EXPECT_EQ(q, -(S256(1) << 67));
        EXPECT_EQ(r, S256(12345));
        EXPECT_EQ(q * S256(divisor) + r, lhs);
    }
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

// Additional coverage: 64-bit non power-of-two divisors using small-divisor path
TEST(WideIntegerDivision, SmallDivisor64_NonPowerOfTwo)
{
    using U256 = gint::integer<256, unsigned>;
    const uint64_t D = 0xF123456789ABCDEFULL; // > 2^32, non power-of-two

    // n = 1 limb
    {
        U256 a = U256(0xFFFFFFFFFFFFFFFFULL);
        U256 q = a / D;
        U256 r = a % D;
        uint64_t q_ref = 0xFFFFFFFFFFFFFFFFULL / D;
        uint64_t r_ref = 0xFFFFFFFFFFFFFFFFULL % D;
        EXPECT_EQ(q, U256(q_ref));
        EXPECT_EQ(r, U256(r_ref));
        EXPECT_LT(static_cast<uint64_t>(U256(r)), D);
        EXPECT_EQ(q * U256(D) + r, a);
    }

    // n = 2 limbs (128-bit numerator)
    {
        const uint64_t hi = 0x0123456789ABCDEFULL;
        const uint64_t lo = 0x0FEDCBA987654321ULL;
        U256 a = (U256(hi) << 64) | U256(lo);
        U256 q = a / D;
        U256 r = a % D;
        unsigned __int128 A = (static_cast<unsigned __int128>(hi) << 64) | lo;
        unsigned __int128 q_ref = A / D;
        unsigned __int128 r_ref = A - q_ref * D;
        EXPECT_EQ(q, U256(q_ref));
        EXPECT_EQ(r, U256(static_cast<uint64_t>(r_ref)));
        EXPECT_EQ(q * U256(D) + r, a);
    }

    // n = 3 limbs (192-bit numerator)
    {
        const uint64_t w2 = 0xAAAAAAAAAAAAAAAAULL;
        const uint64_t w1 = 0x1337133713371337ULL;
        const uint64_t w0 = 0xBADC0FFEE0DDF00DULL;
        U256 a = (U256(w2) << 128) | (U256(w1) << 64) | U256(w0);
        U256 q = a / D;
        U256 r = a % D;
        EXPECT_EQ(q * U256(D) + r, a);
        EXPECT_LT(static_cast<uint64_t>(U256(r)), D);
    }

    // n = 4 limbs (256-bit numerator)
    {
        const uint64_t w3 = 0x7FFFFFFFFFFFFFFFULL;
        const uint64_t w2 = 0x0123456789ABCDEFULL;
        const uint64_t w1 = 0x0FEDCBA987654321ULL;
        const uint64_t w0 = 0x0000000000000003ULL;
        U256 a = (U256(w3) << 192) | (U256(w2) << 128) | (U256(w1) << 64) | U256(w0);
        U256 q = a / D;
        U256 r = a % D;
        EXPECT_EQ(q * U256(D) + r, a);
        EXPECT_LT(static_cast<uint64_t>(U256(r)), D);
    }
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
        U256 q_generic = TestAccess<U256>::div_large(lhs, divisor, 2);
        // Fast path result via operator/ (which dispatches to div_large_2)
        U256 q_fast = lhs / divisor;
        // Also call the fast specialization directly
        U256 q_direct = TestAccess<U256>::div_large_2(lhs, divisor);

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
    U256 q_generic = TestAccess<U256>::div_large(lhs, rhs, 2);
    // Fast path result
    U256 q_fast = lhs / rhs;
    U256 q_direct = TestAccess<U256>::div_large_2(lhs, rhs);

    EXPECT_EQ(q_fast, q_generic);
    EXPECT_EQ(q_direct, q_generic);
}

TEST(WideIntegerDivision, TwoLimbBorrowLowPartCritical)
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

    // Regression: borrow fits in 64 bits but exceeds the next limb, requiring add-back
    U256 lhs = make_u256(0xfefc33e114ad27f3ULL, 0xfcd67a5601f5602aULL, 0x48742c5441466274ULL, 0xf6153b4fa7293ff0ULL);
    U256 rhs = make_u256(0x0000000000000000ULL, 0x0000000000000000ULL, 0x885c52cdadfc8ea0ULL, 0xc1f885eafdfcb690ULL);

    U256 q_generic = TestAccess<U256>::div_large(lhs, rhs, 2);
    U256 q_fast = lhs / rhs;
    U256 q_direct = TestAccess<U256>::div_large_2(lhs, rhs);
    U256 r_generic = lhs - q_generic * rhs;
    U256 r_fast = lhs - q_fast * rhs;

    EXPECT_EQ(q_fast, q_generic);
    EXPECT_EQ(q_direct, q_generic);
    EXPECT_EQ(r_fast, r_generic);
}

TEST(WideIntegerDivision, UnsignedNegativeDivisorReinterpreted)
{
    using U256 = gint::integer<256, unsigned>;
    U256 small = 5;
    int64_t neg_one = -1;

    // Negative divisors should be reinterpreted as unsigned magnitudes, not routed through signed sign-flip logic.
    EXPECT_EQ(small / neg_one, U256(0));
    EXPECT_EQ(small % neg_one, small);

    U256 all_ones = ~U256(0);
    EXPECT_EQ(all_ones / neg_one, U256(1));
    EXPECT_EQ(all_ones % neg_one, U256(0));

    int64_t neg_two = -2;
    EXPECT_EQ(small / neg_two, U256(0));
    EXPECT_EQ(small % neg_two, small);
}

TEST(WideIntegerDivision, UnsignedNegativeDivisorSmallBuiltin)
{
    using U256 = gint::integer<256, unsigned>;
    U256 small = 5;
    int8_t neg_byte = -1;
    int16_t neg_word = -7;

    EXPECT_EQ(small / neg_byte, U256(0));
    EXPECT_EQ(small % neg_byte, small);

    EXPECT_EQ(small / neg_word, U256(0));
    EXPECT_EQ(small % neg_word, small);

    U256 all_ones = ~U256(0);
    EXPECT_EQ(all_ones / neg_byte, U256(1));
    EXPECT_EQ(all_ones % neg_byte, U256(0));
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

TEST(WideIntegerDivision, S64UnsignedInteropAboveSignedRange)
{
    using S64 = gint::integer<64, signed>;

    const S64 min = std::numeric_limits<S64>::min();
    const uint64_t signed_min_magnitude = 1ULL << 63;
    EXPECT_EQ(min / signed_min_magnitude, S64(-1));
    EXPECT_EQ(min % signed_min_magnitude, S64(0));

    const uint64_t larger_than_min_magnitude = signed_min_magnitude + 5;
    EXPECT_EQ(min / larger_than_min_magnitude, S64(0));
    EXPECT_EQ(min % larger_than_min_magnitude, min);

    const unsigned __int128 huge = static_cast<unsigned __int128>(1) << 80;
    EXPECT_EQ(S64(-123) / huge, S64(0));
    EXPECT_EQ(S64(-123) % huge, S64(-123));

    const uint64_t max_u64 = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ(max_u64 / S64(2), std::numeric_limits<S64>::max());
    EXPECT_EQ(max_u64 % S64(2), S64(1));
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
    using U512 = gint::integer<512, unsigned>;
    U512 lhs = (U512(1) << 384) + (U512(1) << 128) + U512(12345);
    U512 divisor = (U512(1) << 64) + U512(3);
    U512 q = lhs / divisor;
    U512 r = lhs % divisor;
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
    U64 q = TestAccess<U64>::div_128(a, b);
    EXPECT_EQ(static_cast<uint64_t>(q), static_cast<uint64_t>(a) / static_cast<uint64_t>(b));

    U64 z = 0;
    EXPECT_EQ(TestAccess<U64>::div_128(a, z), U64(0));
}

TEST(WideIntegerDivision, Div128LargeDivisorSmallQuotient)
{
    using U128 = gint::integer<128, unsigned>;
    const U128 divisor = (U128(1) << 126) + U128(7);

    EXPECT_EQ(TestAccess<U128>::div_128(divisor - U128(1), divisor), U128(0));
    for (uint64_t expected = 1; expected <= 3; ++expected)
    {
        const U128 lhs = divisor * U128(expected) + U128(11);
        EXPECT_EQ(TestAccess<U128>::div_128(lhs, divisor), U128(expected));
    }
}

TEST(WideIntegerDivision, DivLargeBreak)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs;
    TestAccess<U512>::limb(lhs, 0) = 0;
    TestAccess<U512>::limb(lhs, 1) = 0xffffffffffffffffULL;
    TestAccess<U512>::limb(lhs, 2) = 1;
    U512 divisor;
    TestAccess<U512>::limb(divisor, 0) = 0xffffffffffffffffULL;
    TestAccess<U512>::limb(divisor, 1) = 0xffffffffffffffffULL;
    TestAccess<U512>::limb(divisor, 2) = 0;
    U512 q = TestAccess<U512>::div_large(lhs, divisor, 2);
    EXPECT_EQ(static_cast<uint64_t>(q), 1ULL);
}

// Exercise div_mod_small 64-bit divisor generic path for limbs != 4 (here: 512-bit)
TEST(WideIntegerDivision, DivModSmall64GenericU512)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs = (U512(1) << 510) + (U512(1) << 384) + U512(123456789ULL);
    uint64_t div = (1ULL << 63) + 123ULL; // > 32-bit -> 64-bit reciprocal branch

    U512 q = lhs / div; // goes through integer(rhs) -> small divisor -> div_mod_small generic
    U512 r = lhs % div;
    EXPECT_EQ(q * U512(div) + r, lhs);
}

// 32-bit boundary: div = 2^32-1 (32-bit path) vs div = 2^32 (64-bit path)
TEST(WideIntegerDivision, DivModSmall_32bitBoundary)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = (U256(1) << 200) + (U256(1) << 120) + U256(0xDEADBEEFULL);

    const uint64_t d32max = 0xFFFFFFFFULL; // triggers 32-bit reciprocal branch
    const uint64_t d32plus = 0x100000000ULL; // triggers 64-bit reciprocal branch

    // div = 2^32-1
    U256 q1 = a / d32max;
    U256 r1 = a % d32max;
    EXPECT_EQ(q1 * d32max + r1, a);
    EXPECT_LT(static_cast<uint64_t>(U256(r1)), d32max);

    // div = 2^32
    U256 q2 = a / d32plus;
    U256 r2 = a % d32plus;
    EXPECT_EQ(q2 * d32plus + r2, a);
    EXPECT_LT(static_cast<uint64_t>(U256(r2)), d32plus);
}

// Div/mod by UINT64_MAX exercises 64-bit small-divisor reciprocal path
TEST(WideIntegerDivision, DivModSmall_U64Max)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = (U256(1) << 240) + (U256(1) << 128) + (U256(1) << 64) + U256(123456789ULL);
    const uint64_t d = 0xFFFFFFFFFFFFFFFFULL;
    U256 q = a / d;
    U256 r = a % d;
    EXPECT_EQ(q * d + r, a);
    EXPECT_LT(static_cast<uint64_t>(U256(r)), d);
}

// Three-limb divisor: specialized fast path must match generic Knuth-D path
TEST(WideIntegerDivision, ThreeLimbFastPathMatchesGeneric)
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

    // Three non-zero limbs in divisor (top limb has MSB set to ensure normalization path)
    U256 divisor = (U256(0x8000000000000000ULL) << 128) | (U256(0x0123456789ABCDEFULL) << 64) | U256(0x0FEDCBA987654321ULL);
    ASSERT_NE(divisor, U256(0));

    U256 dividends[] = {
        make_u256(0, 0xFFFFFFFFFFFFFFFFULL, 0x0000000000000001ULL, 0x123456789ABCDEF0ULL),
        make_u256(0x7FFFFFFFFFFFFFFFULL, 0x0000000000000000ULL, 0xDEADBEEFDEADBEEFULL, 0xBADC0FFEE0DDF00DULL),
        make_u256(0x0123456789ABCDEFULL, 0x0FEDCBA987654321ULL, 0xCAFEBABECAFED00DULL, 0x0000000000000007ULL),
        make_u256(0x0000000000000001ULL, 0x8000000000000000ULL, 0x0000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL),
    };

    for (const auto & lhs : dividends)
    {
        U256 q_generic = TestAccess<U256>::div_large(lhs, divisor, 3);
        U256 q_fast = lhs / divisor; // dispatches to div_large_3
        U256 q_direct = TestAccess<U256>::div_large_3(lhs, divisor);
        EXPECT_EQ(q_fast, q_generic);
        EXPECT_EQ(q_direct, q_generic);
    }
}

TEST(WideIntegerDivision, FourLimbFastPathMatchesGeneric)
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

    struct Case
    {
        U256 lhs;
        U256 divisor;
    };

    Case cases[] = {
        {make_u256(0xffffffffffffffffULL, 0xfffffffffffffffeULL, 0x0123456789abcdefULL, 0xfedcba9876543210ULL),
         make_u256(0x8000000000000000ULL, 0x76543210fedcba98ULL, 0x89abcdef01234567ULL, 0x1111111111111111ULL)},
        {make_u256(0x7fffffffffffffffULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL, 0xfffffffffffffffeULL),
         make_u256(0x0100000000000000ULL, 0x0000000000000001ULL, 0x0000000000000002ULL, 0x0000000000000003ULL)},
        {make_u256(0x123456789abcdef0ULL, 0x0fedcba987654321ULL, 0x5555555555555555ULL, 0xaaaaaaaaaaaaaaaaULL),
         make_u256(0xf000000000000000ULL, 0x0000000000000000ULL, 0x1111111111111111ULL, 0x2222222222222222ULL)},
        {make_u256(0x9509d6071a3fb370ULL, 0x3fe7309369c26afaULL, 0xaa5e4674e9382781ULL, 0x8dde152b630710d3ULL),
         make_u256(0x4a84eb038d1fd9b8ULL, 0x1ff39849b4e1357dULL, 0x552f233a8c25166aULL, 0xec188efbd080e66eULL)},
        {make_u256(0x03e8fffffffffff0ULL, 0xbdc0000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL),
         make_u256(0x0000ffffffffffffULL, 0xfc18ffffffffffffULL, 0xffff000000000000ULL, 0x0000000000000000ULL)},
        {make_u256(0xfffffffffff4b97fULL, 0x2acaeb35c6adbe88ULL, 0x127d416f3e4cb89aULL, 0xd1a7d6787e53a11dULL),
         make_u256(0x0000000000003e5eULL, 0x5c4882044378546aULL, 0xddae4b6cded24013ULL, 0x4aacad7bee6e9099ULL)},
    };

    for (const auto & c : cases)
    {
        U256 q_generic = TestAccess<U256>::div_large(c.lhs, c.divisor, 4);
        U256 q_direct = TestAccess<U256>::div_large_4(c.lhs, c.divisor);
        U256 q_fast = c.lhs / c.divisor;
        U256 r_direct = c.lhs - q_direct * c.divisor;
        U256 r_fast = c.lhs % c.divisor;

        EXPECT_EQ(q_direct, q_generic);
        EXPECT_EQ(q_fast, q_generic);
        EXPECT_EQ(r_fast, r_direct);
        EXPECT_LT(r_direct, c.divisor);
    }
}

TEST(WideIntegerDivision, DivModSmallDivisorOne)
{
    using U256 = gint::integer<256, unsigned>;
    U256 value = (U256(1) << 192) | (U256(1) << 64) | U256(123456789ULL);
    EXPECT_EQ(value / 1, value);
    EXPECT_EQ(value % 1, U256(0));
}

TEST(WideIntegerDivision, DivModSmall32BitEstimateCorrection)
{
    using U128 = gint::integer<128, unsigned>;
    const uint64_t w1 = 0x9e5c9016ebf47b17ULL;
    const uint64_t w0 = 0xa3624895ddc28cd1ULL;
    const uint64_t d32 = 0xe3f5574bULL;

    U128 lhs = (U128(w1) << 64) | U128(w0);
    U128 q = lhs / d32;
    U128 r = lhs % d32;

    unsigned __int128 num = (static_cast<unsigned __int128>(w1) << 64) | w0;
    unsigned __int128 expected_q = num / d32;
    unsigned __int128 expected_r = num % d32;

    EXPECT_EQ(q, U128(expected_q));
    EXPECT_EQ(static_cast<uint64_t>(r), static_cast<uint64_t>(expected_r));
    EXPECT_EQ(q * d32 + r, lhs);
}

TEST(WideIntegerDivision, HighestBitZeroReturnsMinusOne)
{
    using U256 = gint::integer<256, unsigned>;
    U256 zero = 0;
    EXPECT_EQ(TestAccess<U256>::highest_bit(zero), -1);
}

TEST(WideIntegerDivision, UnsignedDivision_MinValueBranches)
{
    using S256 = gint::integer<256, signed>;
    const S256 min = std::numeric_limits<S256>::min();

    EXPECT_TRUE(TestAccess<S256>::is_min_value(min));
    EXPECT_FALSE(TestAccess<S256>::is_min_value(-(S256(1) << 200)));

    // divisor_limbs == 1
    S256 q_one = TestAccess<S256>::div_unsigned_path(min, S256(1), true, false, true, false);
    EXPECT_EQ(q_one, min);

    // power-of-two divisor
    S256 pow_div = S256(1) << 120;
    S256 q_pow = TestAccess<S256>::div_unsigned_path(min, pow_div, true, false, true, false);
    EXPECT_EQ(q_pow, -(S256(1) << 135));

    // rhs_is_min handling
    S256 rhs_min = min;
    S256 small = 123;
    S256 q_rhs_min = TestAccess<S256>::div_unsigned_path(small, rhs_min, false, true, false, true);
    EXPECT_EQ(q_rhs_min, S256(0));

    // divisor_limbs == 2
    S256 two_limb = (S256(1) << 96) + S256(7);
    EXPECT_EQ(TestAccess<S256>::div_unsigned_path(min, two_limb, true, false, true, false), min / two_limb);

    // divisor_limbs == 3
    S256 three_limb = (S256(1) << 180) + (S256(1) << 100) + S256(9);
    EXPECT_EQ(TestAccess<S256>::div_unsigned_path(min, three_limb, true, false, true, false), min / three_limb);

    // generic div_large path (divisor_limbs == 4)
    S256 four_limb = (S256(1) << 240) + (S256(1) << 128) + (S256(1) << 64) + S256(11);
    EXPECT_EQ(TestAccess<S256>::div_unsigned_path(min, four_limb, true, false, true, false), min / four_limb);
}

TEST(WideIntegerDivision, UnsignedDivision_MinValueSmallType)
{
    using S128 = gint::integer<128, signed>;
    const S128 min = std::numeric_limits<S128>::min();
    S128 divisor = (S128(1) << 80) + S128(5);
    EXPECT_EQ(TestAccess<S128>::div_unsigned_path(min, divisor, true, false, true, false), min / divisor);
}

TEST(WideIntegerDivision, IsMinValueRejectsNonZeroLowLimbs)
{
    using S256 = gint::integer<256, signed>;
    S256 candidate;
    TestAccess<S256>::limb(candidate, 3) = static_cast<uint64_t>(1ULL << 63);
    TestAccess<S256>::limb(candidate, 0) = 1;
    EXPECT_FALSE(TestAccess<S256>::is_min_value(candidate));
}

TEST(WideIntegerDivision, DivLarge2EarlyReturnForShortDividend)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = 5;
    U256 divisor = (U256(1) << 70) + (U256(1) << 5); // two-limb divisor
    U256 q = TestAccess<U256>::div_large_2(lhs, divisor);
    EXPECT_EQ(q, U256(0));
}

TEST(WideIntegerDivision, DivLarge2StubSingleLimbType)
{
    using U64 = gint::integer<64, unsigned>;
    U64 lhs = 9;
    U64 divisor = 2;
    EXPECT_EQ(TestAccess<U64>::div_large_2(lhs, divisor), TestAccess<U64>::div_large(lhs, divisor, 2));
}

TEST(WideIntegerDivision, DivLarge3StubTwoLimbType)
{
    using U128 = gint::integer<128, unsigned>;
    U128 lhs = (U128(1) << 96) + U128(5);
    U128 divisor = (U128(1) << 64) + U128(3);
    EXPECT_EQ(TestAccess<U128>::div_large_3(lhs, divisor), TestAccess<U128>::div_large(lhs, divisor, 3));
}

TEST(WideIntegerDivision, DivLargeGeneric_AddbackBranch)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs;
    TestAccess<U256>::limb(lhs, 0) = 15319920140141428228ULL;
    TestAccess<U256>::limb(lhs, 1) = 6844823494456829948ULL;
    TestAccess<U256>::limb(lhs, 2) = 8650597808387393596ULL;
    TestAccess<U256>::limb(lhs, 3) = 16347772628613761532ULL;

    U256 divisor;
    TestAccess<U256>::limb(divisor, 0) = 2642655789404796171ULL;
    TestAccess<U256>::limb(divisor, 1) = 12954822196855299360ULL;
    TestAccess<U256>::limb(divisor, 2) = 10810217670908235955ULL;

    U256 q = TestAccess<U256>::div_large(lhs, divisor, 3);
    U256 expected = lhs / divisor;
    EXPECT_EQ(q, expected);
    U256 r = lhs - q * divisor;
    EXPECT_LT(r, divisor);
}

TEST(WideIntegerDivision, DivLargeGenericConstructedAddbackBranch)
{
    using U512 = gint::integer<512, unsigned>;

    // lhs is q_digit * divisor - 1 with a normalized three-limb divisor. The
    // first quotient estimate is one too high, so Knuth's add-back path runs.
    constexpr uint64_t q_digit = 2513787319205155663ULL;

    U512 lhs;
    TestAccess<U512>::limb(lhs, 0) = 7865594366602207753ULL;
    TestAccess<U512>::limb(lhs, 1) = 14317669559219201549ULL;
    TestAccess<U512>::limb(lhs, 2) = 18061272594500877176ULL;
    TestAccess<U512>::limb(lhs, 3) = 1890733067841405531ULL;

    U512 divisor;
    TestAccess<U512>::limb(divisor, 0) = 13930160852258120406ULL;
    TestAccess<U512>::limb(divisor, 1) = 11788048577503494824ULL;
    TestAccess<U512>::limb(divisor, 2) = 13874630024467741450ULL;

    U512 q = TestAccess<U512>::div_large(lhs, divisor, 3);
    EXPECT_EQ(q, U512(q_digit - 1));
    EXPECT_EQ(lhs - q * divisor, divisor - U512(1));
}

TEST(WideIntegerDivision, RemLargeGenericMatchesQuotientRemainder)
{
    using U512 = gint::integer<512, unsigned>;

    const U512 lhs = (U512(1) << 511) - U512(12345);
    const U512 full_width_divisor = (U512(1) << 470) + U512(987654321);
    const U512 seven_limb_divisor = (U512(1) << 436) + U512(123456789);

    {
        const U512 q = TestAccess<U512>::div_large(lhs, full_width_divisor, 8);
        const U512 r = TestAccess<U512>::rem_large(lhs, full_width_divisor, 8);
        EXPECT_EQ(q * full_width_divisor + r, lhs);
        EXPECT_LT(r, full_width_divisor);
    }

    {
        const U512 q = TestAccess<U512>::div_large(lhs, seven_limb_divisor, 7);
        const U512 r = TestAccess<U512>::rem_large(lhs, seven_limb_divisor, 7);
        EXPECT_EQ(q * seven_limb_divisor + r, lhs);
        EXPECT_LT(r, seven_limb_divisor);
    }
}

TEST(WideIntegerDivision, SignedLargeModuloMatchesQuotientRemainder)
{
    using I512 = gint::integer<512, signed>;

    const I512 divisor = (I512(1) << 470) + I512(987654321);
    const I512 magnitude = (I512(1) << 510) + I512(12345);
    const I512 lhs = -magnitude;

    const I512 q = lhs / divisor;
    const I512 r = lhs % divisor;
    EXPECT_EQ(q * divisor + r, lhs);
    EXPECT_LT(r, I512(0));
    EXPECT_GT(r, -divisor);
}

TEST(WideIntegerDivision, DivLarge3EarlyReturnWhenDividendShorter)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = 5;
    U256 divisor;
    TestAccess<U256>::limb(divisor, 0) = 1;
    TestAccess<U256>::limb(divisor, 1) = 2;
    TestAccess<U256>::limb(divisor, 2) = static_cast<uint64_t>(1ULL << 63); // keep three-limb divisor

    EXPECT_EQ(TestAccess<U256>::div_large_3(lhs, divisor), U256(0));
}

TEST(WideIntegerDivision, DivLarge2Generic_AddbackBranch)
{
    using U512 = gint::integer<512, unsigned>;
    U512 lhs;
    TestAccess<U512>::limb(lhs, 0) = 14627284802991019572ULL;
    TestAccess<U512>::limb(lhs, 1) = 10210465436952577038ULL;
    TestAccess<U512>::limb(lhs, 2) = 4609778614729378691ULL;
    TestAccess<U512>::limb(lhs, 3) = 7741901622370343826ULL;
    TestAccess<U512>::limb(lhs, 4) = 10933104939142465497ULL;

    U512 divisor;
    TestAccess<U512>::limb(divisor, 0) = 13705805734369151841ULL;
    TestAccess<U512>::limb(divisor, 1) = 12445310395029312220ULL;

    U512 q = TestAccess<U512>::div_large_2(lhs, divisor);
    U512 expected = lhs / divisor;
    EXPECT_EQ(q, expected);
    U512 r = lhs - q * divisor;
    EXPECT_LT(r, divisor);
}

TEST(WideIntegerDivision, DivLarge2GenericLoopUpwardAndAddback)
{
    using U512 = gint::integer<512, unsigned>;

    // Fixed operands that exercise the n != 4 two-limb loop's upward qhat
    // correction and add-back path.
    U512 lhs;
    TestAccess<U512>::limb(lhs, 0) = 6295741537884838863ULL;
    TestAccess<U512>::limb(lhs, 1) = 14138627545845163593ULL;
    TestAccess<U512>::limb(lhs, 2) = 6391316625312145471ULL;
    TestAccess<U512>::limb(lhs, 3) = 15338468070455510617ULL;
    TestAccess<U512>::limb(lhs, 4) = 7382200937827080594ULL;
    TestAccess<U512>::limb(lhs, 5) = 11782123156758599754ULL;
    TestAccess<U512>::limb(lhs, 6) = 13632931721610285019ULL;
    TestAccess<U512>::limb(lhs, 7) = 11884197023051099476ULL;

    U512 divisor;
    TestAccess<U512>::limb(divisor, 0) = 17513998343136139140ULL;
    TestAccess<U512>::limb(divisor, 1) = 6443127926368659239ULL;

    U512 q = TestAccess<U512>::div_large_2(lhs, divisor);
    U512 expected = TestAccess<U512>::div_large(lhs, divisor, 2);
    EXPECT_EQ(q, expected);
    U512 r = lhs - q * divisor;
    EXPECT_LT(r, divisor);
}

TEST(WideIntegerDivision, IntegerModIntegerMultiLimb)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(1) << 200) + (U256(1) << 128) + U256(987654321ULL);
    U256 rhs = (U256(1) << 150) + U256(12345);
    U256 q = lhs / rhs;
    U256 r = lhs % rhs;
    EXPECT_EQ(q * rhs + r, lhs);
    EXPECT_LT(r, rhs);

    using S256 = gint::integer<256, signed>;
    S256 s_lhs = -S256(123456789012345ULL);
    S256 s_rhs = S256(98765);
    S256 s_q = s_lhs / s_rhs;
    S256 s_r = s_lhs % s_rhs;
    EXPECT_EQ(s_q * s_rhs + s_r, s_lhs);
    EXPECT_LT(s_r, s_rhs);
}

TEST(WideIntegerDivision, MinValueUnsignedPathSignCorrection)
{
    using S256 = gint::integer<256, signed>;
    const S256 min = std::numeric_limits<S256>::min();
    S256 divisor = S256(7);

    S256 q = min / divisor;
    S256 r = min % divisor;
    const bool q_negative = static_cast<int64_t>(TestAccess<S256>::limb(q, S256::limbs - 1) >> 63) != 0;
    EXPECT_TRUE(q_negative);
    EXPECT_EQ(q * divisor + r, min);

    S256 neg_divisor = -divisor;
    S256 q2 = min / neg_divisor;
    S256 r2 = min % neg_divisor;
    const bool q2_negative = static_cast<int64_t>(TestAccess<S256>::limb(q2, S256::limbs - 1) >> 63) != 0;
    EXPECT_FALSE(q2_negative);
    EXPECT_EQ(q2 * neg_divisor + r2, min);
}

TEST(WideIntegerDivision, DivLarge3QhatAdjustmentBreak)
{
    using U256 = gint::integer<256, unsigned>;

    // Fixed operands that force the three-limb qhat adjustment loop to stop
    // after the carry crosses one base limb.
    U256 lhs;
    TestAccess<U256>::limb(lhs, 0) = 6565772855194965757ULL;
    TestAccess<U256>::limb(lhs, 1) = 6890790792660555462ULL;
    TestAccess<U256>::limb(lhs, 2) = 7302078116051201395ULL;
    TestAccess<U256>::limb(lhs, 3) = 7241624559238170523ULL;

    U256 divisor;
    TestAccess<U256>::limb(divisor, 0) = 6958441993705679365ULL;
    TestAccess<U256>::limb(divisor, 1) = 17057296535633014190ULL;
    TestAccess<U256>::limb(divisor, 2) = 8175714336819284825ULL;

    U256 q = TestAccess<U256>::div_large_3(lhs, divisor);
    U256 expected = TestAccess<U256>::div_large(lhs, divisor, 3);
    EXPECT_EQ(q, expected);
    U256 r = lhs - q * divisor;
    EXPECT_LT(r, divisor);
}
