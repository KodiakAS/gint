#include "oracle.h"

#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

namespace
{
class deterministic_rng
{
public:
    explicit deterministic_rng(uint64_t seed)
        : state_(seed)
    {
    }

    uint64_t next()
    {
        uint64_t value = state_;
        value ^= value << 7;
        value ^= value >> 9;
        value ^= value << 8;
        state_ = value;
        return value;
    }

private:
    uint64_t state_;
};

size_t parse_iterations(const char * text)
{
    errno = 0;
    char * end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0)
        throw std::invalid_argument("iterations must be a positive integer");
    return static_cast<size_t>(value);
}

void print_input(const std::vector<uint8_t> & input)
{
    std::cerr << "input=";
    for (size_t i = 0; i < input.size(); ++i)
        std::cerr << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(input[i]);
    std::cerr << std::dec << '\n';
}

void verify_division_oracle_against_native()
{
    deterministic_rng rng(0x6a09e667f3bcc909ULL);
    for (unsigned iteration = 0; iteration < 512; ++iteration)
    {
        const gint_differential::uint128_t dividend = (static_cast<gint_differential::uint128_t>(rng.next()) << 64) | rng.next();
        gint_differential::uint128_t divisor = (static_cast<gint_differential::uint128_t>(rng.next()) << 64) | rng.next();
        if (divisor == 0)
            divisor = 1;

        gint_differential::uint256_reference ref_dividend;
        ref_dividend.limbs[0] = static_cast<uint64_t>(dividend);
        ref_dividend.limbs[1] = static_cast<uint64_t>(dividend >> 64);
        gint_differential::uint256_reference ref_divisor;
        ref_divisor.limbs[0] = static_cast<uint64_t>(divisor);
        ref_divisor.limbs[1] = static_cast<uint64_t>(divisor >> 64);
        const gint_differential::reference_divmod_result result = gint_differential::reference_divmod(ref_dividend, ref_divisor);

        const gint_differential::uint128_t quotient
            = (static_cast<gint_differential::uint128_t>(result.quotient.limbs[1]) << 64) | result.quotient.limbs[0];
        const gint_differential::uint128_t remainder
            = (static_cast<gint_differential::uint128_t>(result.remainder.limbs[1]) << 64) | result.remainder.limbs[0];
        gint_differential::require(
            result.quotient.limbs[2] == 0 && result.quotient.limbs[3] == 0 && quotient == dividend / divisor,
            "bitwise division oracle disagrees with native UInt128 quotient");
        gint_differential::require(
            result.remainder.limbs[2] == 0 && result.remainder.limbs[3] == 0 && remainder == dividend % divisor,
            "bitwise division oracle disagrees with native UInt128 remainder");
    }
}

gint_differential::uint256_reference reference_from_uint128(gint_differential::uint128_t value)
{
    gint_differential::uint256_reference result;
    result.limbs[0] = static_cast<uint64_t>(value);
    result.limbs[1] = static_cast<uint64_t>(value >> 64);
    return result;
}

gint_differential::uint128_t signed_magnitude(gint_differential::int128_t value)
{
    if (value >= 0)
        return static_cast<gint_differential::uint128_t>(value);
    return static_cast<gint_differential::uint128_t>(-(value + 1)) + 1;
}

template <typename Float>
void verify_unsigned_native_float(gint_differential::uint128_t value, const char * message)
{
    volatile gint_differential::uint128_t runtime_value = value;
    const Float native = static_cast<Float>(runtime_value);
    const gint::UInt128 actual(value);
    const gint_differential::uint256_reference reference = reference_from_uint128(value);
    gint_differential::require(
        gint_differential::equal_float(gint_differential::reference_binary_float<Float>(reference, false), native), message);
    gint_differential::require(gint_differential::equal_float(static_cast<Float>(actual), native), message);
}

template <typename Float>
void verify_signed_native_float(gint_differential::int128_t value, const char * message)
{
    volatile gint_differential::int128_t runtime_value = value;
    const Float native = static_cast<Float>(runtime_value);
    const gint::Int128 actual(value);
    const bool negative = value < 0;
    const gint_differential::uint256_reference reference = reference_from_uint128(signed_magnitude(value));
    gint_differential::require(
        gint_differential::equal_float(gint_differential::reference_binary_float<Float>(reference, negative), native), message);
    gint_differential::require(gint_differential::equal_float(static_cast<Float>(actual), native), message);
}

void verify_float_oracle_against_native()
{
    gint_differential::rounding_guard guard;
    gint_differential::require(std::fesetround(FE_TONEAREST) == 0, "fesetround failed for native float calibration");

    const gint_differential::uint128_t unsigned_values[] = {
        0,
        1,
        (gint_differential::uint128_t(1) << 24) + 1,
        (gint_differential::uint128_t(1) << 53) + 3,
        gint_differential::uint128_t(1) << 127,
        ~gint_differential::uint128_t(0),
    };
    for (size_t i = 0; i < sizeof(unsigned_values) / sizeof(unsigned_values[0]); ++i)
    {
        verify_unsigned_native_float<float>(unsigned_values[i], "float oracle disagrees with native unsigned __int128");
        verify_unsigned_native_float<double>(unsigned_values[i], "double oracle disagrees with native unsigned __int128");
        verify_unsigned_native_float<long double>(unsigned_values[i], "long double oracle disagrees with native unsigned __int128");
    }

    const gint_differential::int128_t signed_max = static_cast<gint_differential::int128_t>((gint_differential::uint128_t(1) << 127) - 1);
    const gint_differential::int128_t signed_min = -signed_max - 1;
    const gint_differential::int128_t signed_values[] = {
        0,
        1,
        -1,
        static_cast<gint_differential::int128_t>((gint_differential::uint128_t(1) << 54) + 3),
        -static_cast<gint_differential::int128_t>((gint_differential::uint128_t(1) << 54) + 3),
        signed_max,
        signed_min,
    };
    for (size_t i = 0; i < sizeof(signed_values) / sizeof(signed_values[0]); ++i)
    {
        verify_signed_native_float<float>(signed_values[i], "float oracle disagrees with native signed __int128");
        verify_signed_native_float<double>(signed_values[i], "double oracle disagrees with native signed __int128");
        verify_signed_native_float<long double>(signed_values[i], "long double oracle disagrees with native signed __int128");
    }

    deterministic_rng rng(0xbb67ae8584caa73bULL);
    for (unsigned iteration = 0; iteration < 256; ++iteration)
    {
        const gint_differential::uint128_t unsigned_value = (static_cast<gint_differential::uint128_t>(rng.next()) << 64) | rng.next();
        const gint_differential::int128_t signed_value = static_cast<gint_differential::int128_t>(unsigned_value);
        verify_unsigned_native_float<float>(unsigned_value, "float oracle random unsigned __int128 calibration failed");
        verify_unsigned_native_float<double>(unsigned_value, "double oracle random unsigned __int128 calibration failed");
        verify_signed_native_float<float>(signed_value, "float oracle random signed __int128 calibration failed");
        verify_signed_native_float<double>(signed_value, "double oracle random signed __int128 calibration failed");
    }
}

template <typename Float>
void verify_rounding_boundaries(int rounding_mode)
{
    const unsigned precision = static_cast<unsigned>(std::numeric_limits<Float>::digits);
    const gint_differential::uint128_t lower_value = gint_differential::uint128_t(1) << (precision + 1);
    const Float lower = std::ldexp(Float(1), static_cast<int>(precision + 1));
    const Float upper = lower + Float(4);

    for (unsigned offset = 1; offset <= 3; ++offset)
    {
        const gint_differential::uint128_t value = lower_value + offset;
        const gint::UInt128 positive(value);
        const gint::Int128 negative = -gint::Int128(value);
        const bool nearest_rounds_up = offset == 3;
        const Float positive_expected = rounding_mode == FE_UPWARD || (rounding_mode == FE_TONEAREST && nearest_rounds_up) ? upper : lower;
        const Float negative_expected
            = rounding_mode == FE_DOWNWARD || (rounding_mode == FE_TONEAREST && nearest_rounds_up) ? -upper : -lower;
        const gint_differential::uint256_reference reference = reference_from_uint128(value);

        gint_differential::require(
            gint_differential::equal_float(static_cast<Float>(positive), positive_expected), "positive rounding boundary differs");
        gint_differential::require(
            gint_differential::equal_float(static_cast<Float>(negative), negative_expected), "negative rounding boundary differs");
        gint_differential::require(
            gint_differential::equal_float(gint_differential::reference_binary_float<Float>(reference, false), positive_expected),
            "positive rounding oracle boundary differs");
        gint_differential::require(
            gint_differential::equal_float(gint_differential::reference_binary_float<Float>(reference, true), negative_expected),
            "negative rounding oracle boundary differs");
    }
}

template <typename Float>
Float expected_above_max(bool negative, int rounding_mode, bool nearest_overflows)
{
    const bool to_infinity = (rounding_mode == FE_TONEAREST && nearest_overflows) || (rounding_mode == FE_UPWARD && !negative)
        || (rounding_mode == FE_DOWNWARD && negative);
    const Float magnitude = to_infinity ? std::numeric_limits<Float>::infinity() : std::numeric_limits<Float>::max();
    return negative ? -magnitude : magnitude;
}

template <typename Float, typename UInt, typename Int>
void verify_overflow_case(const UInt & magnitude, int rounding_mode, bool nearest_overflows, const char * message)
{
    const Int negative = -Int(magnitude);
    const Float positive_expected = expected_above_max<Float>(false, rounding_mode, nearest_overflows);
    const Float negative_expected = expected_above_max<Float>(true, rounding_mode, nearest_overflows);
    gint_differential::require(gint_differential::equal_float(static_cast<Float>(magnitude), positive_expected), message);
    gint_differential::require(gint_differential::equal_float(static_cast<Float>(negative), negative_expected), message);
}

template <typename Float, typename UInt>
void verify_unsigned_overflow_case(const UInt & value, int rounding_mode, bool nearest_overflows, const char * message)
{
    const Float expected = expected_above_max<Float>(false, rounding_mode, nearest_overflows);
    gint_differential::require(gint_differential::equal_float(static_cast<Float>(value), expected), message);
}

void verify_float_overflow_boundaries(int rounding_mode)
{
    const gint::UInt256 one = 1;
    const gint::UInt256 max_finite_integer = (one << 128) - (one << 104);
    const gint::UInt256 half_ulp = one << 103;
    const gint::UInt256 below_midpoint = max_finite_integer + half_ulp - one;
    const gint::UInt256 midpoint = max_finite_integer + half_ulp;
    const gint::UInt256 huge = one << 200;

    gint_differential::require(
        gint_differential::equal_float(static_cast<float>(max_finite_integer), std::numeric_limits<float>::max()),
        "float maximum finite integer conversion differs");
    gint_differential::require(
        gint_differential::equal_float(static_cast<float>(-gint::Int256(max_finite_integer)), -std::numeric_limits<float>::max()),
        "negative float maximum finite integer conversion differs");
    verify_overflow_case<float, gint::UInt256, gint::Int256>(
        below_midpoint, rounding_mode, false, "float overflow value below nearest midpoint differs");
    verify_overflow_case<float, gint::UInt256, gint::Int256>(midpoint, rounding_mode, true, "float overflow midpoint differs");
    verify_overflow_case<float, gint::UInt256, gint::Int256>(huge, rounding_mode, true, "float far-overflow conversion differs");
}

void verify_double_overflow_boundaries(int rounding_mode)
{
    using UInt1024 = gint::integer<1024, unsigned>;
    const UInt1024 one = 1;
    const UInt1024 max_finite_integer = std::numeric_limits<UInt1024>::max() - ((one << 971) - one);
    const UInt1024 half_ulp = one << 970;
    const UInt1024 below_midpoint = max_finite_integer + half_ulp - one;
    const UInt1024 midpoint = max_finite_integer + half_ulp;
    const UInt1024 huge = std::numeric_limits<UInt1024>::max();

    gint_differential::require(
        gint_differential::equal_float(static_cast<double>(max_finite_integer), std::numeric_limits<double>::max()),
        "double maximum finite integer conversion differs");
    verify_unsigned_overflow_case<double, UInt1024>(
        below_midpoint, rounding_mode, false, "double overflow value below nearest midpoint differs");
    verify_unsigned_overflow_case<double, UInt1024>(midpoint, rounding_mode, true, "double overflow midpoint differs");
    verify_unsigned_overflow_case<double, UInt1024>(huge, rounding_mode, true, "double far-overflow conversion differs");
}

void verify_signed_min_float_conversion()
{
    gint_differential::uint256_reference min_magnitude;
    min_magnitude.limbs[3] = uint64_t(1) << 63;
    const gint::Int256 value = std::numeric_limits<gint::Int256>::min();
    gint_differential::require(
        gint_differential::equal_float(static_cast<float>(value), gint_differential::reference_binary_float<float>(min_magnitude, true)),
        "Int256 minimum to float differs from exact oracle");
    gint_differential::require(
        gint_differential::equal_float(static_cast<double>(value), gint_differential::reference_binary_float<double>(min_magnitude, true)),
        "Int256 minimum to double differs from exact oracle");
    gint_differential::require(
        gint_differential::equal_float(
            static_cast<long double>(value), gint_differential::reference_binary_float<long double>(min_magnitude, true)),
        "Int256 minimum to long double differs from exact oracle");
}

void verify_deterministic_float_boundaries()
{
    static const int rounding_modes[] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
    gint_differential::rounding_guard guard;
    for (size_t i = 0; i < sizeof(rounding_modes) / sizeof(rounding_modes[0]); ++i)
    {
        const int rounding_mode = rounding_modes[i];
        gint_differential::require(std::fesetround(rounding_mode) == 0, "fesetround failed for deterministic float boundaries");
        verify_rounding_boundaries<float>(rounding_mode);
        verify_rounding_boundaries<double>(rounding_mode);
        verify_float_overflow_boundaries(rounding_mode);
        verify_double_overflow_boundaries(rounding_mode);
        verify_signed_min_float_conversion();
    }
}

std::string patterned_digits(size_t length, unsigned base)
{
    std::string result(length, '0');
    for (size_t i = 0; i < length; ++i)
        result[i] = gint_differential::reference_digit_character(static_cast<unsigned>((i * 13u + 5u) % base), (i & 1u) != 0);
    result[0] = '1';
    return result;
}

void verify_uint1024_parser_boundaries()
{
    struct ParserBoundary
    {
        unsigned base;
        size_t multi_limb_digits;
        size_t over_width_digits;
        char invalid_digit;
    };
    const ParserBoundary boundaries[] = {
        {2, 769, 2051, '2'},
        {8, 257, 701, '8'},
        {16, 193, 513, 'g'},
    };

    for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i)
    {
        const ParserBoundary & boundary = boundaries[i];
        const std::string multi_limb = patterned_digits(boundary.multi_limb_digits, boundary.base);
        const std::string over_width = patterned_digits(boundary.over_width_digits, boundary.base);
        gint_differential::verify_parser_1024_power_of_two_api(multi_limb, boundary.base);
        gint_differential::verify_parser_1024_power_of_two_api(over_width, boundary.base);
        gint_differential::verify_parser_1024_power_of_two_api("-" + over_width, boundary.base);

        std::string invalid = multi_limb;
        invalid[invalid.size() / 2] = boundary.invalid_digit;
        gint_differential::verify_parser_1024_power_of_two_api(invalid, boundary.base);
        invalid[invalid.size() / 2] = '@';
        gint_differential::verify_parser_1024_power_of_two_api(invalid, boundary.base);
        invalid[invalid.size() / 2] = '`';
        gint_differential::verify_parser_1024_power_of_two_api(invalid, boundary.base);
    }
}
} // namespace

int main(int argc, char ** argv)
{
    try
    {
        const size_t iterations_per_seed = argc > 1 ? parse_iterations(argv[1]) : 512;
        verify_division_oracle_against_native();
        verify_float_oracle_against_native();
        verify_deterministic_float_boundaries();
        verify_uint1024_parser_boundaries();
        // Punctuation immediately before 'a' in ASCII used to underflow in the
        // power-of-two parser's case-folding arithmetic and become digit 9.
        gint_differential::verify_parser_api("123@456", 16);
        gint_differential::verify_parser_api("123`456", 16);
        std::vector<uint8_t> signed_min_division(66, 0);
        signed_min_division[1] = 0xffu;
        gint_differential::exercise_input(signed_min_division.data(), signed_min_division.size());

        const uint64_t seeds[] = {
            0x243f6a8885a308d3ULL,
            0x9e3779b97f4a7c15ULL,
            0xa4093822299f31d0ULL,
            0x13198a2e03707344ULL,
        };

        size_t total = 0;
        for (size_t seed_index = 0; seed_index < sizeof(seeds) / sizeof(seeds[0]); ++seed_index)
        {
            deterministic_rng rng(seeds[seed_index]);
            for (size_t iteration = 0; iteration < iterations_per_seed; ++iteration)
            {
                const size_t size = 4 + static_cast<size_t>(rng.next() % 509u);
                std::vector<uint8_t> input(size);
                for (size_t i = 0; i < size; ++i)
                    input[i] = static_cast<uint8_t>(rng.next());

                input[0] = static_cast<uint8_t>(iteration % 3u);
                if (input[0] == 1)
                    input[2] = static_cast<uint8_t>((input[2] & ~3u) | ((iteration / 3u) & 3u));

                try
                {
                    gint_differential::exercise_input(input.data(), input.size());
                }
                catch (const std::exception & error)
                {
                    std::cerr << "differential failure: seed=0x" << std::hex << seeds[seed_index] << std::dec << ", iteration=" << iteration
                              << ", reason=" << error.what() << '\n';
                    print_input(input);
                    return 1;
                }
                ++total;
            }
        }

        std::cout << "differential checks passed: " << total << " deterministic cases\n";
        return 0;
    }
    catch (const std::exception & error)
    {
        std::cerr << "differential runner error: " << error.what() << '\n';
        return 2;
    }
}
