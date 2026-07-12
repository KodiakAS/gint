#include <array>
#include <cfenv>
#include <cmath>
#include <limits>
#include <sstream>
#include <type_traits>
#include <gint/gint.h>
#include <gtest/gtest.h>

// Compile-time convertibility checks (moved from removed compile_test.cpp)
static_assert(std::is_convertible<int, gint::Int256>::value, "int should convert to Int256 implicitly");
static_assert(!std::is_convertible<gint::Int256, int>::value, "Int256 should not implicitly convert to int");
static_assert(std::is_constructible<gint::Int128, gint::UInt128>::value, "UInt128 should explicitly construct Int128");
static_assert(std::is_constructible<gint::UInt256, gint::Int128>::value, "Int128 should explicitly construct UInt256");
static_assert(!std::is_convertible<gint::UInt128, gint::Int128>::value, "integer signedness conversion should stay explicit");
static_assert(std::is_assignable<gint::Int256 &, gint::UInt128>::value, "UInt128 should assign to Int256 explicitly");
static_assert(std::is_constructible<gint::UInt128, const char *>::value, "UInt128 should be constructible from string literals");
static_assert(std::is_constructible<gint::UInt128, std::string>::value, "UInt128 should be constructible from std::string");
static_assert(!std::is_convertible<const char *, gint::UInt128>::value, "string construction should be explicit");

namespace
{
char digit_character(unsigned digit)
{
    return static_cast<char>(digit < 10 ? '0' + digit : 'a' + (digit - 10));
}

template <typename Int>
Int parse_string_digit_by_digit(const std::string & text, unsigned base)
{
    Int result;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const unsigned digit = text[i] <= '9' ? static_cast<unsigned>(text[i] - '0') : static_cast<unsigned>(text[i] - 'a') + 10;
        result = result * static_cast<uint64_t>(base);
        result += Int(digit);
    }
    return result;
}
}

#if __cplusplus >= 201402L
constexpr bool constexpr_integer_conversion_works()
{
    constexpr gint::UInt128 one = 1;
    constexpr gint::Int256 widened(one);
    constexpr gint::Int128 minus_one = -1;
    constexpr gint::UInt256 sign_extended(minus_one);
    return widened == gint::Int256(1) && sign_extended == std::numeric_limits<gint::UInt256>::max();
}
static_assert(constexpr_integer_conversion_works(), "integer conversion constructors should be constexpr in C++14+");
#endif

template <size_t SmallBits, size_t LargeBits>
static void test_integer_conversion_width_pair()
{
    using USmall = gint::integer<SmallBits, unsigned>;
    using SSmall = gint::integer<SmallBits, signed>;
    using ULarge = gint::integer<LargeBits, unsigned>;
    using SLarge = gint::integer<LargeBits, signed>;

    const SSmall minus_one = SSmall(-1);
    const SLarge sign_extended_signed = static_cast<SLarge>(minus_one);
    const ULarge sign_extended_unsigned = static_cast<ULarge>(minus_one);
    EXPECT_EQ(sign_extended_signed, SLarge(-1));
    EXPECT_EQ(sign_extended_unsigned, std::numeric_limits<ULarge>::max());

    const USmall small_max = std::numeric_limits<USmall>::max();
    const ULarge zero_extended = static_cast<ULarge>(small_max);
    EXPECT_EQ(gint::to_string(zero_extended), gint::to_string(small_max));

    ULarge high_unsigned = static_cast<ULarge>(USmall(77));
    high_unsigned += ULarge(1) << SmallBits;
    EXPECT_EQ(static_cast<USmall>(high_unsigned), USmall(77));

    SLarge high_signed = SLarge(1);
    high_signed <<= SmallBits;
    high_signed -= SLarge(123);
    EXPECT_EQ(static_cast<SSmall>(high_signed), SSmall(-123));
}

TEST(WideIntegerConversion, Int128NegativeConversion)
{
    using gint::Int256;
    __int128 small = -5;
    Int256 a = small;
    EXPECT_EQ(a, Int256(-5));

    Int256 b;
    b = small;
    EXPECT_EQ(b, Int256(-5));

    __int128 big = -((static_cast<__int128>(1) << 100));
    Int256 c = big;
    EXPECT_EQ(c, -(Int256(1) << 100));

    Int256 d;
    d = big;
    EXPECT_EQ(d, -(Int256(1) << 100));
}

TEST(WideIntegerConversion, BuiltinToWide)
{
    int a = -42;
    gint::integer<128, signed> b = a; // implicit from int
    EXPECT_EQ(gint::to_string(b), "-42");

    unsigned long long c = 42;
    gint::integer<128, unsigned> d = c; // implicit from unsigned long long
    EXPECT_EQ(gint::to_string(d), "42");
}

TEST(WideIntegerConversion, WideToBuiltin)
{
    gint::integer<128, unsigned> a = 100;
    auto b = static_cast<unsigned int>(a); // explicit conversion to builtin
    EXPECT_EQ(b, 100u);

    gint::integer<128, signed> c = -100;
    auto d = static_cast<int>(c);
    EXPECT_EQ(d, -100);
}

TEST(WideIntegerConversion, MixedArithmetic)
{
    gint::integer<128, unsigned> a = 100;
    unsigned int b = 50;
    auto c = a + b; // builtin implicitly converted to wide integer
    EXPECT_EQ(gint::to_string(c), "150");
}

TEST(WideIntegerConversion, Conditional)
{
    const bool cond = true;
    const gint::Int256 value = 2;
    gint::Int256 result = cond ? 1 : value;
    EXPECT_EQ(result, gint::Int256(1));
}

TEST(WideIntegerConversion, ConditionalConstLvalue)
{
    const bool cond = true;
    const std::array<const gint::Int256, 1> arr = {gint::Int256(2)};
    const gint::Int256 & ref = arr[0];
    gint::Int256 result = cond ? 1 : ref;
    EXPECT_EQ(result, gint::Int256(1));
}

TEST(WideIntegerConversion, SmallIntegral)
{
    int8_t i8 = -7;
    gint::integer<128, signed> a = i8;
    EXPECT_EQ(static_cast<int8_t>(a), i8);
    uint16_t u16 = 60000;
    gint::integer<128, unsigned> b = u16;
    EXPECT_EQ(static_cast<uint16_t>(b), u16);
}

TEST(WideIntegerConversion, FloatingPoint)
{
    gint::integer<128, unsigned> v = 123;
    double d = static_cast<double>(v);
    EXPECT_NEAR(d, 123.0, 1e-9);
    float f = static_cast<float>(v);
    EXPECT_NEAR(f, 123.0f, 1e-6f);

    float f2 = 456.0f;
    gint::integer<128, unsigned> w = f2;
    EXPECT_EQ(w, 456U);
    double d2 = -789.0;
    gint::integer<128, signed> s = d2;
    EXPECT_EQ(s, -789);
}

TEST(WideIntegerConversion, DoubleConversionAvoidsLongDoubleDoubleRounding)
{
    using U128 = gint::integer<128, unsigned>;
    const unsigned __int128 native = (static_cast<unsigned __int128>(1) << 64) + (1ULL << 11) + 1;
    U128 wide = U128(1);
    wide <<= 64;
    wide += U128(1ULL << 11);
    wide += U128(1);

    EXPECT_EQ(static_cast<double>(wide), static_cast<double>(native));

    using S128 = gint::integer<128, signed>;
    S128 signed_wide = S128(1);
    signed_wide <<= 64;
    signed_wide += S128(1ULL << 11);
    signed_wide += S128(1);
    signed_wide = -signed_wide;
    const __int128 signed_native = -static_cast<__int128>(native);
    EXPECT_EQ(static_cast<double>(signed_wide), static_cast<double>(signed_native));
}

TEST(WideIntegerConversion, DoubleConversionRespectsCurrentRoundingMode)
{
    struct RoundingGuard
    {
        int old_round;
        RoundingGuard()
            : old_round(std::fegetround())
        {
        }
        ~RoundingGuard() { std::fesetround(old_round); }
    } guard;

    using U128 = gint::integer<128, unsigned>;
    using S128 = gint::integer<128, signed>;
    const double base = std::ldexp(1.0, 53);
    U128 positive = U128(1);
    positive <<= 53;
    positive += U128(3);
    S128 negative = S128(1);
    negative <<= 53;
    negative += S128(3);
    negative = -negative;

    ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);
    EXPECT_EQ(static_cast<double>(positive), base + 2.0);
    EXPECT_EQ(static_cast<double>(negative), -(base + 4.0));

    ASSERT_EQ(std::fesetround(FE_UPWARD), 0);
    EXPECT_EQ(static_cast<double>(positive), base + 4.0);
    EXPECT_EQ(static_cast<double>(negative), -(base + 2.0));

    ASSERT_EQ(std::fesetround(FE_TOWARDZERO), 0);
    EXPECT_EQ(static_cast<double>(positive), base + 2.0);
    EXPECT_EQ(static_cast<double>(negative), -(base + 2.0));

    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
    EXPECT_EQ(static_cast<double>(positive), base + 4.0);
    EXPECT_EQ(static_cast<double>(negative), -(base + 4.0));
}

TEST(WideIntegerConversion, FloatConversionAvoidsLongDoubleDoubleRounding)
{
    using U128 = gint::integer<128, unsigned>;
    const unsigned __int128 native = (static_cast<unsigned __int128>(1) << 64) + (static_cast<unsigned __int128>(1) << 40) + 1;
    U128 wide = U128(1);
    wide <<= 64;
    wide += U128(1) << 40;
    wide += U128(1);

    EXPECT_EQ(static_cast<float>(wide), static_cast<float>(native));

    using S128 = gint::integer<128, signed>;
    S128 signed_wide = S128(1);
    signed_wide <<= 64;
    signed_wide += S128(1) << 40;
    signed_wide += S128(1);
    signed_wide = -signed_wide;
    const __int128 signed_native = -static_cast<__int128>(native);
    EXPECT_EQ(static_cast<float>(signed_wide), static_cast<float>(signed_native));
}

TEST(WideIntegerConversion, FloatConversionSignedMinMagnitude)
{
    using S128 = gint::integer<128, signed>;
    const S128 min128 = std::numeric_limits<S128>::min();
    EXPECT_EQ(static_cast<double>(min128), -std::ldexp(1.0, 127));
    EXPECT_EQ(static_cast<float>(min128), -std::ldexp(1.0f, 127));
}

TEST(WideIntegerConversion, FloatConversionOverflowRespectsAllRoundingModes)
{
    struct RoundingGuard
    {
        int old_round;
        RoundingGuard()
            : old_round(std::fegetround())
        {
        }
        ~RoundingGuard() { std::fesetround(old_round); }
    } guard;

    using U256 = gint::integer<256, unsigned>;
    using S256 = gint::integer<256, signed>;
    const U256 magnitude = U256(1) << 200;
    const S256 negative = -S256(magnitude);
    const float maximum = std::numeric_limits<float>::max();
    const float infinity = std::numeric_limits<float>::infinity();

    struct Expected
    {
        int mode;
        float positive;
        float negative;
    };
    const Expected cases[] = {
        {FE_TONEAREST, infinity, -infinity},
        {FE_UPWARD, infinity, -maximum},
        {FE_DOWNWARD, maximum, -infinity},
        {FE_TOWARDZERO, maximum, -maximum},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        SCOPED_TRACE(testing::Message() << "rounding mode=" << cases[i].mode);
        ASSERT_EQ(std::fesetround(cases[i].mode), 0);
        EXPECT_EQ(static_cast<float>(magnitude), cases[i].positive);
        EXPECT_EQ(static_cast<float>(negative), cases[i].negative);
    }
}

TEST(WideIntegerConversion, FloatCtorAndAssignLongDouble)
{
    long double ld = 1234.75L;
    gint::integer<128, unsigned> a = ld; // ctor from floating
    EXPECT_EQ(gint::to_string(a), "1234");

    gint::integer<128, unsigned> b = 0;
    b = 56.9L; // assignment from floating
    EXPECT_EQ(gint::to_string(b), "56");
}

TEST(WideIntegerConversion, LongDoubleConversion256)
{
    gint::integer<256, signed> z = 0;
    EXPECT_EQ(static_cast<long double>(z), 0.0L);

    gint::integer<256, signed> n = -123;
    EXPECT_EQ(static_cast<long double>(n), -123.0L);
}

TEST(WideIntegerConversion, LongDoubleConversionRoundsOnce)
{
    struct RoundingGuard
    {
        int old_round;
        RoundingGuard()
            : old_round(std::fegetround())
        {
        }
        ~RoundingGuard() { std::fesetround(old_round); }
    } guard;

    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
    const int digits = std::numeric_limits<long double>::digits;
    ASSERT_TRUE(digits == 53 || digits == 64 || digits == 113);

    using U256 = gint::integer<256, unsigned>;
    using S256 = gint::integer<256, signed>;
    const U256 value = (U256(1) << (digits + 127)) + (U256(1) << 127) + U256(1);
    const long double significand = std::ldexp(1.0L, digits - 1) + 1.0L;
    const long double expected = std::ldexp(significand, 128);

    EXPECT_EQ(static_cast<long double>(value), expected);
    EXPECT_EQ(static_cast<long double>(-S256(value)), -expected);
}

TEST(WideIntegerConversion, LongDoubleConversionRespectsAllRoundingModesAroundTie)
{
    struct RoundingGuard
    {
        int old_round;
        RoundingGuard()
            : old_round(std::fegetround())
        {
        }
        ~RoundingGuard() { std::fesetround(old_round); }
    } guard;

    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
    const int digits = std::numeric_limits<long double>::digits;
    ASSERT_TRUE(digits == 53 || digits == 64 || digits == 113);

    using S256 = gint::integer<256, signed>;
    const S256 base = S256(1) << (digits + 1);
    const S256 positive[] = {base + S256(1), base + S256(2), base + S256(3)};
    const S256 negative[] = {-positive[0], -positive[1], -positive[2]};
    const long double lower = std::ldexp(1.0L, digits + 1);
    const long double upper = std::nextafter(lower, std::numeric_limits<long double>::infinity());
    ASSERT_EQ(upper - lower, 4.0L);

    const int modes[] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
    for (size_t mode_index = 0; mode_index < sizeof(modes) / sizeof(modes[0]); ++mode_index)
    {
        const int mode = modes[mode_index];
        ASSERT_EQ(std::fesetround(mode), 0);
        for (size_t offset_index = 0; offset_index < 3; ++offset_index)
        {
            SCOPED_TRACE(testing::Message() << "rounding mode=" << mode << ", tie offset=" << offset_index);
            const long double positive_expected = mode == FE_UPWARD ? upper : (mode == FE_TONEAREST && offset_index == 2) ? upper : lower;
            const long double negative_expected = mode == FE_DOWNWARD ? -upper
                : (mode == FE_TONEAREST && offset_index == 2)         ? -upper
                                                                      : -lower;
            EXPECT_EQ(static_cast<long double>(positive[offset_index]), positive_expected);
            EXPECT_EQ(static_cast<long double>(negative[offset_index]), negative_expected);
        }
    }
}

TEST(WideIntegerConversion, ToStringZero)
{
    EXPECT_EQ(gint::to_string(gint::integer<128, unsigned>(0)), "0");
    EXPECT_EQ(gint::to_string(gint::integer<128, signed>(0)), "0");
}

TEST(WideIntegerConversion, FromStringDecimal)
{
    const auto max_u128 = gint::from_string<gint::UInt128>("340282366920938463463374607431768211455");
    EXPECT_EQ(max_u128, std::numeric_limits<gint::UInt128>::max());

    const auto min_s128 = gint::from_string<gint::Int128>("-170141183460469231731687303715884105728");
    EXPECT_EQ(min_s128, std::numeric_limits<gint::Int128>::min());
    EXPECT_EQ(gint::to_string(min_s128), "-170141183460469231731687303715884105728");
}

TEST(WideIntegerConversion, FromStringPrefixesAndBases)
{
    EXPECT_EQ((gint::from_string<128, unsigned>("0xffffffffffffffffffffffffffffffff")), std::numeric_limits<gint::UInt128>::max());
    EXPECT_EQ(gint::from_string<gint::UInt128>("0b101010"), gint::UInt128(42));
    EXPECT_EQ(gint::from_string<gint::UInt128>("0107"), gint::UInt128(71));
    EXPECT_EQ(gint::from_string<gint::UInt128>("ff", 16), gint::UInt128(255));
    EXPECT_EQ(gint::from_string<gint::UInt128>("0xff", 16), gint::UInt128(255));
}

TEST(WideIntegerConversion, FromStringWrapsToFixedWidth)
{
    EXPECT_EQ(gint::from_string<gint::UInt128>("340282366920938463463374607431768211456"), gint::UInt128(0));
    EXPECT_EQ(gint::from_string<gint::UInt128>("-1"), std::numeric_limits<gint::UInt128>::max());
}

TEST(WideIntegerConversion, FromStringChunkedMatchesDigitByDigitReference)
{
    const unsigned bases[] = {2, 3, 8, 10, 16, 36};
    for (size_t base_index = 0; base_index < sizeof(bases) / sizeof(bases[0]); ++base_index)
    {
        const unsigned base = bases[base_index];
        std::string text(317, '0');
        for (size_t i = 0; i < text.size(); ++i)
            text[i] = digit_character(static_cast<unsigned>((i * 17 + 5) % base));
        text[0] = '1';

        EXPECT_EQ(gint::from_string<gint::UInt256>(text, base), parse_string_digit_by_digit<gint::UInt256>(text, base));
        EXPECT_EQ(
            (gint::from_string<1024, unsigned>(text, base)), (parse_string_digit_by_digit<gint::integer<1024, unsigned>>(text, base)));
    }

    const std::string negative = "-12345678901234567890123456789012345678901234567890";
    using Int1024 = gint::integer<1024, signed>;
    EXPECT_EQ((gint::from_string<Int1024>(negative, 10)), -parse_string_digit_by_digit<Int1024>(negative.substr(1), 10));
}

TEST(WideIntegerConversion, FromStringStringAndCStrPathsMatch)
{
    using U1024 = gint::integer<1024, unsigned>;
    struct Case
    {
        const char * text;
        unsigned base;
    };
    const Case cases[] = {
        {"+0b101010", 0},
        {"0B1111000011110000", 0},
        {"0777777777777777777777", 0},
        {"0xFEDCBA9876543210", 0},
        {"0Xabcdef0123456789", 16},
        {"1010101010101010", 2},
        {"7654321076543210", 8},
        {"12345678901234567890", 10},
        {"deadBEEF01234567", 16},
        {"zyxwvutsrqponmlkjihgfedcba9876543210", 36},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        const std::string text(cases[i].text);
        EXPECT_EQ(gint::from_string<U1024>(text, cases[i].base), gint::from_string<U1024>(text.c_str(), cases[i].base));
    }

    const std::string negative = "-0x123456789abcdef0123456789abcdef";
    EXPECT_EQ(gint::from_string<gint::Int256>(negative), gint::from_string<gint::Int256>(negative.c_str()));
}

TEST(WideIntegerConversion, FromStringPowerOfTwoWrapAndValidation)
{
    using U128 = gint::UInt128;
    using U1024 = gint::integer<1024, unsigned>;

    std::string binary(2051, '0');
    for (size_t i = 0; i < binary.size(); ++i)
        binary[i] = static_cast<char>('0' + ((i * 5u + 1u) & 1u));
    EXPECT_EQ(gint::from_string<U128>(binary, 2), parse_string_digit_by_digit<U128>(binary, 2));
    EXPECT_EQ(gint::from_string<U1024>(binary.c_str(), 2), parse_string_digit_by_digit<U1024>(binary, 2));

    std::string octal(701, '0');
    for (size_t i = 0; i < octal.size(); ++i)
        octal[i] = digit_character(static_cast<unsigned>((i * 7u + 3u) % 8u));
    EXPECT_EQ(gint::from_string<U128>(octal.c_str(), 8), parse_string_digit_by_digit<U128>(octal, 8));
    EXPECT_EQ(gint::from_string<U1024>(octal, 8), parse_string_digit_by_digit<U1024>(octal, 8));

    std::string hexadecimal(513, '0');
    for (size_t i = 0; i < hexadecimal.size(); ++i)
        hexadecimal[i] = digit_character(static_cast<unsigned>((i * 11u + 9u) % 16u));
    EXPECT_EQ(gint::from_string<U128>(hexadecimal, 16), parse_string_digit_by_digit<U128>(hexadecimal, 16));
    EXPECT_EQ(gint::from_string<U1024>(hexadecimal.c_str(), 16), parse_string_digit_by_digit<U1024>(hexadecimal, 16));

    std::string invalid_high_digit = "z" + std::string(300, '0');
    EXPECT_THROW(gint::from_string<U128>(invalid_high_digit, 16), std::invalid_argument);
    EXPECT_THROW(gint::from_string<U128>(invalid_high_digit.c_str(), 16), std::invalid_argument);
}

TEST(WideIntegerConversion, FromStringPowerOfTwoRejectsPunctuationBeforeA)
{
    const unsigned bases[] = {2, 8, 16};
    const char invalid_characters[] = {'@', '`'};
    for (size_t base_index = 0; base_index < sizeof(bases) / sizeof(bases[0]); ++base_index)
    {
        for (size_t character_index = 0; character_index < sizeof(invalid_characters) / sizeof(invalid_characters[0]); ++character_index)
        {
            std::string text = "1";
            text.push_back(invalid_characters[character_index]);
            text.push_back('1');
            EXPECT_THROW(gint::from_string<gint::UInt256>(text, bases[base_index]), std::invalid_argument);
            EXPECT_THROW(gint::from_string<gint::UInt256>(text.c_str(), bases[base_index]), std::invalid_argument);
        }
    }

    EXPECT_THROW(gint::from_string<gint::UInt256>("Ab0Bcde3456789abcdef89abcdef010a`bcd", 16), std::invalid_argument);
    EXPECT_THROW(gint::from_string<gint::UInt256>("0x1@1"), std::invalid_argument);

    const size_t boundary_lengths[] = {16, 17, 63, 64, 65, 127, 128, 129};
    typedef gint::integer<1024, unsigned> U1024;
    for (size_t length_index = 0; length_index < sizeof(boundary_lengths) / sizeof(boundary_lengths[0]); ++length_index)
    {
        const size_t length = boundary_lengths[length_index];
        EXPECT_NO_THROW(gint::from_string<U1024>(std::string(length, 'a'), 16));
        for (size_t position = 0; position < length; ++position)
        {
            for (size_t character_index = 0; character_index < sizeof(invalid_characters) / sizeof(invalid_characters[0]);
                 ++character_index)
            {
                SCOPED_TRACE(
                    ::testing::Message() << "length=" << length << ", position=" << position
                                         << ", byte=" << static_cast<unsigned>(invalid_characters[character_index]));
                std::string text(length, 'a');
                text[position] = invalid_characters[character_index];
                EXPECT_THROW(gint::from_string<U1024>(text, 16), std::invalid_argument);
                EXPECT_THROW(gint::from_string<U1024>(text.c_str(), 16), std::invalid_argument);
            }
        }
    }
}

TEST(WideIntegerConversion, FromStringHexClassifiesEveryByte)
{
    for (unsigned byte = 0; byte <= 0xffu; ++byte)
    {
        SCOPED_TRACE(::testing::Message() << "byte=" << byte);
        const bool decimal = byte >= static_cast<unsigned>('0') && byte <= static_cast<unsigned>('9');
        const bool uppercase = byte >= static_cast<unsigned>('A') && byte <= static_cast<unsigned>('F');
        const bool lowercase = byte >= static_cast<unsigned>('a') && byte <= static_cast<unsigned>('f');
        const bool valid = decimal || uppercase || lowercase;
        const std::string text(1, static_cast<char>(byte));
        const char c_text[] = {static_cast<char>(byte), '\0'};

        if (valid)
        {
            const unsigned expected = decimal ? byte - static_cast<unsigned>('0')
                : uppercase                   ? byte - static_cast<unsigned>('A') + 10u
                                              : byte - static_cast<unsigned>('a') + 10u;
            EXPECT_EQ(gint::from_string<gint::UInt256>(text, 16), gint::UInt256(expected));
            EXPECT_EQ(gint::from_string<gint::UInt256>(c_text, 16), gint::UInt256(expected));
        }
        else
        {
            EXPECT_THROW(gint::from_string<gint::UInt256>(text, 16), std::invalid_argument);
            EXPECT_THROW(gint::from_string<gint::UInt256>(c_text, 16), std::invalid_argument);
        }
    }
}

TEST(WideIntegerConversion, FromStringPreservesEmbeddedNullSemantics)
{
    const std::string with_null(
        "123\0"
        "456",
        7);
    EXPECT_THROW(gint::from_string<gint::UInt128>(with_null, 10), std::invalid_argument);
    EXPECT_EQ(gint::from_string<gint::UInt128>(with_null.c_str(), 10), gint::UInt128(123));
}

TEST(WideIntegerConversion, StringConstructorsAndAssignments)
{
    gint::UInt256 value("12345678901234567890");
    EXPECT_EQ(gint::to_string(value), "12345678901234567890");

    gint::UInt256 assigned;
    assigned = std::string("0x10000000000000000");
    EXPECT_EQ(assigned, gint::UInt256(1) << 64);

    assigned = "0b1001";
    EXPECT_EQ(assigned, gint::UInt256(9));
}

TEST(WideIntegerConversion, FromStringRejectsInvalidInput)
{
    EXPECT_THROW(gint::from_string<gint::UInt128>(""), std::invalid_argument);
    EXPECT_THROW(gint::from_string<gint::UInt128>("-"), std::invalid_argument);
    EXPECT_THROW(gint::from_string<gint::UInt128>("0x"), std::invalid_argument);
    EXPECT_THROW(gint::from_string<gint::UInt128>("12z", 10), std::invalid_argument);
    EXPECT_THROW(gint::from_string<gint::UInt128>("2", 2), std::invalid_argument);
    EXPECT_THROW(gint::from_string<gint::UInt128>("123", 1), std::invalid_argument);
    EXPECT_THROW((gint::from_string<128, unsigned>(nullptr)), std::invalid_argument);
}

TEST(WideIntegerConversion, FloatingDivisionBothWays)
{
    using S256 = gint::integer<256, signed>;
    S256 a = 1000;
    double b = 3.5;
    // Float operand is truncated to integer before integer division.
    EXPECT_EQ(S256(a / b), S256(a / S256(b)));
    EXPECT_EQ(S256(b / a), S256(S256(b) / a));
}

TEST(WideIntegerConversion, FloatingModuloBothWays)
{
    using S256 = gint::integer<256, signed>;
    S256 a = 1000;
    double b = 3.5;
    EXPECT_EQ(S256(a % b), S256(a % S256(b)));
    EXPECT_EQ(S256(b % a), S256(3));
}

TEST(WideIntegerConversion, Int128)
{
    gint::integer<128, unsigned> a = 0;
    a = static_cast<unsigned __int128>(1) << 100;
    EXPECT_EQ(gint::to_string(a), "1267650600228229401496703205376");
}

TEST(WideIntegerConversion, UnsignedRoundtrip)
{
    gint::integer<128, unsigned> w = 42;
    auto u = static_cast<uint64_t>(w);
    EXPECT_EQ(u, 42u);
    gint::integer<128, unsigned> w2 = u;
    EXPECT_EQ(w2, w);
}

TEST(WideIntegerConversion, SignedRoundtrip)
{
    gint::integer<128, signed> w = -123;
    auto i = static_cast<int64_t>(w);
    EXPECT_EQ(i, -123);
    gint::integer<128, signed> w2 = i;
    EXPECT_EQ(w2, w);
}

TEST(WideIntegerConversion, ArithmeticWithBuiltin)
{
    gint::integer<128, unsigned> a = 100;
    uint64_t b = 20;
    auto c = a + b;
    auto d = b + a;
    auto e = a * b;
    EXPECT_EQ(gint::to_string(c), "120");
    EXPECT_EQ(gint::to_string(d), "120");
    EXPECT_EQ(gint::to_string(e), "2000");
}

TEST(WideIntegerConversion, UnsignedRoundtripInt128)
{
    unsigned __int128 value = (static_cast<unsigned __int128>(1) << 80) + 42;
    gint::integer<256, unsigned> w = value;
    auto back = static_cast<unsigned __int128>(w);
    EXPECT_EQ(back, value);
}

TEST(WideIntegerConversion, SignedRoundtripInt128)
{
    __int128 value = -((static_cast<__int128>(1) << 90) + 77);
    gint::integer<256, signed> w = value;
    auto back = static_cast<__int128>(w);
    EXPECT_EQ(back, value);
}

TEST(WideIntegerConversion, Int128Arithmetic)
{
    gint::integer<256, unsigned> w = 100;
    unsigned __int128 b = 20;
    auto c = w + b;
    auto d = b + w;
    auto e = w * b;
    EXPECT_EQ(gint::to_string(c), "120");
    EXPECT_EQ(gint::to_string(d), "120");
    EXPECT_EQ(gint::to_string(e), "2000");
}

TEST(WideIntegerConversion, SignedToUnsignedConversion)
{
    gint::integer<256, signed> w = 123;
    auto via_explicit = static_cast<unsigned __int128>(w);
    EXPECT_TRUE(via_explicit == static_cast<unsigned __int128>(123));

    gint::integer<256, signed> negative = -1;
    auto neg_explicit = static_cast<unsigned __int128>(negative);
    EXPECT_TRUE(neg_explicit == static_cast<unsigned __int128>(-1));
}

TEST(WideIntegerConversion, Signed64ToInt128SignExtends)
{
    using S64 = gint::integer<64, signed>;
    S64 negative = -1;

    EXPECT_TRUE(static_cast<__int128>(negative) == static_cast<__int128>(-1));
    EXPECT_TRUE(static_cast<unsigned __int128>(negative) == ~static_cast<unsigned __int128>(0));
}

TEST(WideIntegerConversion, SignedConversionInt128)
{
    gint::integer<256, signed> w = 123;
    auto via_explicit = static_cast<__int128>(w);
    EXPECT_TRUE(via_explicit == static_cast<__int128>(123));

    gint::integer<256, signed> negative = -1;
    auto neg_explicit = static_cast<__int128>(negative);
    EXPECT_TRUE(neg_explicit == static_cast<__int128>(-1));
}

TEST(WideIntegerConversion, SameWidthIntegerConversionPreservesBits)
{
    using S128 = gint::integer<128, signed>;
    using U128 = gint::integer<128, unsigned>;

    U128 sign_bit = U128(1);
    sign_bit <<= 127;
    const S128 min_value = static_cast<S128>(sign_bit);
    EXPECT_EQ(min_value, std::numeric_limits<S128>::min());
    EXPECT_EQ(static_cast<U128>(min_value), sign_bit);

    const S128 negative = S128(-1);
    EXPECT_EQ(static_cast<U128>(negative), std::numeric_limits<U128>::max());
    EXPECT_EQ(static_cast<S128>(std::numeric_limits<U128>::max()), S128(-1));
}

TEST(WideIntegerConversion, CrossWidthIntegerConversionExtendsAndNarrows)
{
    test_integer_conversion_width_pair<64, 128>();
    test_integer_conversion_width_pair<128, 256>();
    test_integer_conversion_width_pair<256, 512>();
    test_integer_conversion_width_pair<512, 1024>();
}

TEST(WideIntegerConversion, CrossWidthIntegerAssignment)
{
    using S128 = gint::integer<128, signed>;
    using U128 = gint::integer<128, unsigned>;
    using S256 = gint::integer<256, signed>;
    using U256 = gint::integer<256, unsigned>;

    S256 signed_assigned;
    signed_assigned = S128(-42);
    EXPECT_EQ(signed_assigned, S256(-42));

    U256 unsigned_assigned;
    unsigned_assigned = S128(-1);
    EXPECT_EQ(unsigned_assigned, std::numeric_limits<U256>::max());

    U128 high_unsigned = U128(1);
    high_unsigned <<= 127;
    S256 positive_assigned;
    positive_assigned = high_unsigned;
    EXPECT_EQ(positive_assigned, S256(1) << 127);
}

TEST(WideIntegerConversion, SignedNarrowWidthUnsignedComparisons)
{
    using S64 = gint::integer<64, signed>;
    using S128 = gint::integer<128, signed>;

    const uint64_t u64_above_signed = 0x8000000000000005ULL;
    EXPECT_TRUE(S64(0) < u64_above_signed);
    EXPECT_TRUE(S64(0) <= u64_above_signed);
    EXPECT_TRUE(u64_above_signed > S64(0));
    EXPECT_TRUE(u64_above_signed >= S64(0));
    EXPECT_FALSE(S64(0) >= u64_above_signed);
    EXPECT_FALSE(S64(-1) == std::numeric_limits<uint64_t>::max());
    EXPECT_TRUE(S64(-1) < std::numeric_limits<uint64_t>::max());
    EXPECT_TRUE(S64(-1) <= std::numeric_limits<uint64_t>::max());

    const unsigned __int128 u128_above_s64 = static_cast<unsigned __int128>(1) << 80;
    EXPECT_TRUE(S64(0) < u128_above_s64);
    EXPECT_TRUE(S64(0) <= u128_above_s64);
    EXPECT_FALSE(u128_above_s64 < S64(0));
    EXPECT_TRUE(u128_above_s64 >= S64(0));

    const unsigned __int128 u128_above_s128 = (static_cast<unsigned __int128>(1) << 127) + 5;
    EXPECT_TRUE(S128(0) < u128_above_s128);
    EXPECT_TRUE(S128(0) <= u128_above_s128);
    EXPECT_FALSE(u128_above_s128 < S128(0));
    EXPECT_TRUE(u128_above_s128 >= S128(0));
    EXPECT_FALSE(S128(0) >= u128_above_s128);
}

template <typename T>
static void test_integral_ops()
{
    using W = gint::integer<256, signed>;
    const __int128 ai = 1000;
    W a = ai;
    T b = static_cast<T>(123);
    __int128 bi = static_cast<__int128>(b);
    EXPECT_EQ(a + b, W(ai + bi));
    EXPECT_EQ(b + a, W(bi + ai));
    EXPECT_EQ(a - b, W(ai - bi));
    EXPECT_EQ(b - a, W(bi - ai));
    EXPECT_EQ(a * b, W(ai * bi));
    EXPECT_EQ(b * a, W(bi * ai));
    EXPECT_EQ(a / b, W(ai / bi));
    EXPECT_EQ(b / a, W(bi / ai));
    EXPECT_EQ(a % b, W(ai % bi));
    EXPECT_EQ(b % a, W(bi % ai));
    EXPECT_EQ(a & b, W(ai & bi));
    EXPECT_EQ(b & a, W(ai & bi));
    EXPECT_EQ(a | b, W(ai | bi));
    EXPECT_EQ(b | a, W(ai | bi));
    EXPECT_EQ(a ^ b, W(ai ^ bi));
    EXPECT_EQ(b ^ a, W(ai ^ bi));
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(b <= a);
    EXPECT_TRUE(a != b);
}

template <typename T>
static void test_float_ops()
{
    using W = gint::integer<256, signed>;
    const __int128 ai = 1000;
    W a = ai;
    T b = static_cast<T>(123.5);
    // New semantics: mixed int/float arithmetic truncates the float to integer first.
    __int128 bi = static_cast<__int128>(b);
    __int128 expected_add = ai + bi;
    __int128 expected_sub = ai - bi;
    __int128 expected_rsub = bi - ai;
    __int128 expected_mul = ai * bi;
    __int128 expected_div = ai / bi;
    __int128 expected_rdiv = bi / ai;
    EXPECT_EQ(W(a + b), W(expected_add));
    EXPECT_EQ(W(b + a), W(expected_add));
    EXPECT_EQ(W(a - b), W(expected_sub));
    EXPECT_EQ(W(b - a), W(expected_rsub));
    EXPECT_EQ(W(a * b), W(expected_mul));
    EXPECT_EQ(W(b * a), W(expected_mul));
    EXPECT_EQ(W(a / b), W(expected_div));
    EXPECT_EQ(W(b / a), W(expected_rdiv));
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(b <= a);
    EXPECT_TRUE(a != b);
}

TEST(WideIntegerConversion, IntegralTypes)
{
    test_integral_ops<int8_t>();
    test_integral_ops<uint8_t>();
    test_integral_ops<int16_t>();
    test_integral_ops<uint16_t>();
    test_integral_ops<int32_t>();
    test_integral_ops<uint32_t>();
    test_integral_ops<int64_t>();
    test_integral_ops<uint64_t>();
    test_integral_ops<__int128>();
    test_integral_ops<unsigned __int128>();
}

TEST(WideIntegerConversion, FloatingTypes)
{
    test_float_ops<float>();
    test_float_ops<double>();
}

TEST(WideIntegerConversion, LongDoubleZero)
{
    gint::integer<128, signed> z = 0;
    EXPECT_EQ(static_cast<long double>(z), 0.0L);
    gint::integer<128, unsigned> from_zero = 0.0f;
    EXPECT_EQ(from_zero, (gint::integer<128, unsigned>(0)));
}

TEST(WideIntegerConversion, ExtraFloatConversion)
{
    gint::integer<128, unsigned> v = 123;
    double d = static_cast<double>(v);
    EXPECT_NEAR(d, 123.0, 1e-9);
    float f = static_cast<float>(v);
    EXPECT_NEAR(f, 123.0f, 1e-6f);

    float f2 = 456.0f;
    gint::integer<128, unsigned> w = f2;
    EXPECT_EQ(w, 456U);
    double d2 = -789.0;
    gint::integer<128, signed> s = d2;
    EXPECT_EQ(s, -789);
}

TEST(WideIntegerConversion, BoolConversion)
{
    using U128 = gint::integer<128, unsigned>;
    U128 z = 0;
    U128 o = 1;
    EXPECT_FALSE(static_cast<bool>(z));
    EXPECT_TRUE(static_cast<bool>(o));
}
