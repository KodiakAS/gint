#include <fmt/format.h>
#include <gint/gint.h>
#include <gtest/gtest.h>

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
