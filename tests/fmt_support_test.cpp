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
public:
    explicit comma_numpunct(std::string grouping = "\3")
        : grouping_(std::move(grouping))
    {
    }

private:
    char do_thousands_sep() const override { return ','; }

    std::string do_grouping() const override { return grouping_; }

    std::string grouping_;
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

TEST(fmt_support, repeated_zero_flag_matches_native)
{
    for (const char * spec : {"{:00d}", "{:005d}", "{:>005d}"})
    {
#if FMT_VERSION >= 100000
        EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime(spec), 42)), fmt::format_error);
        EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime(spec), gint::Int128(42))), fmt::format_error);
#else
        EXPECT_EQ(fmt::format(fmt::runtime(spec), gint::Int128(42)), fmt::format(fmt::runtime(spec), 42));
#endif
    }
}

#if FMT_VERSION >= 90100
TEST(fmt_support, delegates_positional_width_type_checks_to_fmt)
{
    struct parse_context : fmt::format_parse_context
    {
        explicit parse_context(const char * spec)
            : fmt::format_parse_context(spec)
        {
        }
        void check_dynamic_spec(int id) { checked_id = id; }
        int checked_id = -1;
    };

    parse_context automatic("{}}");
    automatic.next_arg_id();
    fmt::formatter<gint::Int128>().parse(automatic);
    EXPECT_EQ(automatic.checked_id, 1);

    parse_context indexed("{2}}");
    fmt::formatter<gint::Int128>().parse(indexed);
    EXPECT_EQ(indexed.checked_id, 2);
}
#endif

TEST(fmt_support, dynamic_width_matches_integer_grammar)
{
    const gint::Int128 value = 42;
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{0:{01}}"), value, 6)), fmt::format_error);
    EXPECT_EQ(fmt::format(fmt::runtime("{1:{0}}"), 6, value), "    42");
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:{}}"), value, 1.5)), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:4{}}"), value, 6)), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{0:4{1}}"), value, 6)), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:4{w}}"), value, fmt::arg("w", 6))), fmt::format_error);
}

#if FMT_USE_INT128
TEST(fmt_support, dynamic_width_accepts_native_int128)
{
    const gint::Int128 value = 42;
    const __int128 width = 6;
    const unsigned too_wide = static_cast<unsigned>((std::numeric_limits<int>::max)()) + 1;
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:{}}"), value, too_wide)), fmt::format_error);
    const unsigned __int128 unsigned_width = 6;
    EXPECT_EQ(fmt::format(fmt::runtime("{:{}}"), value, width), fmt::format(fmt::runtime("{:{}}"), 42, width));
    EXPECT_EQ(fmt::format(fmt::runtime("{:{}}"), value, unsigned_width), fmt::format(fmt::runtime("{:{}}"), 42, unsigned_width));
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:{}}"), value, -width)), fmt::format_error);
    EXPECT_THROW(static_cast<void>(fmt::format(fmt::runtime("{:{}}"), value, unsigned_width << 80)), fmt::format_error);
}
#endif

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

TEST(fmt_support, format_signed_minimum_octal)
{
    using S1024 = gint::integer<1024, signed>;
    EXPECT_EQ(fmt::format("{:o}", std::numeric_limits<S1024>::min()), "-1" + std::string(341, '0'));
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

#if FMT_VERSION >= 100000 && (!defined(FMT_USE_LOCALE) || FMT_USE_LOCALE)
TEST(fmt_support, explicit_format_facet)
{
    const std::locale comma(std::locale::classic(), new comma_numpunct);
    const std::locale locale(comma, new fmt::format_facet<std::locale>("_"));
    const gint::Int256 negative(-1234567);
    EXPECT_EQ(fmt::format(locale, "{:L}", negative), "-1_234_567");
    EXPECT_EQ(fmt::format(locale, "{} {:界^{}L}", 1, negative, 15), fmt::format(locale, "{} {:界^{}L}", 1, -1234567, 15));
    EXPECT_EQ(fmt::format(locale, "{:+0{w}Ld}", -negative, fmt::arg("w", 15)), fmt::format(locale, "{:+015Ld}", 1234567));
    EXPECT_EQ(fmt::format(locale, "{0:{1}L}", negative, 15), fmt::format(locale, "{:15L}", -1234567));
#    if FMT_VERSION >= 100200
    EXPECT_EQ(fmt::format(locale, "{:#014LX}", negative), fmt::format(locale, "{:#014LX}", -1234567));
#    else
    EXPECT_EQ(fmt::format(locale, "{:#LX}", negative), "-0X12D,687");
#    endif
#    if FMT_USE_INT128
    using Native = unsigned __int128;
    using NativeSigned = __int128;
#    else
    using Native = unsigned long long;
    using NativeSigned = long long;
#    endif
    const Native max_native = ~Native(0);
    const gint::Int256 max_unsigned(max_native);
    const NativeSigned native_min = -static_cast<NativeSigned>(max_native / 2) - 1;
    const gint::Int256 min_signed(native_min);
    EXPECT_EQ(fmt::format(locale, "{:L}", gint::UInt256(max_native)), fmt::format(locale, "{:L}", max_native));
    EXPECT_EQ(fmt::format(locale, "{:+L}", max_unsigned), "+" + fmt::format(locale, "{:L}", max_native));
    EXPECT_EQ(fmt::format(locale, "{:L}", min_signed), fmt::format(locale, "{:L}", native_min));
    EXPECT_EQ(fmt::format(locale, "{:L}", max_unsigned + 1), fmt::format(comma, "{:L}", max_unsigned + 1));
    EXPECT_EQ(fmt::format(locale, "{:L}", min_signed - 1), fmt::format(comma, "{:L}", min_signed - 1));
}
#endif

TEST(fmt_support, locale_grouping_terminators)
{
    using U1024 = gint::integer<1024, unsigned>;
    using S1024 = gint::integer<1024, signed>;
    const std::string decimal = "1" + std::string(300, '0');
    const U1024 value(decimal);
    const std::string grouped = "1" + std::string(297, '0') + ",000";
    const char terminators[] = {char(0), (std::numeric_limits<char>::max)(), char(-1)};
    for (char terminator : terminators)
    {
        SCOPED_TRACE(static_cast<int>(terminator));
        const std::locale stop_first(std::locale::classic(), new comma_numpunct(std::string(1, terminator)));
        const std::locale stop_after_three(std::locale::classic(), new comma_numpunct(std::string("\3") + terminator));
        EXPECT_EQ(fmt::format(stop_first, "{:L}", value), decimal);
        EXPECT_EQ(fmt::format(stop_after_three, "{:L}", value), grouped);
        EXPECT_EQ(fmt::format(stop_after_three, "{:L}", -S1024(value)), "-" + grouped);
    }
}

TEST(fmt_support, locale_grouping_preserves_char_signedness)
{
    using U1024 = gint::integer<1024, unsigned>;
    const U1024 value("1" + std::string(300, '0'));
    const std::locale locale(std::locale::classic(), new comma_numpunct(std::string("\3\200", 2)));
    // 0x80 stops grouping with signed char, but is a positive group size with unsigned char.
    const std::string expected = std::numeric_limits<char>::is_signed
        ? "1" + std::string(297, '0') + ",000"
        : "1" + std::string(41, '0') + "," + std::string(128, '0') + "," + std::string(128, '0') + ",000";
    EXPECT_EQ(fmt::format(locale, "{:L}", value), expected);
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
