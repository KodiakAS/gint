#include <gint/gint.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace
{
constexpr uint64_t equal_words[] = {7, 11};
constexpr uint64_t different_words[] = {7, 12};
static_assert(gint::detail::equal_limbs(equal_words, equal_words), "raw equality must remain C++11 constexpr");
static_assert(!gint::detail::equal_limbs(equal_words, different_words), "raw equality must inspect every limb");

TEST(LimbOps, SmallDivisionOverwritesTheWholeOutput)
{
    using Division = gint::detail::limb_division<4>;
    const uint64_t dividend[4] = {17, 3, 1, 0};
    uint64_t quotient[4];
    std::fill_n(quotient, 4, ~uint64_t(0));
    EXPECT_EQ(Division::div_mod_small(dividend, 1, quotient), 0u);
    EXPECT_TRUE(std::equal(quotient, quotient + 4, dividend));

    std::fill_n(quotient, 4, ~uint64_t(0));
    EXPECT_EQ(Division::div_mod_small(dividend, 64, quotient), 17u);
    const uint64_t expected[4] = {uint64_t(3) << 58, uint64_t(1) << 58, 0, 0};
    EXPECT_TRUE(std::equal(quotient, quotient + 4, expected));

    const uint64_t zero[4] = {0, 0, 0, 0};
    std::fill_n(quotient, 4, ~uint64_t(0));
    EXPECT_EQ(Division::div_mod_small(zero, 7, quotient), 0u);
    EXPECT_TRUE(std::equal(quotient, quotient + 4, zero));
}

template <size_t Limbs>
void check_small_division_used_limbs()
{
    using Division = gint::detail::limb_division<Limbs>;
    const uint64_t divisors[] = {3, 0x10000000003ULL, 0x8000000000000001ULL};
    for (uint64_t divisor : divisors)
    {
        for (size_t used = 0; used <= Limbs; ++used)
        {
            uint64_t dividend[Limbs] = {};
            uint64_t expected[Limbs] = {};
            for (size_t i = 0; i < used; ++i)
            {
                dividend[i] = divisor;
                expected[i] = 1;
            }
            uint64_t quotient[Limbs];
            std::fill_n(quotient, Limbs, ~uint64_t(0));
            EXPECT_EQ(Division::div_mod_small(dividend, divisor, quotient), 0u);
            for (size_t i = 0; i < Limbs; ++i)
            {
                EXPECT_EQ(quotient[i], expected[i]);
                EXPECT_EQ(dividend[i], expected[i] * divisor);
            }
        }
    }
}

TEST(LimbOps, SmallDivisionInitializesEveryUsedWidth)
{
    check_small_division_used_limbs<1>();
    check_small_division_used_limbs<2>();
    check_small_division_used_limbs<4>();
    check_small_division_used_limbs<16>();
}

TEST(LimbOps, FixedDivisorKernelsPreserveInputsAndInitializeOutputs)
{
    using Division = gint::detail::limb_division<4>;
    for (size_t divisor_limbs = 2; divisor_limbs <= 4; ++divisor_limbs)
    {
        std::array<uint64_t, 4> dividend = {{19, 0, 0, 6}};
        std::array<uint64_t, 4> divisor = {{0, 0, 0, 0}};
        divisor[divisor_limbs - 1] = 3;
        const auto original_dividend = dividend;
        const auto original_divisor = divisor;
        std::array<uint64_t, 4> quotient;
        quotient.fill(~uint64_t(0));
        if (divisor_limbs == 2)
            Division::div_large_2(dividend.data(), divisor.data(), quotient.data());
        else if (divisor_limbs == 3)
            Division::div_large_3(dividend.data(), divisor.data(), quotient.data());
        else
            Division::div_large_4(dividend.data(), divisor.data(), quotient.data());
        std::array<uint64_t, 4> expected = {{0, 0, 0, 0}};
        expected[4 - divisor_limbs] = 2;
        EXPECT_EQ(quotient, expected);

        std::array<uint64_t, 4> remainder;
        remainder.fill(~uint64_t(0));
        Division::rem_large(dividend.data(), divisor.data(), divisor_limbs, remainder.data());
        expected.fill(0);
        expected[0] = 19;
        EXPECT_EQ(remainder, expected);
        if (divisor_limbs == 4)
        {
            remainder.fill(~uint64_t(0));
            Division::rem_large_4(dividend.data(), divisor.data(), remainder.data());
            EXPECT_EQ(remainder, expected);
        }
        EXPECT_EQ(dividend, original_dividend);
        EXPECT_EQ(divisor, original_divisor);
    }
}

TEST(LimbOps, GeneralDivisionHandlesWideAndShortDividends)
{
    using Division = gint::detail::limb_division<16>;
    std::array<uint64_t, 16> dividend = {{19}};
    dividend[15] = 6;
    std::array<uint64_t, 16> divisor = {{0}};
    divisor[14] = 3;
    std::array<uint64_t, 16> result;
    result.fill(~uint64_t(0));
    Division::div_large(dividend.data(), divisor.data(), 15, result.data());
    std::array<uint64_t, 16> expected = {{0, 2}};
    EXPECT_EQ(result, expected);
    result.fill(~uint64_t(0));
    Division::rem_large(dividend.data(), divisor.data(), 15, result.data());
    expected.fill(0);
    expected[0] = 19;
    EXPECT_EQ(result, expected);

    dividend[15] = 0;
    result.fill(~uint64_t(0));
    Division::div_large(dividend.data(), divisor.data(), 15, result.data());
    expected.fill(0);
    EXPECT_EQ(result, expected);
    result.fill(~uint64_t(0));
    Division::rem_large(dividend.data(), divisor.data(), 15, result.data());
    EXPECT_EQ(result, dividend);
}

TEST(LimbOps, InPlaceShiftsUseTheSuppliedFill)
{
    using Shift = gint::detail::limb_shift<4>;
    uint64_t value[4] = {1, 2, 3, 4};
    Shift::shift_left_assign(value, 1, 1);
    const std::array<uint64_t, 4> left = {{0, 2, 4, 6}};
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(value[i], left[i]);

    const std::array<uint64_t, 4> original = {{1, 2, 3, uint64_t(1) << 63}};
    for (size_t i = 0; i < 4; ++i)
        value[i] = original[i];
    Shift::shift_right_assign(value, 1, 1, 0);
    const std::array<uint64_t, 4> logical = {{(uint64_t(1) << 63) | 1, 1, uint64_t(1) << 62, 0}};
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(value[i], logical[i]);

    for (size_t i = 0; i < 4; ++i)
        value[i] = original[i];
    Shift::shift_right_assign(value, 1, 1, ~uint64_t(0));
    const std::array<uint64_t, 4> arithmetic = {{(uint64_t(1) << 63) | 1, 1, uint64_t(3) << 62, ~uint64_t(0)}};
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(value[i], arithmetic[i]);
}
} // namespace
