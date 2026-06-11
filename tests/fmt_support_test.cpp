#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gint/gint.h>
#include <gtest/gtest.h>

#include <array>
#include <locale>
#include <utility>
#include <vector>

namespace
{
class comma_numpunct : public std::numpunct<char>
{
    char do_thousands_sep() const override { return ','; }

    std::string do_grouping() const override { return "\3"; }
};
}

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
    EXPECT_EQ(fmt::format("{:c}", U128(65)), fmt::format("{:c}", 65));
    EXPECT_EQ(fmt::format("{:c}", U128(321)), fmt::format("{:c}", 321));
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
    EXPECT_EQ(fmt::format("{:>{}}", U128(42), 6), "    42");
    EXPECT_EQ(fmt::format("{:{}x}", U128(42), 6), "    2a");
    EXPECT_EQ(fmt::format("{0:>{1}}", U128(42), 6), "    42");
    EXPECT_EQ(fmt::format("{:0>{}}", U128(42), 6), "000042");
    EXPECT_EQ(fmt::format("{:{w}}", U128(42), fmt::arg("w", 6)), "    42");
    EXPECT_EQ(fmt::format("{:>{w}}", U128(42), fmt::arg("w", 6)), "    42");
    EXPECT_EQ(fmt::format("{:4c}", U128(65)), fmt::format("{:4c}", 65));
    EXPECT_EQ(fmt::format("{:>4c}", U128(65)), fmt::format("{:>4c}", 65));
    EXPECT_EQ(fmt::format("{:^4c}", U128(65)), fmt::format("{:^4c}", 65));
    EXPECT_EQ(fmt::format("{:08c}", U128(65)), fmt::format("{:08c}", 65));
    EXPECT_EQ(fmt::format("{:L}", U128(42)), fmt::format("{:L}", 42u));
    EXPECT_EQ(fmt::format("{:Ld}", U128(42)), fmt::format("{:Ld}", 42u));
    EXPECT_EQ(fmt::format("{:Lx}", U128(42)), fmt::format("{:Lx}", 42u));
    EXPECT_EQ(fmt::format("{:#Lx}", U128(42)), fmt::format("{:#Lx}", 42u));
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:{>8d}"), U128(42))), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:999999999999999999999999999999999999d}"), U128(42))), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:>{999999999999999999999999999999999999}}"), U128(42))), fmt::format_error);
}

TEST(fmt_support, reject_unsigned_integer_sign_specs)
{
    using U128 = gint::integer<128, unsigned>;

    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:+d}"), U128(42))), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{: d}"), U128(42))), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:-d}"), U128(42))), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:+x}"), U128(42))), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:+c}"), U128(42))), fmt::format_error);
}

TEST(fmt_support, format_signed_integer_base_specs)
{
    using S128 = gint::integer<128, signed>;
    const S128 value = -42;

    EXPECT_EQ(fmt::format("{:x}", value), "-2a");
    EXPECT_EQ(fmt::format("{:#b}", value), "-0b101010");
    EXPECT_EQ(fmt::format("{:#08x}", value), "-0x0002a");
    EXPECT_EQ(fmt::format("{:08d}", value), fmt::format("{:08d}", -42));
    EXPECT_EQ(fmt::format("{:>08d}", value), fmt::format("{:>08d}", -42));
    EXPECT_EQ(fmt::format("{:>{}}", value, 6), fmt::format("{:>{}}", -42, 6));
    EXPECT_EQ(fmt::format("{:c}", value), fmt::format("{:c}", -42));
    EXPECT_EQ(fmt::format("{:4c}", value), fmt::format("{:4c}", -42));
    EXPECT_EQ(fmt::format("{:L}", value), fmt::format("{:L}", -42));
    EXPECT_EQ(fmt::format("{:Ld}", value), fmt::format("{:Ld}", -42));
    EXPECT_EQ(fmt::format("{:#Lx}", value), fmt::format("{:#Lx}", -42));
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:=8d}"), value)), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:0=8d}"), value)), fmt::format_error);
}

TEST(fmt_support, format_integer_locale_specs)
{
    using U128 = gint::integer<128, unsigned>;
    using S128 = gint::integer<128, signed>;
    const std::locale locale(std::locale::classic(), new comma_numpunct);

    EXPECT_EQ(fmt::format(locale, "{:L}", U128(1234567)), fmt::format(locale, "{:L}", 1234567u));
    EXPECT_EQ(fmt::format(locale, "{:Ld}", U128(1234567)), fmt::format(locale, "{:Ld}", 1234567u));
    EXPECT_EQ(fmt::format(locale, "{:Lx}", U128(0x12d687)), fmt::format(locale, "{:Lx}", 0x12d687u));
    EXPECT_EQ(fmt::format(locale, "{:#Lx}", U128(0x12d687)), fmt::format(locale, "{:#Lx}", 0x12d687u));
    EXPECT_EQ(fmt::format(locale, "{:Lo}", U128(01234567)), fmt::format(locale, "{:Lo}", 01234567u));
    EXPECT_EQ(fmt::format(locale, "{:Lb}", U128(0x12d687)), fmt::format(locale, "{:Lb}", 0x12d687u));
    EXPECT_EQ(fmt::format(locale, "{:014L}", U128(1234567)), fmt::format(locale, "{:014L}", 1234567u));
    EXPECT_EQ(fmt::format(locale, "{:014L}", S128(-1234567)), fmt::format(locale, "{:014L}", -1234567));
    EXPECT_EQ(fmt::format(locale, "{:+014L}", S128(1234567)), fmt::format(locale, "{:+014L}", 1234567));
    EXPECT_EQ(fmt::format(locale, "{: 014L}", S128(1234567)), fmt::format(locale, "{: 014L}", 1234567));
    EXPECT_EQ(fmt::format(locale, "{:#014Lx}", S128(0x12d687)), fmt::format(locale, "{:#014Lx}", 0x12d687));
    EXPECT_EQ(fmt::format(locale, "{:#014Lx}", S128(-0x12d687)), fmt::format(locale, "{:#014Lx}", -0x12d687));
}
