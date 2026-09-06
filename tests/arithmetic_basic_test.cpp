#include <fmt/format.h>
#include <gint/gint.h>
#include <gtest/gtest.h>

#include <array>
#include <random>

#if __cplusplus >= 201402L
constexpr bool constexpr_add_sub_works()
{
    using U = gint::integer<128, unsigned>;
    U value = (U(1) << 64) - U(1);
    const U one = 1;
    const U sum = value + one;
    const U difference = sum - one;
    value += one;
    value -= one;
    return sum == (U(1) << 64) && difference == ((U(1) << 64) - U(1)) && value == difference;
}

static_assert(constexpr_add_sub_works(), "Add/Sub APIs should remain constexpr in C++14+");
#endif

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

TEST(WideIntegerArithmetic, SignedArithmetic)
{
    gint::integer<128, signed> a = -5;
    gint::integer<128, signed> b = 2;
    auto c = a + b;
    EXPECT_EQ(gint::to_string(c), "-3");
}

TEST(WideIntegerArithmetic, UInt256)
{
    using U = gint::integer<256, unsigned>;
    U a = (U(1) << 200) + (U(1) << 100) + 123;
    U b = (U(1) << 150) + 456;
    U c = a + b;
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(c > b);
}

TEST(WideIntegerArithmetic, UInt512)
{
    using U = gint::integer<512, unsigned>;
    U a = (U(1) << 400) + (U(1) << 200) + 123456789;
    U b = (U(1) << 350) + (U(1) << 100) + 98765;
    U c = a + b;
    EXPECT_TRUE(c > a);
}

// From mul_limb_overflow_test.cpp
TEST(WideIntegerArithmetic, MulLimbOverflowAllOnes)
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

TEST(WideIntegerArithmetic, PrefixPostfixIncrement128)
{
    using U128 = gint::integer<128, unsigned>;
    U128 a = (U128(1) << 64) - U128(1); // 2^64-1
    U128 prev = a++;
    EXPECT_EQ(prev, (U128(1) << 64) - U128(1));
    EXPECT_EQ(a, U128(1) << 64);
    ++a;
    EXPECT_EQ(a, (U128(1) << 64) + U128(1));
}

TEST(WideIntegerArithmetic, PrefixPostfixDecrement256)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = U256(1) << 128; // trigger borrow across multiple limbs
    U256 prev = a--;
    EXPECT_EQ(prev, U256(1) << 128);

    U256 expected = 0;
    expected += U256(0xffffffffffffffffULL);
    expected += U256(0xffffffffffffffffULL) << 64;
    EXPECT_EQ(a, expected);

    --a;
    U256 expected2 = 0;
    expected2 += U256(0xfffffffffffffffeULL);
    expected2 += U256(0xffffffffffffffffULL) << 64;
    EXPECT_EQ(a, expected2);
}

TEST(WideIntegerArithmetic, SubBorrowChain256)
{
    using U256 = gint::integer<256, unsigned>;
    U256 top = U256(1) << 192; // limb[3] = 1
    U256 one = 1;
    U256 diff = top - one; // cascaded borrow across 3 limbs

    U256 expected = 0;
    expected += U256(0xffffffffffffffffULL);
    expected += U256(0xffffffffffffffffULL) << 64;
    expected += U256(0xffffffffffffffffULL) << 128;
    EXPECT_EQ(diff, expected);
}

namespace
{
template <size_t Bits, typename Signed>
gint::integer<Bits, Signed> scalar_test_value(const std::array<uint64_t, Bits / 64> & words)
{
    gint::integer<Bits, Signed> value = 0;
    for (size_t i = 0; i < words.size(); ++i)
        value |= gint::integer<Bits, Signed>(words[i]) << (64 * i);
    return value;
}

template <size_t Bits, typename Signed>
void check_scalar_add_sub()
{
    using Int = gint::integer<Bits, Signed>;
    using Wide = unsigned __int128;
    std::mt19937_64 rng(0xaddu + Bits);
    const uint64_t scalars[] = {0, 1, 2, uint64_t(1) << 63, ~uint64_t(0)};
    for (unsigned sample = 0; sample < 48; ++sample)
    {
        std::array<uint64_t, Bits / 64> words = {{0}};
        for (size_t i = 0; i < words.size(); ++i)
            words[i] = sample == 0 ? 0 : sample == 1 ? ~uint64_t(0) : rng();
        // Exercise every possible carry/borrow-chain length, including full wraparound.
        if (sample >= 2 && sample < 2 + 2 * words.size())
            for (size_t i = 0; i <= (sample - 2) / 2; ++i)
                words[i] = sample % 2 ? ~uint64_t(0) : 0;
        const Int value = scalar_test_value<Bits, Signed>(words);
        for (uint64_t scalar : scalars)
        {
            std::array<uint64_t, Bits / 64> sum = {{0}};
            std::array<uint64_t, Bits / 64> difference = {{0}};
            Wide carry = scalar;
            Wide borrow = scalar;
            for (size_t i = 0; i < words.size(); ++i)
            {
                const Wide added = Wide(words[i]) + carry;
                sum[i] = static_cast<uint64_t>(added);
                carry = added >> 64;
                // The extra high bit makes subtraction non-negative even for UINT64_MAX.
                const Wide subtracted = (Wide(1) << 64) + words[i] - borrow;
                difference[i] = static_cast<uint64_t>(subtracted);
                borrow = 1 - (subtracted >> 64);
            }
            const Int expected_sum = scalar_test_value<Bits, Signed>(sum);
            const Int expected_difference = scalar_test_value<Bits, Signed>(difference);
            EXPECT_EQ(value + scalar, expected_sum);
            EXPECT_EQ(scalar + value, expected_sum);
            EXPECT_EQ(value - scalar, expected_difference);
            Int assigned = value;
            assigned += scalar;
            EXPECT_EQ(assigned, expected_sum);
            assigned = value;
            assigned -= scalar;
            EXPECT_EQ(assigned, expected_difference);
        }
    }
}
} // namespace

TEST(WideIntegerArithmetic, U64ScalarCarryBorrowAllWidths)
{
    check_scalar_add_sub<64, signed>();
    check_scalar_add_sub<64, unsigned>();
    check_scalar_add_sub<128, signed>();
    check_scalar_add_sub<128, unsigned>();
    check_scalar_add_sub<256, signed>();
    check_scalar_add_sub<256, unsigned>();
    check_scalar_add_sub<512, signed>();
    check_scalar_add_sub<512, unsigned>();
    check_scalar_add_sub<1024, signed>();
    check_scalar_add_sub<1024, unsigned>();
}

#if __cplusplus >= 201402L
constexpr bool constexpr_u64_scalar_add_sub_works()
{
    using U = gint::UInt256;
    using I = gint::Int256;
    const uint64_t one = 1;
    const U all = ~U(0);
    return all + one == U(0) && one + all == U(0) && U(0) - one == all && I(-1) + one == I(0) && one + I(-1) == I(0) && I(0) - one == I(-1)
        && (U(1) << 192) - one == (all >> 64);
}
static_assert(constexpr_u64_scalar_add_sub_works(), "U64 scalar carry/borrow must remain constexpr");
#endif
