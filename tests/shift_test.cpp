#include <gint/gint.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace
{
template <typename Int>
using TestAccess = gint::detail::integer_test_access<Int::bits, typename Int::signed_tag>;

template <typename Int>
Int patterned_value(uint64_t salt)
{
    Int value = 0;
    for (std::size_t i = 0; i < Int::limbs; ++i)
    {
        uint64_t limb = 0x9e3779b97f4a7c15ULL * static_cast<uint64_t>(i + 1);
        limb ^= salt + (static_cast<uint64_t>(i) << 32);
        limb ^= limb >> 29;
        TestAccess<Int>::limb(value, i) = limb;
    }
    return value;
}

template <typename Int>
Int reference_left_shift(const Int & input, std::size_t shift)
{
    Int result = 0;
    if (shift >= Int::bits)
        return result;

    const std::size_t limb_shift = shift / 64;
    const unsigned bit_shift = static_cast<unsigned>(shift % 64);
    for (std::size_t i = 0; i < Int::limbs; ++i)
    {
        typename Int::limb_type part = 0;
        if (i >= limb_shift)
        {
            const std::size_t src = i - limb_shift;
            part = TestAccess<Int>::limb(input, src);
            if (bit_shift)
            {
                part <<= bit_shift;
                if (src)
                    part |= TestAccess<Int>::limb(input, src - 1) >> (64U - bit_shift);
            }
        }
        TestAccess<Int>::limb(result, i) = part;
    }
    return result;
}

template <typename Int>
Int reference_right_shift(const Int & input, std::size_t shift)
{
    const bool is_signed_t = std::is_same<typename Int::signed_tag, signed>::value;
    const bool neg = is_signed_t && (TestAccess<Int>::limb(input, Int::limbs - 1) >> 63);
    const typename Int::limb_type fill = neg ? ~typename Int::limb_type(0) : typename Int::limb_type(0);

    Int result = 0;
    if (shift >= Int::bits)
    {
        for (std::size_t i = 0; i < Int::limbs; ++i)
            TestAccess<Int>::limb(result, i) = fill;
        return result;
    }

    const std::size_t limb_shift = shift / 64;
    const unsigned bit_shift = static_cast<unsigned>(shift % 64);
    for (std::size_t i = 0; i < Int::limbs; ++i)
    {
        const std::size_t src = i + limb_shift;
        typename Int::limb_type part = fill;
        if (src < Int::limbs)
        {
            part = TestAccess<Int>::limb(input, src);
            if (bit_shift)
            {
                part >>= bit_shift;
                if (src + 1 < Int::limbs)
                    part |= TestAccess<Int>::limb(input, src + 1) << (64U - bit_shift);
                else
                    part |= fill << (64U - bit_shift);
            }
        }
        TestAccess<Int>::limb(result, i) = part;
    }
    return result;
}

template <typename Int>
void expect_shift_matches_reference(const Int & value)
{
    const std::size_t shifts[] = {1, 31, 63, 64, 65, 73, 127, 191, 255, 319, 511, Int::bits - 1, Int::bits};
    for (std::size_t i = 0; i < sizeof(shifts) / sizeof(shifts[0]); ++i)
    {
        const int shift = static_cast<int>(shifts[i]);
        EXPECT_EQ(value << shift, reference_left_shift(value, shifts[i])) << "left shift " << shift;
        EXPECT_EQ(value >> shift, reference_right_shift(value, shifts[i])) << "right shift " << shift;
    }
}
} // namespace

#if __cplusplus >= 201402L
constexpr gint::integer<512, unsigned> constexpr_shift_512 = gint::integer<512, unsigned>(1) << 511;
constexpr gint::integer<1024, unsigned> constexpr_shift_1024 = gint::integer<1024, unsigned>(1) << 1000;
static_assert((constexpr_shift_512 >> 511) == gint::integer<512, unsigned>(1), "512-bit right shift should stay constexpr");
static_assert((constexpr_shift_1024 >> 999) == gint::integer<1024, unsigned>(2), "1024-bit right shift should stay constexpr");
#endif

TEST(WideIntegerShift, Basic)
{
    gint::integer<128, unsigned> a = 1;
    auto left = a << 100;
    EXPECT_EQ(gint::to_string(left), "1267650600228229401496703205376");
    auto b = gint::integer<128, unsigned>(1) << 127;
    auto right = b >> 64;
    EXPECT_EQ(gint::to_string(right), "9223372036854775808");
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

TEST(WideIntegerShift, ExactBoundaryBitCounts)
{
    using U128 = gint::integer<128, unsigned>;
    using S128 = gint::integer<128, signed>;
    using U256 = gint::integer<256, unsigned>;

    // Left shift by 63 and 64 on 128-bit
    U128 one = 1;
    EXPECT_EQ(one << 63, U128(1) << 63);
    EXPECT_EQ(one << 64, U128(1) << 64);

    // Bridge across limbs: prepare value with lower and next limb bits
    U256 val = U256(1);
    U256 v63 = val << 63;
    U256 v64 = val << 64;
    EXPECT_NE(v63, U256(0));
    EXPECT_NE(v64, U256(0));
    // Shifting by 64 moves limb index by one
    EXPECT_EQ(v64, (U256(1) << 64));

    // Right shift by 63/64/127/128 on unsigned 128-bit
    U128 top = U128(1) << 127;
    EXPECT_EQ(top >> 63, U128(1) << 64);
    EXPECT_EQ(top >> 64, U128(1) << 63);
    EXPECT_EQ(top >> 127, U128(1));
    EXPECT_EQ(top >> 128, U128(0));

    // Signed arithmetic right shift should sign-extend at boundaries
    S128 minus_one = S128(-1);
    EXPECT_EQ(minus_one >> 63, S128(-1));
    EXPECT_EQ(minus_one >> 64, S128(-1));
    EXPECT_EQ(minus_one >> 127, S128(-1));
    EXPECT_EQ(minus_one >> 128, S128(-1));
}

TEST(WideIntegerShift, UInt128UnsignedShiftAmountMatchesReference)
{
    using U128 = gint::integer<128, unsigned>;
    using S128 = gint::integer<128, signed>;

    U128 unsigned_value = patterned_value<U128>(0xbadc0ffee0ddf00dULL);
    S128 signed_value = patterned_value<S128>(0x13579bdf2468ace0ULL);
    TestAccess<S128>::limb(signed_value, S128::limbs - 1) |= uint64_t(1) << 63;

    const unsigned shifts[] = {0, 1, 63, 64, 65, 73, 127, 128, 192};
    for (std::size_t i = 0; i < sizeof(shifts) / sizeof(shifts[0]); ++i)
    {
        const unsigned shift = shifts[i];
        EXPECT_EQ(unsigned_value << shift, reference_left_shift(unsigned_value, shift)) << "unsigned left shift " << shift;
        EXPECT_EQ(signed_value << shift, reference_left_shift(signed_value, shift)) << "signed left shift " << shift;
    }
}

TEST(WideIntegerShift, Int128SizeTShiftAmountMatchesReference)
{
    using S128 = gint::integer<128, signed>;

    S128 signed_value = patterned_value<S128>(0x0f1e2d3c4b5a6978ULL);
    TestAccess<S128>::limb(signed_value, S128::limbs - 1) |= uint64_t(1) << 63;

    const std::size_t shifts[] = {0, 1, 63, 64, 65, 73, 127, 128, 192};
    for (std::size_t i = 0; i < sizeof(shifts) / sizeof(shifts[0]); ++i)
    {
        const std::size_t shift = shifts[i];
        EXPECT_EQ(signed_value >> shift, reference_right_shift(signed_value, shift)) << "signed right shift " << shift;
    }
}

TEST(WideIntegerShift, LongShiftAmountMatchesReference)
{
    using S128 = gint::integer<128, signed>;

    S128 signed_value = patterned_value<S128>(0x1020304050607080ULL);
    TestAccess<S128>::limb(signed_value, S128::limbs - 1) |= uint64_t(1) << 63;

    const long shifts[] = {0, 1, 63, 64, 65, 127, 128};
    for (std::size_t i = 0; i < sizeof(shifts) / sizeof(shifts[0]); ++i)
    {
        const long shift = shifts[i];
        EXPECT_EQ(signed_value >> shift, reference_right_shift(signed_value, static_cast<std::size_t>(shift)))
            << "signed right shift " << shift;
    }
}

TEST(WideIntegerShift, NativeLargeUnsignedShiftAmountsDoNotNarrow)
{
    using U128 = gint::integer<128, unsigned>;
    using S128 = gint::integer<128, signed>;

    const std::size_t huge = std::numeric_limits<std::size_t>::max();
    const unsigned huge_unsigned = std::numeric_limits<unsigned>::max();

    EXPECT_EQ(U128(1) << huge, U128(0));
    EXPECT_EQ(U128(1) >> huge, U128(0));
    EXPECT_EQ(S128(-8) >> huge, S128(-1));
    EXPECT_EQ(U128(1) << huge_unsigned, U128(0));
    EXPECT_EQ(U128(1) >> huge_unsigned, U128(0));
    EXPECT_EQ(S128(-8) >> huge_unsigned, S128(-1));

    U128 unsigned_value = 1;
    unsigned_value <<= huge;
    EXPECT_EQ(unsigned_value, U128(0));
    unsigned_value = 1;
    unsigned_value >>= huge_unsigned;
    EXPECT_EQ(unsigned_value, U128(0));

    S128 signed_value = -8;
    signed_value >>= huge;
    EXPECT_EQ(signed_value, S128(-1));
    signed_value = -8;
    signed_value >>= huge_unsigned;
    EXPECT_EQ(signed_value, S128(-1));
}

TEST(WideIntegerShift, UInt256ExactWholeLimbRightShifts)
{
    using U256 = gint::integer<256, unsigned>;

    U256 value = U256(0x1111111111111111ULL);
    value |= U256(0x2222222222222222ULL) << 64;
    value |= U256(0x3333333333333333ULL) << 128;
    value |= U256(0x4444444444444444ULL) << 192;

    U256 by64 = U256(0x2222222222222222ULL);
    by64 |= U256(0x3333333333333333ULL) << 64;
    by64 |= U256(0x4444444444444444ULL) << 128;
    EXPECT_EQ(value >> 64, by64);

    EXPECT_EQ(value >> 192, U256(0x4444444444444444ULL));
}

TEST(WideIntegerShift, SignedRightShiftWrapper)
{
    using S128 = gint::integer<128, signed>;
    S128 v = 8;
    auto r = v >> 2; // wrapper delegates to >>= internally
    EXPECT_EQ(r, S128(2));
}

TEST(WideIntegerShift, SignedArithmeticRightShift)
{
    using S128 = gint::integer<128, signed>;
    // Basic sign-propagation within a limb
    EXPECT_EQ(S128(-8) >> 1, S128(-4));
    EXPECT_EQ(S128(-8) >> 2, S128(-2));
    EXPECT_EQ(S128(-1) >> 1, S128(-1));

    // Cross-limb: for 256-bit, shifting negative values must fill with ones
    using S256 = gint::integer<256, signed>;
    EXPECT_EQ(S256(-1) >> 65, S256(-1));
}

TEST(WideIntegerShift, EdgeCasesSigned512)
{
    using S512 = gint::integer<512, signed>;
    S512 x = 42;
    S512 t = x;
    t <<= 0;
    EXPECT_EQ(t, x);
    t >>= 0;
    EXPECT_EQ(t, x);
    t <<= 600; // >= total_bits
    EXPECT_EQ(t, S512(0));
    t = x;
    t >>= 600; // >= total_bits
    EXPECT_EQ(t, S512(0));
}

TEST(WideIntegerShift, EdgeCasesSigned512Negative)
{
    using S512 = gint::integer<512, signed>;
    S512 x = -42;
    S512 t = x;
    t >>= 600; // >= total_bits
    // Arithmetic shift of negative by >= width yields -1
    EXPECT_EQ(t, S512(-1));
}

TEST(WideIntegerShift, GenericWideUnsignedMatchesReference)
{
    expect_shift_matches_reference(patterned_value<gint::integer<128, unsigned>>(0x123456789abcdef0ULL));
    expect_shift_matches_reference(patterned_value<gint::integer<512, unsigned>>(0x0fedcba987654321ULL));
    expect_shift_matches_reference(patterned_value<gint::integer<1024, unsigned>>(0xa5a5a5a55a5a5a5aULL));
}

TEST(WideIntegerShift, GenericWideSignedRightShiftMatchesReference)
{
    using S512 = gint::integer<512, signed>;
    S512 value = patterned_value<S512>(0x4242424242424242ULL);
    TestAccess<S512>::limb(value, S512::limbs - 1) |= uint64_t(1) << 63;
    expect_shift_matches_reference(value);
}
