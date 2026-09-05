#ifndef GINT_TESTS_DIFFERENTIAL_ORACLE_H
#define GINT_TESTS_DIFFERENTIAL_ORACLE_H

#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#define GINT_TEST_ACCESS
#include <gint/gint.h>
#undef GINT_TEST_ACCESS

namespace gint_differential
{
using uint128_t = unsigned __int128;
using int128_t = __int128;
using uint1024_t = gint::integer<1024, unsigned>;

inline void require(bool condition, const char * message)
{
    if (!condition)
        throw std::runtime_error(message);
}

inline uint8_t input_byte(const uint8_t * data, size_t size, size_t index)
{
    return size == 0 ? uint8_t(0) : data[index % size];
}

template <size_t LimbCount>
struct reference_uint
{
    uint64_t limbs[LimbCount];

    reference_uint()
        : limbs()
    {
    }
};

template <size_t LimbCount>
bool wide_reference_is_zero(const reference_uint<LimbCount> & value)
{
    uint64_t combined = 0;
    for (size_t i = 0; i < LimbCount; ++i)
        combined |= value.limbs[i];
    return combined == 0;
}

template <size_t LimbCount>
int wide_reference_compare(const reference_uint<LimbCount> & lhs, const reference_uint<LimbCount> & rhs)
{
    for (size_t i = LimbCount; i-- > 0;)
    {
        if (lhs.limbs[i] < rhs.limbs[i])
            return -1;
        if (lhs.limbs[i] > rhs.limbs[i])
            return 1;
    }
    return 0;
}

template <size_t LimbCount>
bool wide_reference_bit(const reference_uint<LimbCount> & value, size_t bit)
{
    return ((value.limbs[bit / 64] >> (bit % 64)) & 1u) != 0;
}

template <size_t LimbCount>
void wide_reference_set_bit(reference_uint<LimbCount> & value, size_t bit)
{
    value.limbs[bit / 64] |= uint64_t(1) << (bit % 64);
}

template <size_t LimbCount>
bool wide_reference_shift_left_one(reference_uint<LimbCount> & value)
{
    const bool carry = (value.limbs[LimbCount - 1] >> 63) != 0;
    for (size_t i = LimbCount; i-- > 1;)
        value.limbs[i] = (value.limbs[i] << 1) | (value.limbs[i - 1] >> 63);
    value.limbs[0] <<= 1;
    return carry;
}

template <size_t LimbCount>
reference_uint<LimbCount> wide_reference_subtract(const reference_uint<LimbCount> & lhs, const reference_uint<LimbCount> & rhs)
{
    reference_uint<LimbCount> result;
    uint64_t borrow = 0;
    for (size_t i = 0; i < LimbCount; ++i)
    {
        const uint128_t subtrahend = static_cast<uint128_t>(rhs.limbs[i]) + borrow;
        result.limbs[i] = lhs.limbs[i] - static_cast<uint64_t>(subtrahend);
        borrow = static_cast<uint128_t>(lhs.limbs[i]) < subtrahend;
    }
    return result;
}

template <size_t LimbCount>
void wide_reference_negate(reference_uint<LimbCount> & value)
{
    uint64_t carry = 1;
    for (size_t i = 0; i < LimbCount; ++i)
    {
        const uint128_t sum = static_cast<uint128_t>(~value.limbs[i]) + carry;
        value.limbs[i] = static_cast<uint64_t>(sum);
        carry = static_cast<uint64_t>(sum >> 64);
    }
}

template <size_t LimbCount>
struct wide_reference_divmod_result
{
    reference_uint<LimbCount> quotient;
    reference_uint<LimbCount> remainder;
};

template <size_t LimbCount>
wide_reference_divmod_result<LimbCount>
wide_reference_divmod(const reference_uint<LimbCount> & dividend, const reference_uint<LimbCount> & divisor)
{
    require(!wide_reference_is_zero(divisor), "wide reference division by zero");
    wide_reference_divmod_result<LimbCount> result;
    for (size_t bit = LimbCount * 64; bit-- > 0;)
    {
        const bool carry = wide_reference_shift_left_one(result.remainder);
        result.remainder.limbs[0] |= wide_reference_bit(dividend, bit) ? 1u : 0u;
        if (carry || wide_reference_compare(result.remainder, divisor) >= 0)
        {
            result.remainder = wide_reference_subtract(result.remainder, divisor);
            wide_reference_set_bit(result.quotient, bit);
        }
    }
    return result;
}

template <size_t LimbCount>
reference_uint<LimbCount>
wide_reference_from_input(const uint8_t * data, size_t size, size_t offset, size_t byte_count, bool clear_top_bit = false)
{
    reference_uint<LimbCount> value;
    for (size_t i = 0; i < byte_count; ++i)
    {
        uint8_t byte = input_byte(data, size, offset + i);
        if (clear_top_bit && i + 1 == byte_count)
            byte &= 0x7fu;
        value.limbs[i / 8] |= uint64_t(byte) << ((i % 8) * 8);
    }
    return value;
}

template <size_t LimbCount>
void reference_mul_add(reference_uint<LimbCount> & value, unsigned multiplier, unsigned addend)
{
    uint128_t carry = addend;
    for (size_t i = 0; i < LimbCount; ++i)
    {
        const uint128_t product = static_cast<uint128_t>(value.limbs[i]) * multiplier + carry;
        value.limbs[i] = static_cast<uint64_t>(product);
        carry = product >> 64;
    }
}

using uint256_reference = reference_uint<4>;
using uint1024_reference = reference_uint<16>;
using reference_divmod_result = wide_reference_divmod_result<4>;

template <typename UInt>
struct gint_integer_access;

template <size_t Bits, typename Signed>
struct gint_integer_access<gint::integer<Bits, Signed>>
{
    using access = gint::detail::integer_test_access<Bits, Signed>;

    static uint64_t & limb(gint::integer<Bits, Signed> & value, size_t index) { return access::limb(value, index); }

    static const uint64_t & limb(const gint::integer<Bits, Signed> & value, size_t index) { return access::limb(value, index); }
};

template <typename UInt>
UInt gint_from_input(const uint8_t * data, size_t size, size_t offset, size_t byte_count, bool clear_top_bit = false)
{
    UInt value = 0;
    for (size_t i = 0; i < byte_count; ++i)
    {
        uint8_t byte = input_byte(data, size, offset + i);
        if (clear_top_bit && i + 1 == byte_count)
            byte &= 0x7fu;
        gint_integer_access<UInt>::limb(value, i / 8) |= uint64_t(byte) << ((i % 8) * 8);
    }
    return value;
}

template <size_t Bits>
bool equal_wide_unsigned_bits(const gint::integer<Bits, unsigned> & actual, const reference_uint<Bits / 64> & expected)
{
    for (size_t limb = 0; limb < Bits / 64; ++limb)
    {
        if (gint_integer_access<gint::integer<Bits, unsigned>>::limb(actual, limb) != expected.limbs[limb])
            return false;
    }
    return true;
}

template <size_t Bits>
struct wide_signed_reference
{
    reference_uint<Bits / 64> magnitude;
    bool negative;
};

template <size_t Bits>
wide_signed_reference<Bits> make_wide_signed_reference(reference_uint<Bits / 64> magnitude, bool negative)
{
    wide_signed_reference<Bits> result = {magnitude, negative && !wide_reference_is_zero(magnitude)};
    return result;
}

template <size_t Bits>
reference_uint<Bits / 64> wide_signed_reference_bits(const wide_signed_reference<Bits> & value)
{
    reference_uint<Bits / 64> result = value.magnitude;
    if (value.negative)
        wide_reference_negate(result);
    return result;
}

template <size_t Bits>
bool equal_wide_signed_bits(const gint::integer<Bits, signed> & actual, const wide_signed_reference<Bits> & expected)
{
    return equal_wide_unsigned_bits(gint::integer<Bits, unsigned>(actual), wide_signed_reference_bits(expected));
}

template <size_t Bits>
void verify_wide_signed_division(
    const gint::integer<Bits, signed> & dividend,
    const gint::integer<Bits, signed> & divisor,
    const wide_signed_reference<Bits> & ref_dividend,
    const wide_signed_reference<Bits> & ref_divisor)
{
    typedef gint::integer<Bits, signed> Int;

    require(equal_wide_signed_bits(dividend, ref_dividend), "wide signed dividend differs from reference input");
    require(equal_wide_signed_bits(divisor, ref_divisor), "wide signed divisor differs from reference input");
    const wide_reference_divmod_result<Bits / 64> expected_magnitudes
        = wide_reference_divmod(ref_dividend.magnitude, ref_divisor.magnitude);
    const wide_signed_reference<Bits> expected_quotient
        = make_wide_signed_reference<Bits>(expected_magnitudes.quotient, ref_dividend.negative != ref_divisor.negative);
    const wide_signed_reference<Bits> expected_remainder
        = make_wide_signed_reference<Bits>(expected_magnitudes.remainder, ref_dividend.negative);
    const gint::divmod_result<Int> result = gint::divmod(dividend, divisor);
    require(equal_wide_signed_bits(result.quotient, expected_quotient), "wide signed division quotient differs from bitwise oracle");
    require(equal_wide_signed_bits(result.remainder, expected_remainder), "wide signed division remainder differs from bitwise oracle");
    require(result.quotient == dividend / divisor, "wide signed divmod quotient differs from operator/");
    require(result.remainder == dividend % divisor, "wide signed divmod remainder differs from operator%");
    require(result.quotient * divisor + result.remainder == dividend, "wide signed division identity failed");
    if (result.remainder != Int(0))
        require((result.remainder < Int(0)) == (dividend < Int(0)), "wide signed remainder has the wrong sign");
}

template <size_t Bits>
void exercise_wide_signed_min_division()
{
    typedef gint::integer<Bits, signed> Int;
    typedef reference_uint<Bits / 64> Reference;

    Reference min_magnitude;
    min_magnitude.limbs[Bits / 64 - 1] = uint64_t(1) << 63;
    Reference one;
    one.limbs[0] = 1;
    Reference small;
    small.limbs[0] = 123;
    Reference wide_divisor_magnitude;
    wide_divisor_magnitude.limbs[0] = 5;
    wide_divisor_magnitude.limbs[Bits / 64 - 1] = uint64_t(1) << 62;

    const Int min_value = std::numeric_limits<Int>::min();
    const Int wide_divisor = (Int(1) << (Bits - 2)) + Int(5);
    const wide_signed_reference<Bits> ref_min = make_wide_signed_reference<Bits>(min_magnitude, true);
    const wide_signed_reference<Bits> ref_minus_one = make_wide_signed_reference<Bits>(one, true);
    const wide_signed_reference<Bits> ref_small = make_wide_signed_reference<Bits>(small, false);
    const wide_signed_reference<Bits> ref_wide_divisor = make_wide_signed_reference<Bits>(wide_divisor_magnitude, false);

    verify_wide_signed_division(min_value, Int(-1), ref_min, ref_minus_one);
    verify_wide_signed_division(min_value, wide_divisor, ref_min, ref_wide_divisor);
    verify_wide_signed_division(Int(123), min_value, ref_small, ref_min);
    verify_wide_signed_division(min_value, min_value, ref_min, ref_min);
}

template <size_t Bits>
void exercise_wide_division(const uint8_t * data, size_t size)
{
    static_assert(Bits == 512 || Bits == 1024, "wide differential division only covers supported large widths");
    typedef gint::integer<Bits, unsigned> UInt;
    typedef gint::integer<Bits, signed> Int;
    typedef reference_uint<Bits / 64> Reference;

    const size_t value_bytes = Bits / 8;
    const uint8_t control = input_byte(data, size, 1);
    const unsigned divisor_shape = (control >> 2) & 3u;
    const size_t divisor_bytes = divisor_shape == 0 ? 8 : divisor_shape == 1 ? 16 : divisor_shape == 2 ? value_bytes / 2 : value_bytes;

    const UInt dividend = gint_from_input<UInt>(data, size, 2, value_bytes);
    UInt divisor = gint_from_input<UInt>(data, size, 2 + value_bytes, divisor_bytes);
    const Reference ref_dividend = wide_reference_from_input<Bits / 64>(data, size, 2, value_bytes);
    Reference ref_divisor = wide_reference_from_input<Bits / 64>(data, size, 2 + value_bytes, divisor_bytes);
    if (wide_reference_is_zero(ref_divisor))
    {
        divisor = 1;
        ref_divisor.limbs[0] = 1;
    }

    const wide_reference_divmod_result<Bits / 64> expected = wide_reference_divmod(ref_dividend, ref_divisor);
    const gint::divmod_result<UInt> result = gint::divmod(dividend, divisor);
    require(equal_wide_unsigned_bits(result.quotient, expected.quotient), "wide division quotient differs from bitwise oracle");
    require(equal_wide_unsigned_bits(result.remainder, expected.remainder), "wide division remainder differs from bitwise oracle");
    require(result.quotient == dividend / divisor, "wide divmod quotient differs from operator/");
    require(result.remainder == dividend % divisor, "wide divmod remainder differs from operator%");
    require(result.quotient * divisor + result.remainder == dividend, "wide division identity failed");
    require(result.remainder < divisor, "wide division remainder is not less than divisor");

    const UInt signed_dividend_magnitude = gint_from_input<UInt>(data, size, 2, value_bytes, true);
    UInt signed_divisor_magnitude = gint_from_input<UInt>(data, size, 2 + value_bytes, divisor_bytes, divisor_bytes == value_bytes);
    const Reference ref_signed_dividend_magnitude = wide_reference_from_input<Bits / 64>(data, size, 2, value_bytes, true);
    Reference ref_signed_divisor_magnitude
        = wide_reference_from_input<Bits / 64>(data, size, 2 + value_bytes, divisor_bytes, divisor_bytes == value_bytes);
    if (wide_reference_is_zero(ref_signed_divisor_magnitude))
    {
        signed_divisor_magnitude = 1;
        ref_signed_divisor_magnitude.limbs[0] = 1;
    }

    const bool dividend_negative = (control & 0x10u) != 0;
    const bool divisor_negative = (control & 0x20u) != 0;
    Int signed_dividend(signed_dividend_magnitude);
    Int signed_divisor(signed_divisor_magnitude);
    if (dividend_negative)
        signed_dividend = -signed_dividend;
    if (divisor_negative)
        signed_divisor = -signed_divisor;

    verify_wide_signed_division(
        signed_dividend,
        signed_divisor,
        make_wide_signed_reference<Bits>(ref_signed_dividend_magnitude, dividend_negative),
        make_wide_signed_reference<Bits>(ref_signed_divisor_magnitude, divisor_negative));

    if ((control & 0x0fu) == 0x0fu)
        exercise_wide_signed_min_division<Bits>();
}

using signed_reference = wide_signed_reference<256>;

inline void verify_unsigned_division(
    const gint::UInt256 & dividend,
    const gint::UInt256 & divisor,
    const uint256_reference & ref_dividend,
    const uint256_reference & ref_divisor)
{
    const reference_divmod_result expected = wide_reference_divmod(ref_dividend, ref_divisor);
    const gint::divmod_result<gint::UInt256> result = gint::divmod(dividend, divisor);

    require(equal_wide_unsigned_bits(result.quotient, expected.quotient), "unsigned division quotient differs from bitwise oracle");
    require(equal_wide_unsigned_bits(result.remainder, expected.remainder), "unsigned division remainder differs from bitwise oracle");
    require(result.quotient == dividend / divisor, "unsigned divmod quotient differs from operator/");
    require(result.remainder == dividend % divisor, "unsigned divmod remainder differs from operator%");
    require(result.quotient * divisor + result.remainder == dividend, "unsigned division identity failed");
    require(result.remainder < divisor, "unsigned division remainder is not less than divisor");
}

inline void verify_signed_division(
    const gint::Int256 & dividend,
    const gint::Int256 & divisor,
    const signed_reference & ref_dividend,
    const signed_reference & ref_divisor)
{
    const reference_divmod_result expected_magnitudes = wide_reference_divmod(ref_dividend.magnitude, ref_divisor.magnitude);
    const signed_reference expected_quotient
        = make_wide_signed_reference<256>(expected_magnitudes.quotient, ref_dividend.negative != ref_divisor.negative);
    const signed_reference expected_remainder = make_wide_signed_reference<256>(expected_magnitudes.remainder, ref_dividend.negative);
    const gint::divmod_result<gint::Int256> result = gint::divmod(dividend, divisor);

    require(equal_wide_signed_bits(result.quotient, expected_quotient), "signed division quotient differs from bitwise oracle");
    require(equal_wide_signed_bits(result.remainder, expected_remainder), "signed division remainder differs from bitwise oracle");
    require(result.quotient == dividend / divisor, "signed divmod quotient differs from operator/");
    require(result.remainder == dividend % divisor, "signed divmod remainder differs from operator%");
    require(result.quotient * divisor + result.remainder == dividend, "signed division identity failed");
    if (result.remainder != gint::Int256(0))
        require((result.remainder < gint::Int256(0)) == (dividend < gint::Int256(0)), "signed remainder has the wrong sign");
}

inline void exercise_division(const uint8_t * data, size_t size)
{
    const uint8_t control = input_byte(data, size, 1);
    const size_t divisor_bytes = (control & 3u) == 1u ? 8u : (control & 3u) == 2u ? 16u : 32u;

    const gint::UInt256 unsigned_dividend = gint_from_input<gint::UInt256>(data, size, 2, 32);
    gint::UInt256 unsigned_divisor = gint_from_input<gint::UInt256>(data, size, 34, divisor_bytes);
    const uint256_reference ref_unsigned_dividend = wide_reference_from_input<4>(data, size, 2, 32);
    uint256_reference ref_unsigned_divisor = wide_reference_from_input<4>(data, size, 34, divisor_bytes);
    if (wide_reference_is_zero(ref_unsigned_divisor))
    {
        unsigned_divisor = 1;
        ref_unsigned_divisor.limbs[0] = 1;
    }
    verify_unsigned_division(unsigned_dividend, unsigned_divisor, ref_unsigned_dividend, ref_unsigned_divisor);

    // Keep the established 256-bit checks on every input, then alternate the
    // larger widths to exercise their distinct limb loops and signed wrappers.
    if ((control & 0x40u) == 0)
        exercise_wide_division<512>(data, size);
    else
        exercise_wide_division<1024>(data, size);

    if (control == 0xffu)
    {
        uint256_reference min_magnitude;
        min_magnitude.limbs[3] = uint64_t(1) << 63;
        uint256_reference one;
        one.limbs[0] = 1;
        verify_signed_division(
            std::numeric_limits<gint::Int256>::min(),
            gint::Int256(-1),
            make_wide_signed_reference<256>(min_magnitude, true),
            make_wide_signed_reference<256>(one, true));
        return;
    }

    const gint::UInt256 signed_dividend_magnitude = gint_from_input<gint::UInt256>(data, size, 2, 32, true);
    gint::UInt256 signed_divisor_magnitude = gint_from_input<gint::UInt256>(data, size, 34, divisor_bytes, divisor_bytes == 32);
    const uint256_reference ref_signed_dividend_magnitude = wide_reference_from_input<4>(data, size, 2, 32, true);
    uint256_reference ref_signed_divisor_magnitude = wide_reference_from_input<4>(data, size, 34, divisor_bytes, divisor_bytes == 32);
    if (wide_reference_is_zero(ref_signed_divisor_magnitude))
    {
        signed_divisor_magnitude = 1;
        ref_signed_divisor_magnitude.limbs[0] = 1;
    }

    const bool dividend_negative = (control & 0x10u) != 0;
    const bool divisor_negative = (control & 0x20u) != 0;
    gint::Int256 signed_dividend(signed_dividend_magnitude);
    gint::Int256 signed_divisor(signed_divisor_magnitude);
    if (dividend_negative)
        signed_dividend = -signed_dividend;
    if (divisor_negative)
        signed_divisor = -signed_divisor;
    verify_signed_division(
        signed_dividend,
        signed_divisor,
        make_wide_signed_reference<256>(ref_signed_dividend_magnitude, dividend_negative),
        make_wide_signed_reference<256>(ref_signed_divisor_magnitude, divisor_negative));
}

inline unsigned reference_digit_value(char character)
{
    const unsigned value = static_cast<unsigned>(static_cast<unsigned char>(character));
    if (value >= static_cast<unsigned>('0') && value <= static_cast<unsigned>('9'))
        return value - static_cast<unsigned>('0');
    const unsigned lower = value | 0x20u;
    if (lower >= static_cast<unsigned>('a') && lower <= static_cast<unsigned>('z'))
        return lower - static_cast<unsigned>('a') + 10u;
    return 36u;
}

inline bool reference_parse(const std::string & text, unsigned base, uint256_reference & result)
{
    if (text.empty())
        return false;

    size_t pos = 0;
    bool negative = false;
    if (text[pos] == '+' || text[pos] == '-')
    {
        negative = text[pos] == '-';
        ++pos;
        if (pos == text.size())
            return false;
    }

    if (base != 0 && (base < 2 || base > 36))
        return false;
    if (base == 0)
    {
        if (text.size() - pos >= 2 && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X'))
        {
            base = 16;
            pos += 2;
        }
        else if (text.size() - pos >= 2 && text[pos] == '0' && (text[pos + 1] == 'b' || text[pos + 1] == 'B'))
        {
            base = 2;
            pos += 2;
        }
        else if (text.size() - pos >= 2 && text[pos] == '0')
        {
            base = 8;
        }
        else
        {
            base = 10;
        }
    }
    else if (base == 16 && text.size() - pos >= 2 && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X'))
    {
        pos += 2;
    }
    else if (base == 2 && text.size() - pos >= 2 && text[pos] == '0' && (text[pos + 1] == 'b' || text[pos + 1] == 'B'))
    {
        pos += 2;
    }

    if (pos == text.size())
        return false;

    for (; pos < text.size(); ++pos)
    {
        const unsigned digit = reference_digit_value(text[pos]);
        if (digit >= base)
            return false;
        reference_mul_add(result, base, digit);
    }
    if (negative)
        wide_reference_negate(result);
    return true;
}

inline bool reference_parse_1024_power_of_two(const std::string & text, unsigned base, uint1024_reference & result)
{
    if (text.empty() || (base != 2 && base != 8 && base != 16))
        return false;

    size_t pos = 0;
    bool negative = false;
    if (text[pos] == '+' || text[pos] == '-')
    {
        negative = text[pos] == '-';
        ++pos;
        if (pos == text.size())
            return false;
    }

    if (base == 16 && text.size() - pos >= 2 && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X'))
        pos += 2;
    else if (base == 2 && text.size() - pos >= 2 && text[pos] == '0' && (text[pos + 1] == 'b' || text[pos + 1] == 'B'))
        pos += 2;

    if (pos == text.size())
        return false;

    for (; pos < text.size(); ++pos)
    {
        const unsigned digit = reference_digit_value(text[pos]);
        if (digit >= base)
            return false;
        reference_mul_add(result, base, digit);
    }
    if (negative)
        wide_reference_negate(result);
    return true;
}

inline char reference_digit_character(unsigned digit, bool uppercase)
{
    return static_cast<char>(digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + (digit - 10));
}

struct parser_case
{
    std::string text;
    unsigned base;
};

inline parser_case make_parser_case(const uint8_t * data, size_t size)
{
    static const unsigned raw_bases[] = {0, 2, 3, 8, 10, 16, 36, 1, 37};
    static const unsigned valid_bases[] = {2, 3, 8, 10, 16, 36};
    const uint8_t base_selector = input_byte(data, size, 1);
    const uint8_t style = input_byte(data, size, 2) & 3u;
    parser_case generated;

    if (style == 0)
    {
        generated.base = raw_bases[base_selector % (sizeof(raw_bases) / sizeof(raw_bases[0]))];
        if (size > 3)
            generated.text.assign(reinterpret_cast<const char *>(data + 3), size - 3);
        return generated;
    }

    const unsigned effective_base = valid_bases[base_selector % (sizeof(valid_bases) / sizeof(valid_bases[0]))];
    const bool can_autodetect = effective_base == 2 || effective_base == 8 || effective_base == 10 || effective_base == 16;
    generated.base = can_autodetect && (base_selector & 0x40u) ? 0u : effective_base;
    if (base_selector & 0x10u)
        generated.text.push_back((base_selector & 0x20u) ? '-' : '+');

    if (generated.base == 0)
    {
        if (effective_base == 2)
            generated.text += (base_selector & 0x08u) ? "0B" : "0b";
        else if (effective_base == 16)
            generated.text += (base_selector & 0x08u) ? "0X" : "0x";
        else if (effective_base == 8)
            generated.text.push_back('0');
    }
    else if ((effective_base == 2 || effective_base == 16) && (base_selector & 0x08u))
    {
        generated.text += effective_base == 2 ? "0b" : "0x";
    }

    const size_t digit_start = generated.text.size();
    if (style == 3)
    {
        generated.text += "1";
        generated.text.push_back((input_byte(data, size, 3) & 1u) != 0 ? '`' : '@');
        generated.text += "1";
        return generated;
    }

    for (size_t i = 3; i < size; ++i)
    {
        const unsigned digit = static_cast<unsigned>(data[i]) % effective_base;
        generated.text.push_back(reference_digit_character(digit, (data[i] & 0x80u) != 0));
    }
    if (generated.text.size() == digit_start)
        generated.text.push_back('0');
    if (generated.base == 0 && effective_base == 10 && generated.text[digit_start] == '0')
        generated.text[digit_start] = '1';
    if (style == 2)
        generated.text.push_back('_');
    return generated;
}

inline void verify_parser_api(const std::string & text, unsigned base)
{
    uint256_reference expected;
    const bool valid = reference_parse(text, base, expected);

    bool unsigned_threw = false;
    try
    {
        const gint::UInt256 actual = gint::from_string<gint::UInt256>(text, base);
        require(valid, "UInt256 string parser accepted invalid input");
        require(equal_wide_unsigned_bits(actual, expected), "UInt256 string parser differs from multiply-add oracle");
    }
    catch (const std::invalid_argument &)
    {
        unsigned_threw = true;
    }
    require(unsigned_threw == !valid, "UInt256 string parser exception differs from reference parser");

    bool signed_threw = false;
    try
    {
        const gint::Int256 actual = gint::from_string<gint::Int256>(text, base);
        require(valid, "Int256 string parser accepted invalid input");
        require(
            equal_wide_unsigned_bits(gint::UInt256(actual), expected), "Int256 string parser bit pattern differs from reference parser");
    }
    catch (const std::invalid_argument &)
    {
        signed_threw = true;
    }
    require(signed_threw == !valid, "Int256 string parser exception differs from reference parser");
}

inline void verify_parser_1024_power_of_two_api(const std::string & text, unsigned base)
{
    uint1024_reference expected;
    const bool valid = reference_parse_1024_power_of_two(text, base, expected);

    bool string_threw = false;
    try
    {
        const uint1024_t actual = gint::from_string<uint1024_t>(text, base);
        require(valid, "UInt1024 string parser accepted invalid power-of-two input");
        require(equal_wide_unsigned_bits(actual, expected), "UInt1024 string parser differs from independent limb oracle");
    }
    catch (const std::invalid_argument &)
    {
        string_threw = true;
    }
    require(string_threw == !valid, "UInt1024 string parser exception differs from independent limb oracle");

    const std::string c_string_text(text.c_str());
    uint1024_reference c_string_expected;
    const bool c_string_valid = reference_parse_1024_power_of_two(c_string_text, base, c_string_expected);
    bool c_string_threw = false;
    try
    {
        const uint1024_t actual = gint::from_string<uint1024_t>(text.c_str(), base);
        require(c_string_valid, "UInt1024 C-string parser accepted invalid power-of-two input");
        require(equal_wide_unsigned_bits(actual, c_string_expected), "UInt1024 C-string parser differs from independent limb oracle");
    }
    catch (const std::invalid_argument &)
    {
        c_string_threw = true;
    }
    require(c_string_threw == !c_string_valid, "UInt1024 C-string parser exception differs from independent limb oracle");
}

inline void exercise_parser(const uint8_t * data, size_t size)
{
    const parser_case generated = make_parser_case(data, size);
    verify_parser_api(generated.text, generated.base);
    if (generated.base == 2 || generated.base == 8 || generated.base == 16)
        verify_parser_1024_power_of_two_api(generated.text, generated.base);

    const std::string c_string_text(generated.text.c_str());
    uint256_reference expected;
    const bool valid = reference_parse(c_string_text, generated.base, expected);
    bool threw = false;
    try
    {
        const gint::UInt256 actual = gint::from_string<gint::UInt256>(generated.text.c_str(), generated.base);
        require(valid, "UInt256 C-string parser accepted invalid input");
        require(equal_wide_unsigned_bits(actual, expected), "UInt256 C-string parser differs from reference parser");
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    require(threw == !valid, "UInt256 C-string parser exception differs from reference parser");
}

class rounding_guard
{
public:
    rounding_guard()
        : old_mode_(std::fegetround())
    {
    }

    ~rounding_guard() { std::fesetround(old_mode_); }

private:
    int old_mode_;
};

inline unsigned reference_bit_count(const uint256_reference & value)
{
    for (unsigned limb = 4; limb-- > 0;)
    {
        if (value.limbs[limb] == 0)
            continue;
        uint64_t top = value.limbs[limb];
        unsigned local_bits = 0;
        while (top != 0)
        {
            ++local_bits;
            top >>= 1;
        }
        return limb * 64 + local_bits;
    }
    return 0;
}

inline bool reference_any_low_bits(const uint256_reference & value, unsigned bit_count)
{
    const unsigned whole_limbs = bit_count / 64;
    for (unsigned limb = 0; limb < whole_limbs; ++limb)
    {
        if (value.limbs[limb] != 0)
            return true;
    }
    const unsigned partial_bits = bit_count % 64;
    if (partial_bits != 0)
    {
        const uint64_t mask = (uint64_t(1) << partial_bits) - 1;
        if ((value.limbs[whole_limbs] & mask) != 0)
            return true;
    }
    return false;
}

inline unsigned reference_uint128_bit_count(uint128_t value)
{
    unsigned bits = 0;
    while (value != 0)
    {
        ++bits;
        value >>= 1;
    }
    return bits;
}

template <typename Float>
Float reference_binary_float(const uint256_reference & magnitude, bool negative)
{
    static_assert(std::numeric_limits<Float>::radix == 2, "binary floating-point is required");
    const unsigned bit_count = reference_bit_count(magnitude);
    if (bit_count == 0)
        return Float(0);

    const unsigned precision = static_cast<unsigned>(std::numeric_limits<Float>::digits);
    const unsigned kept_bits = bit_count < precision ? bit_count : precision;
    unsigned shift = bit_count - kept_bits;
    uint128_t significand = 0;
    for (unsigned i = 0; i < kept_bits; ++i)
        significand = (significand << 1) | (wide_reference_bit(magnitude, bit_count - i - 1) ? 1u : 0u);

    if (shift != 0)
    {
        const bool guard = wide_reference_bit(magnitude, shift - 1);
        const bool sticky = reference_any_low_bits(magnitude, shift - 1);
        const bool discarded = guard || sticky;
        bool increment = false;
        const int rounding_mode = std::fegetround();
        if (rounding_mode == FE_TONEAREST)
            increment = guard && (sticky || (significand & 1u) != 0);
        else if (rounding_mode == FE_UPWARD)
            increment = !negative && discarded;
        else if (rounding_mode == FE_DOWNWARD)
            increment = negative && discarded;
        else if (rounding_mode == FE_TOWARDZERO)
            increment = false;
        else
            throw std::runtime_error("unsupported floating-point rounding mode");

        if (increment)
            ++significand;
        if (significand == (uint128_t(1) << precision))
        {
            significand >>= 1;
            ++shift;
        }
    }

    const unsigned significand_bits = reference_uint128_bit_count(significand);
    if (significand_bits - 1 + shift >= static_cast<unsigned>(std::numeric_limits<Float>::max_exponent))
    {
        const int rounding_mode = std::fegetround();
        const bool to_infinity
            = rounding_mode == FE_TONEAREST || (rounding_mode == FE_UPWARD && !negative) || (rounding_mode == FE_DOWNWARD && negative);
        const Float result = to_infinity && std::numeric_limits<Float>::has_infinity ? std::numeric_limits<Float>::infinity()
                                                                                     : std::numeric_limits<Float>::max();
        return negative ? -result : result;
    }
    Float result = 0;
    for (unsigned bit = significand_bits; bit-- > 0;)
    {
        result = std::ldexp(result, 1);
        if (((significand >> bit) & 1u) != 0)
            result += Float(1);
    }
    result = std::ldexp(result, static_cast<int>(shift));
    return negative ? -result : result;
}

template <typename Float>
bool equal_float(Float actual, Float expected)
{
    return actual == expected && std::signbit(actual) == std::signbit(expected);
}

inline void exercise_float_conversion(const uint8_t * data, size_t size)
{
    static const int rounding_modes[] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
    rounding_guard guard;
    require(std::fesetround(rounding_modes[input_byte(data, size, 1) & 3u]) == 0, "fesetround failed");

    const gint::UInt256 unsigned_256 = gint_from_input<gint::UInt256>(data, size, 2, 32);
    const uint256_reference ref_unsigned_256 = wide_reference_from_input<4>(data, size, 2, 32);
    require(
        equal_float(static_cast<double>(unsigned_256), reference_binary_float<double>(ref_unsigned_256, false)),
        "UInt256 to double differs from exact oracle");
    require(
        equal_float(static_cast<long double>(unsigned_256), reference_binary_float<long double>(ref_unsigned_256, false)),
        "UInt256 to long double differs from exact oracle");

    const gint::UInt256 signed_magnitude = gint_from_input<gint::UInt256>(data, size, 2, 32, true);
    const uint256_reference ref_signed_256 = wide_reference_from_input<4>(data, size, 2, 32, true);
    const bool signed_256_negative = (input_byte(data, size, 1) & 0x10u) != 0;
    gint::Int256 signed_256(signed_magnitude);
    if (signed_256_negative)
        signed_256 = -signed_256;
    require(
        equal_float(static_cast<double>(signed_256), reference_binary_float<double>(ref_signed_256, signed_256_negative)),
        "Int256 to double differs from exact oracle");
    require(
        equal_float(static_cast<long double>(signed_256), reference_binary_float<long double>(ref_signed_256, signed_256_negative)),
        "Int256 to long double differs from exact oracle");

    // Keep float inputs below 2^120 so every generated value is finite on all
    // supported GCC/Clang targets while still exercising many discarded bits.
    const gint::UInt128 unsigned_128 = gint_from_input<gint::UInt128>(data, size, 2, 15);
    const uint256_reference ref_unsigned_128 = wide_reference_from_input<4>(data, size, 2, 15);
    require(
        equal_float(static_cast<float>(unsigned_128), reference_binary_float<float>(ref_unsigned_128, false)),
        "UInt128 to float differs from exact oracle");

    const bool signed_128_negative = (input_byte(data, size, 1) & 0x20u) != 0;
    gint::Int128 signed_128(unsigned_128);
    if (signed_128_negative)
        signed_128 = -signed_128;
    require(
        equal_float(static_cast<float>(signed_128), reference_binary_float<float>(ref_unsigned_128, signed_128_negative)),
        "Int128 to float differs from exact oracle");
}

inline void exercise_input(const uint8_t * data, size_t size)
{
    if (size == 0)
        return;

    switch (data[0] % 3u)
    {
        case 0:
            exercise_division(data, size);
            break;
        case 1:
            exercise_parser(data, size);
            break;
        default:
            exercise_float_conversion(data, size);
            break;
    }
}
} // namespace gint_differential

#endif
