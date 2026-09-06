#include <gint/gint.h>
#include <gtest/gtest.h>

#include <array>

namespace
{
template <typename Int>
using Access = gint::detail::integer_test_access<Int::bits, typename Int::signed_tag>;

template <typename Int>
Int from_words(const std::array<uint64_t, Int::limbs> & words)
{
    Int result;
    for (size_t i = 0; i < Int::limbs; ++i)
        Access<Int>::limb(result, i) = words[i];
    return result;
}

TEST(IntegerStorage, SmallDivisionOverwritesTheWholeOutput)
{
    using Int = gint::UInt256;
    const Int dividend = from_words<Int>({{17, 3, 1, 0}});
    Int quotient = ~Int(0);
    quotient = dividend / uint64_t(1);
    EXPECT_EQ(quotient, dividend);

    quotient = ~Int(0);
    quotient = dividend / uint64_t(64);
    EXPECT_EQ(quotient, from_words<Int>({{uint64_t(3) << 58, uint64_t(1) << 58, 0, 0}}));
    EXPECT_EQ(dividend % uint64_t(64), Int(17));

    quotient = ~Int(0);
    quotient = Int(0) / uint64_t(7);
    EXPECT_EQ(quotient, Int(0));
}

template <size_t Limbs>
void check_small_division_used_limbs()
{
    using Int = gint::integer<Limbs * 64, unsigned>;
    const uint64_t divisors[] = {3, 0x10000000003ULL, 0x8000000000000001ULL};
    for (uint64_t divisor : divisors)
    {
        for (size_t used = 0; used <= Limbs; ++used)
        {
            Int dividend;
            Int expected;
            for (size_t i = 0; i < used; ++i)
            {
                Access<Int>::limb(dividend, i) = divisor;
                Access<Int>::limb(expected, i) = 1;
            }
            const Int original = dividend;
            Int quotient = ~Int(0);
            quotient = dividend / divisor;
            EXPECT_EQ(quotient, expected);
            EXPECT_EQ(dividend % divisor, Int(0));
            EXPECT_EQ(dividend, original);
        }
    }
}

TEST(IntegerStorage, SmallDivisionInitializesEveryUsedWidth)
{
    check_small_division_used_limbs<1>();
    check_small_division_used_limbs<2>();
    check_small_division_used_limbs<4>();
    check_small_division_used_limbs<16>();
}

TEST(IntegerStorage, FixedDivisorKernelsPreserveInputsAndInitializeOutputs)
{
    using Int = gint::UInt256;
    const Int dividend = from_words<Int>({{19, 0, 0, 6}});
    for (size_t divisor_limbs = 2; divisor_limbs <= 4; ++divisor_limbs)
    {
        Int divisor;
        Access<Int>::limb(divisor, divisor_limbs - 1) = 3;
        const Int original_divisor = divisor;
        Int quotient;
        if (divisor_limbs == 2)
            quotient = Access<Int>::div_large_2(dividend, divisor);
        else if (divisor_limbs == 3)
            quotient = Access<Int>::div_large_3(dividend, divisor);
        else
            quotient = Access<Int>::div_large_4(dividend, divisor);
        Int expected;
        Access<Int>::limb(expected, 4 - divisor_limbs) = 2;
        EXPECT_EQ(quotient, expected);
        EXPECT_EQ(Access<Int>::rem_large(dividend, divisor, divisor_limbs), Int(19));
        EXPECT_EQ(divisor, original_divisor);
        EXPECT_EQ(dividend, from_words<Int>({{19, 0, 0, 6}}));
    }
}

TEST(IntegerStorage, GeneralDivisionHandlesWideAndShortDividends)
{
    using Int = gint::integer<1024, unsigned>;
    Int dividend(19);
    Access<Int>::limb(dividend, 15) = 6;
    Int divisor;
    Access<Int>::limb(divisor, 14) = 3;
    Int expected;
    Access<Int>::limb(expected, 1) = 2;
    EXPECT_EQ(Access<Int>::div_large(dividend, divisor, 15), expected);
    EXPECT_EQ(Access<Int>::rem_large(dividend, divisor, 15), Int(19));

    Access<Int>::limb(dividend, 15) = 0;
    EXPECT_EQ(Access<Int>::div_large(dividend, divisor, 15), Int(0));
    EXPECT_EQ(Access<Int>::rem_large(dividend, divisor, 15), dividend);
}

TEST(IntegerStorage, InPlaceShiftsPreserveSignedAndUnsignedFill)
{
    using UInt = gint::UInt256;
    using SInt = gint::Int256;
    UInt left = from_words<UInt>({{1, 2, 3, 4}});
    left <<= 65;
    EXPECT_EQ(left, from_words<UInt>({{0, 2, 4, 6}}));

    const std::array<uint64_t, 4> original = {{1, 2, 3, uint64_t(1) << 63}};
    UInt logical = from_words<UInt>(original);
    logical >>= 65;
    EXPECT_EQ(logical, from_words<UInt>({{(uint64_t(1) << 63) | 1, 1, uint64_t(1) << 62, 0}}));

    SInt arithmetic = from_words<SInt>(original);
    arithmetic >>= 65;
    EXPECT_EQ(arithmetic, from_words<SInt>({{(uint64_t(1) << 63) | 1, 1, uint64_t(3) << 62, ~uint64_t(0)}}));
}
} // namespace
