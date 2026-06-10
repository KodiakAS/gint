#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gint/gint.h>
#include <gtest/gtest.h>

#include <array>
#include <utility>
#include <vector>

TEST(fmt_support, format_gint)
{
    gint::integer<256, unsigned> value{1};
    EXPECT_EQ(fmt::format("{}", value), "1");
}

TEST(fmt_support, format_vector_of_gint)
{
    std::vector<gint::integer<128, unsigned>> vec{1, 2, 3};
    EXPECT_EQ(fmt::format("{}", vec), "[1, 2, 3]");
}

TEST(fmt_support, format_pair_of_gint)
{
    std::pair<gint::integer<64, unsigned>, gint::integer<64, unsigned>> p{1, 2};
    EXPECT_EQ(fmt::format("{}", p), "(1, 2)");
}

TEST(fmt_support, format_array_of_gint)
{
    std::array<gint::integer<128, unsigned>, 2> arr{1, 2};
    EXPECT_EQ(fmt::format("{}", arr), "[1, 2]");
}

TEST(fmt_support, format_signed_gint)
{
    gint::integer<128, signed> value{-42};
    EXPECT_EQ(fmt::format("{}", value), "-42");
}

TEST(fmt_support, format_integer_base_specs)
{
    using U128 = gint::integer<128, unsigned>;
    const U128 value = (U128(1) << 68) + U128(0x2a);

    EXPECT_EQ(fmt::format("{:x}", value), "10000000000000002a");
    EXPECT_EQ(fmt::format("{:X}", value), "10000000000000002A");
    EXPECT_EQ(fmt::format("{:o}", U128(71)), "107");
    EXPECT_EQ(fmt::format("{:b}", U128(42)), "101010");
    EXPECT_EQ(fmt::format("{:B}", U128(42)), "101010");
    EXPECT_EQ(fmt::format("{:d}", value), "295147905179352825898");
}

TEST(fmt_support, format_integer_width_and_alternate_specs)
{
    using U128 = gint::integer<128, unsigned>;

    EXPECT_EQ(fmt::format("{:#X}", U128(42)), "0X2A");
    EXPECT_EQ(fmt::format("{:#b}", U128(42)), "0b101010");
    EXPECT_EQ(fmt::format("{:#B}", U128(42)), "0B101010");
    EXPECT_EQ(fmt::format("{:#x}", U128(0)), "0x0");
    EXPECT_EQ(fmt::format("{:#X}", U128(0)), "0X0");
    EXPECT_EQ(fmt::format("{:#b}", U128(0)), "0b0");
    EXPECT_EQ(fmt::format("{:#B}", U128(0)), "0B0");
    EXPECT_EQ(fmt::format("{:#o}", U128(0)), "0");
    EXPECT_EQ(fmt::format("{:08x}", U128(42)), "0000002a");
    EXPECT_EQ(fmt::format("{:#08x}", U128(42)), "0x00002a");
    EXPECT_EQ(fmt::format("{:#010b}", U128(42)), "0b00101010");
    EXPECT_EQ(fmt::format("{:>6d}", U128(42)), "    42");
    EXPECT_EQ(fmt::format("{:<6d}", U128(42)), "42    ");
    EXPECT_EQ(fmt::format("{:^6d}", U128(42)), "  42  ");
#if FMT_VERSION < 120000
    EXPECT_EQ(fmt::format("{:<08d}", U128(42)), "42000000");
    EXPECT_EQ(fmt::format("{:^08d}", U128(42)), "00042000");
    EXPECT_EQ(fmt::format("{:x<08d}", U128(42)), "42000000");
#else
    EXPECT_EQ(fmt::format("{:<08d}", U128(42)), "42      ");
    EXPECT_EQ(fmt::format("{:^08d}", U128(42)), "   42   ");
    EXPECT_EQ(fmt::format("{:x<08d}", U128(42)), "42xxxxxx");
#endif
}

TEST(fmt_support, format_signed_integer_base_specs)
{
    using S128 = gint::integer<128, signed>;
    const S128 value = -42;

    EXPECT_EQ(fmt::format("{:x}", value), "-2a");
    EXPECT_EQ(fmt::format("{:#b}", value), "-0b101010");
    EXPECT_EQ(fmt::format("{:#08x}", value), "-0x0002a");
}
