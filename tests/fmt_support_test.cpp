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
