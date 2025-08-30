#include <fmt/format.h>
#include <gint/gint.h>
#include <gtest/gtest.h>

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
    return fmt::format("{}_{}_{}", info.param.a, info.param.b, op);
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

TEST(WideIntegerAdditional, BitwiseNot)
{
    gint::integer<128, unsigned> a = 0;
    auto b = ~a;
    EXPECT_EQ(gint::to_string(b), "340282366920938463463374607431768211455");
}

TEST(WideInteger256, BitwiseAndShift)
{
    using U = gint::integer<256, unsigned>;
    U v = 1;
    U s = v << 128;
    EXPECT_EQ((s >> 128), v);
    EXPECT_EQ((s & v), U(0));
}

TEST(WideInteger512, BitwiseOps)
{
    using U = gint::integer<512, unsigned>;
    U a = U(1) << 511;
    U b = U(1);
    EXPECT_EQ((a | b) & a, a);
}
