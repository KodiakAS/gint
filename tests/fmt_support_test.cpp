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
#if FMT_VERSION < 100000
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
    EXPECT_EQ(fmt::format("{:Lx}", U128(42)), "2a");
    EXPECT_EQ(fmt::format("{:#Lx}", U128(42)), "0x2a");
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
    EXPECT_EQ(fmt::format("{:#Lx}", value), "-0x2a");
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
    EXPECT_EQ(fmt::format(locale, "{:Lx}", U128(0x12d687)), "12d,687");
    EXPECT_EQ(fmt::format(locale, "{:#Lx}", U128(0x12d687)), "0x12d,687");
    EXPECT_EQ(fmt::format(locale, "{:Lo}", U128(01234567)), "1,234,567");
    EXPECT_EQ(fmt::format(locale, "{:Lb}", U128(0x12d687)), "100,101,101,011,010,000,111");
    EXPECT_EQ(fmt::format(locale, "{:014L}", U128(1234567)), fmt::format(locale, "{:014L}", 1234567u));
    EXPECT_EQ(fmt::format(locale, "{:014L}", S128(-1234567)), fmt::format(locale, "{:014L}", -1234567));
    EXPECT_EQ(fmt::format(locale, "{:+014L}", S128(1234567)), fmt::format(locale, "{:+014L}", 1234567));
    EXPECT_EQ(fmt::format(locale, "{: 014L}", S128(1234567)), fmt::format(locale, "{: 014L}", 1234567));
    EXPECT_EQ(fmt::format(locale, "{:#014Lx}", S128(0x12d687)), "000000x12d,687");
    EXPECT_EQ(fmt::format(locale, "{:#014Lx}", S128(-0x12d687)), "0000-0x12d,687");
}

TEST(fmt_support, utf8_fill_matches_native)
{
    const char * fills[] = {"*", "é", "界", "😀"};
    const char aligns[] = {'<', '>', '^'};
    for (const char * fill : fills)
        for (char align : aligns)
            for (int value : {42, -42})
                for (int width : {0, 2, 6, 7})
                {
                    const std::string prefix = std::string("{:") + fill + align;
                    const std::string fixed = prefix + std::to_string(width) + "}";
                    const std::string dynamic = prefix + "{}}";
                    EXPECT_EQ(fmt::format(fmt::runtime(fixed), gint::Int128(value)), fmt::format(fmt::runtime(fixed), value));
                    EXPECT_EQ(
                        fmt::format(fmt::runtime(dynamic), gint::Int128(value), width), fmt::format(fmt::runtime(dynamic), value, width));
                }
    EXPECT_EQ(fmt::format(FMT_STRING("{:界>6}"), gint::Int128(42)), "界界界界42");
    EXPECT_EQ(fmt::format(FMT_STRING("{:😀^{}}"), gint::Int128(42), 7), fmt::format(FMT_STRING("{:😀^{}}"), 42, 7));
}

TEST(fmt_support, utf8_fill_zero_flag_and_named_width)
{
    for (const char * spec : {"{:界<06}", "{:界>06}", "{:界^06}"})
        EXPECT_EQ(fmt::format(fmt::runtime(spec), gint::Int128(42)), fmt::format(fmt::runtime(spec), 42));
    EXPECT_EQ(
        fmt::format(FMT_STRING("{:é>{width}}"), gint::Int128(42), fmt::arg("width", 6)),
        fmt::format(FMT_STRING("{:é>{width}}"), 42, fmt::arg("width", 6)));
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:\xf0\x9f"), gint::Int128(42))), fmt::format_error);
}

TEST(fmt_support, localized_nondecimal_contract)
{
    const std::locale locale(std::locale::classic(), new comma_numpunct);
    struct test_case
    {
        const char * spec;
        int value;
        const char * expected;
    };
    const test_case cases[] = {
        {"{:Lx}", 0x1234567, "1,234,567"},
        {"{:#LX}", -0xabcdef, "-0XABC,DEF"},
        {"{:#Lo}", 01234567, "01,234,567"},
        {"{:#LB}", 0x123, "0B100,100,011"},
        {"{:#Lx}", 0, "0x0"},
        {"{:#Lo}", 0, "0"},
        {"{:界>14Lx}", 0x1234567, "界界界界界1,234,567"},
    };
    // fmt < 10.2 either ignores grouping in non-decimal formats or formats
    // them as decimal (fmtlib/fmt#3693, fixed by #3750 in 10.2.0).
    // Keep independent expected strings on every version, including old fmt.
    for (const test_case & item : cases)
    {
        SCOPED_TRACE(item.spec);
        EXPECT_EQ(fmt::format(locale, fmt::runtime(item.spec), gint::Int128(item.value)), item.expected);
#if FMT_VERSION >= 100200
        EXPECT_EQ(fmt::format(locale, fmt::runtime(item.spec), item.value), item.expected);
#endif
    }
}
