#pragma once

#include "integer.hpp"

#if GINT_DETAIL_EXCEPTIONS_ENABLED && defined(__GLIBCXX__)
#    include <cxxabi.h>
#endif

// Decimal/base conversion, string parsing, and stream output for integers.

namespace gint
{
inline namespace GINT_DETAIL_CONFIG_NAMESPACE
{

//=== String and stream definitions =========================================
namespace detail
{
template <unsigned Width>
GINT_FORCE_INLINE char * write_decimal_chunk(char * end, uint64_t value) noexcept
{
    char * pos = end;
    do
    {
        *--pos = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (Width == 0 ? value != 0 : pos != end - Width);
    return pos;
}
} // namespace detail

template <size_t Bits, typename Signed>
inline std::string to_string(const integer<Bits, Signed> & v)
{
    integer<Bits, Signed> tmp = v;
    bool neg = false;
    if (std::is_same<Signed, signed>::value && (v.data_[integer<Bits, Signed>::limbs - 1] >> 63))
    {
        tmp = -v;
        neg = true;
    }
    if (tmp.is_zero())
        return "0";

    using Int = integer<Bits, Signed>;
    using Division = detail::division_ops<Bits, Signed>;
    const typename Int::limb_type base = 10000000000000000000ULL; // 1e19
    const unsigned chunk_digits = 19;

    constexpr size_t max_chunks = (Bits + 62) / 63; // 10^19 exceeds 2^63.
    std::array<typename Int::limb_type, max_chunks> chunks = {{}};
    size_t chunk_count = 0;
    while (!tmp.is_zero())
    {
        Int q;
        const typename Int::limb_type rem = Division::template div_mod_small_constant<base>(tmp, q);
        chunks[chunk_count++] = rem;
        tmp = q;
    }

    // Write least significant chunks first into the tail of the final buffer.
    char digits[max_chunks * chunk_digits + 1];
    char * const end = digits + sizeof(digits);
    char * pos = end;
    for (size_t i = 0; i + 1 < chunk_count; ++i)
        pos = detail::write_decimal_chunk<chunk_digits>(pos, chunks[i]);
    pos = detail::write_decimal_chunk<0>(pos, chunks[chunk_count - 1]);
    if (neg)
        *--pos = '-';
    return std::string(pos, static_cast<size_t>(end - pos));
}

namespace detail
{
inline unsigned string_digit_value(char c) noexcept
{
    const unsigned value = static_cast<unsigned>(static_cast<unsigned char>(c));
    const unsigned decimal = value - static_cast<unsigned>('0');
    if (decimal <= 9u)
        return decimal;
    const unsigned alpha = (value | 0x20u) - static_cast<unsigned>('a');
    return alpha < 26u ? alpha + 10u : 36u;
}

inline unsigned hexadecimal_digit_value(unsigned value) noexcept
{
    // Full byte-domain table keeps validation and decoding to one load and one
    // range check. Constant initialization emits read-only data with no guard.
    static const unsigned char digits[256] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0,    1,    2,    3,    4,    5,    6,    7,    8,    9,    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 10,
        11,   12,   13,   14,   15,   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 10,   11,   12,   13,   14,   15,   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    return digits[value];
}

inline void detect_parse_base(const char * end, const char *& pos, unsigned & base)
{
    if (base != 0 && (base < 2 || base > 36))
        GINT_THROW(std::invalid_argument("gint::from_string invalid base"));

    if (base == 0)
    {
        if (end - pos >= 2 && pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        {
            base = 16;
            pos += 2;
        }
        else if (end - pos >= 2 && pos[0] == '0' && (pos[1] == 'b' || pos[1] == 'B'))
        {
            base = 2;
            pos += 2;
        }
        else if (end - pos >= 2 && pos[0] == '0')
        {
            base = 8;
        }
        else
        {
            base = 10;
        }
        return;
    }

    if (base == 16 && end - pos >= 2 && pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;
    else if (base == 2 && end - pos >= 2 && pos[0] == '0' && (pos[1] == 'b' || pos[1] == 'B'))
        pos += 2;
}

inline char format_digit(unsigned digit, bool uppercase) noexcept
{
    return static_cast<char>((digit < 10) ? ('0' + digit) : ((uppercase ? 'A' : 'a') + (digit - 10)));
}

inline std::streamsize stream_padding_offset(const std::string & text, std::ios_base::fmtflags align) noexcept
{
    if (align == std::ios_base::left)
        return static_cast<std::streamsize>(text.size());
    if (align != std::ios_base::internal || text.empty())
        return 0;
    if (text[0] == '-' || text[0] == '+')
        return 1;
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        return 2;
    return 0;
}

inline bool write_stream_padding(std::streambuf & buffer, std::streamsize count, char fill)
{
    char block[1024];
    const std::streamsize capacity = sizeof(block);
    std::memset(block, fill, static_cast<size_t>(count < capacity ? count : capacity));
    while (count > 0)
    {
        const std::streamsize n = count < capacity ? count : capacity;
        if (buffer.sputn(block, n) != n)
            return false;
        count -= n;
    }
    return true;
}

#if GINT_DETAIL_EXCEPTIONS_ENABLED
inline void set_stream_badbit(std::ostream & out) noexcept
{
    if (out.bad())
        return;
    // Preserve the original exception, including with the old ABI failure
    // type thrown by libstdc++ on GCC 5/6 with the new ABI (PR66145).
    try
    {
        out.setstate(std::ios_base::badbit);
    }
    catch (...)
    {
    }
}

#    if defined(__GLIBCXX__)
// A pthread cancellation is a foreign exception. GCC's null reference check
// misdiagnoses its ABI catch binding even though the reference is never used.
#        if !defined(__clang__) && __GNUC__ >= 8
__attribute__((no_sanitize("null")))
#        elif !defined(__clang__) && __GNUC__ >= 5
__attribute__((no_sanitize_undefined))
#        endif
inline void
rethrow_stream_cancellation()
{
    try
    {
        throw;
    }
    catch (__cxxabiv1::__forced_unwind &)
    {
        throw;
    }
    catch (...)
    {
    }
}
#    endif
#endif

inline std::ostream & write_formatted_string(std::ostream & out, const std::string & text)
{
    const std::streamsize width = out.width(0);
    const std::streamsize size = static_cast<std::streamsize>(text.size());

#if GINT_DETAIL_EXCEPTIONS_ENABLED
    if (!out.good())
    {
        // Preserve the standard library's sentry behavior on entry to a failed stream.
        const std::ostream::sentry sentry(out);
        return out;
    }

    try
    {
#endif
        std::ostream::sentry sentry(out);
        if (!sentry)
            return out;

        const std::streamsize padding = width > size ? width - size : 0;
        const std::streamsize before = padding ? stream_padding_offset(text, out.flags() & std::ios_base::adjustfield) : 0;
        std::streambuf & buffer = *out.rdbuf();
        if ((before && buffer.sputn(text.data(), before) != before) || (padding && !write_stream_padding(buffer, padding, out.fill()))
            || (before != size && buffer.sputn(text.data() + before, size - before) != size - before))
            out.setstate(std::ios_base::badbit);
#if GINT_DETAIL_EXCEPTIONS_ENABLED
    }
    catch (...)
    {
        set_stream_badbit(out);
        if (out.exceptions() & std::ios_base::badbit)
            throw;
#    if defined(__GLIBCXX__)
        rethrow_stream_cancellation();
#    endif
    }
#endif
    return out;
}

template <size_t Bits, typename Signed>
inline std::string to_base_string(const integer<Bits, Signed> & value, unsigned base, bool uppercase)
{
    if (base == 10)
        return to_string(value);

    const int high_bit = value.highest_bit();
    if (high_bit < 0)
        return "0";

    const unsigned bits_per_digit = (base == 16) ? 4 : (base == 8) ? 3 : 1;
    const unsigned mask = (1u << bits_per_digit) - 1u;
    const size_t digit_count = static_cast<size_t>(high_bit) / bits_per_digit + 1;
    std::string out(digit_count, '\0');
    char * next_digit = &out[0];

    for (size_t digit_index = digit_count; digit_index-- > 0;)
    {
        const size_t bit = digit_index * bits_per_digit;
        const size_t limb_index = bit / 64;
        const unsigned shift = static_cast<unsigned>(bit % 64);
        unsigned digit = static_cast<unsigned>((value.data_[limb_index] >> shift) & mask);
        if (shift + bits_per_digit > 64 && limb_index + 1 < integer<Bits, Signed>::limbs)
            digit |= static_cast<unsigned>(value.data_[limb_index + 1] << (64 - shift)) & mask;

        *next_digit++ = format_digit(digit, uppercase);
    }

    return out;
}

template <size_t Bits, typename Signed>
inline std::string format_stream_value(const integer<Bits, Signed> & value, const std::ios_base::fmtflags flags)
{
    const std::ios_base::fmtflags basefield = flags & std::ios_base::basefield;
    const bool uppercase = (flags & std::ios_base::uppercase) != 0;
    unsigned base = 10;
    if (basefield == std::ios_base::hex)
        base = 16;
    else if (basefield == std::ios_base::oct)
        base = 8;

    std::string text = to_base_string(value, base, uppercase);
    if (base == 10)
    {
        if ((flags & std::ios_base::showpos) && (text.empty() || text[0] != '-'))
            text.insert(text.begin(), '+');
        return text;
    }

    if ((flags & std::ios_base::showbase) && text != "0")
    {
        if (base == 16)
            text.insert(0, uppercase ? "0X" : "0x");
        else if (base == 8 && text[0] != '0')
            text.insert(text.begin(), '0');
    }
    return text;
}

template <size_t Limbs, unsigned Shift>
GINT_FORCE_INLINE void append_power_of_two_chunk(uint64_t * data, uint64_t chunk, std::true_type) noexcept
{
    for (size_t i = Limbs; i-- > 1;)
        data[i] = data[i - 1];
    data[0] = chunk;
}

template <size_t Limbs, unsigned Shift>
GINT_FORCE_INLINE void append_power_of_two_chunk(uint64_t * data, uint64_t chunk, std::false_type) noexcept
{
    const unsigned inv_shift = 64u - Shift;
    for (size_t i = Limbs; i-- > 1;)
        data[i] = (data[i] << Shift) | (data[i - 1] >> inv_shift);
    data[0] = (data[0] << Shift) | chunk;
}

template <unsigned BitsPerDigit, size_t Bits, typename Signed>
GINT_FORCE_INLINE integer<Bits, Signed> parse_power_of_two_range(const char * begin, const char * end)
{
    using Int = integer<Bits, Signed>;
    using limb_type = typename Int::limb_type;
    const unsigned base = 1u << BitsPerDigit;
    constexpr size_t chunk_digits = 64u / BitsPerDigit;

    Int result;
    const char * cursor = begin;
    size_t digits_left = static_cast<size_t>(end - begin);
    size_t current_chunk_digits = digits_left % chunk_digits;
    if (current_chunk_digits == 0)
        current_chunk_digits = chunk_digits;
    bool first_chunk = true;

    while (digits_left != 0)
    {
        limb_type chunk = 0;
        for (size_t i = 0; i < current_chunk_digits; ++i)
        {
            const unsigned value = static_cast<unsigned>(static_cast<unsigned char>(*cursor++));
            const unsigned digit = BitsPerDigit == 4u ? hexadecimal_digit_value(value) : value - static_cast<unsigned>('0');
            if (digit >= base)
                GINT_THROW(std::invalid_argument("gint::from_string invalid digit"));
            chunk = (chunk << BitsPerDigit) | digit;
        }

        digits_left -= current_chunk_digits;
        if (Int::limbs <= 4)
        {
            if (first_chunk)
            {
                result.data_[0] = chunk;
                first_chunk = false;
            }
            else
            {
                constexpr unsigned shift = static_cast<unsigned>(chunk_digits * BitsPerDigit);
                append_power_of_two_chunk<Int::limbs, shift>(result.data_, chunk, std::integral_constant<bool, (shift == 64u)>{});
            }
        }
        else if (digits_left <= Bits / BitsPerDigit)
        {
            const size_t bit_offset = digits_left * BitsPerDigit;
            if (bit_offset < Bits)
            {
                const size_t limb_index = bit_offset / 64u;
                const unsigned shift = static_cast<unsigned>(bit_offset % 64u);
                result.data_[limb_index] |= chunk << shift;
                if (shift && limb_index + 1 < Int::limbs)
                    result.data_[limb_index + 1] |= chunk >> (64u - shift);
            }
        }
        current_chunk_digits = chunk_digits;
    }
    return result;
}

template <unsigned BitsPerDigit, size_t Bits, typename Signed>
GINT_WIDE_PARSE_INLINE integer<Bits, Signed> parse_wide_power_of_two_range(const char * begin, const char * end)
{
    // GCC/x86 is sensitive to the layout of this large parser when it is
    // merged into parse_string_range. Keep wide power-of-two parsing in its
    // own code-generation unit; other compiler/architecture pairs inline it.
    return parse_power_of_two_range<BitsPerDigit, Bits, Signed>(begin, end);
}

template <unsigned BitsPerDigit, size_t Bits, typename Signed>
GINT_FORCE_INLINE integer<Bits, Signed> parse_signed_power_of_two_range(const char * begin, const char * end, bool negative)
{
    const auto result = integer<Bits, Signed>::limbs > 4 ? parse_wide_power_of_two_range<BitsPerDigit, Bits, Signed>(begin, end)
                                                         : parse_power_of_two_range<BitsPerDigit, Bits, Signed>(begin, end);
    return negative ? -result : result;
}

template <size_t Bits, typename Signed>
inline integer<Bits, Signed> parse_string_range(const char * begin, const char * end, unsigned base)
{
    if (begin == end)
        GINT_THROW(std::invalid_argument("gint::from_string empty string"));

    const char * pos = begin;
    bool negative = false;
    if (*pos == '+' || *pos == '-')
    {
        negative = *pos == '-';
        ++pos;
        if (pos == end)
            GINT_THROW(std::invalid_argument("gint::from_string sign without digits"));
    }

    detect_parse_base(end, pos, base);
    if (pos == end)
        GINT_THROW(std::invalid_argument("gint::from_string prefix without digits"));

    using Int = integer<Bits, Signed>;
    using limb_type = typename Int::limb_type;

    // Fixed-width parsing of power-of-two bases is pure bit packing. Work from
    // 64-bit digit chunks and place them directly into the destination limbs,
    // so over-width input naturally wraps modulo 2^Bits.
    if (base == 2)
        return parse_signed_power_of_two_range<1, Bits, Signed>(pos, end, negative);
    if (base == 8)
        return parse_signed_power_of_two_range<3, Bits, Signed>(pos, end, negative);
    if (base == 16)
        return parse_signed_power_of_two_range<4, Bits, Signed>(pos, end, negative);

    const limb_type max_limb = std::numeric_limits<limb_type>::max();
    limb_type chunk_base = 1;
    size_t chunk_digits = 0;
    while (chunk_base <= max_limb / base)
    {
        chunk_base *= base;
        ++chunk_digits;
    }

    Int result;
    bool first_chunk = true;
    size_t digits_left = static_cast<size_t>(end - pos);
    size_t current_chunk_digits = digits_left % chunk_digits;
    if (current_chunk_digits == 0)
        current_chunk_digits = chunk_digits;

    while (digits_left != 0)
    {
        limb_type chunk = 0;
        for (size_t i = 0; i < current_chunk_digits; ++i, ++pos)
        {
            const unsigned digit = string_digit_value(*pos);
            if (digit >= base)
                GINT_THROW(std::invalid_argument("gint::from_string invalid digit"));
            chunk = chunk * base + digit;
        }
        if (first_chunk)
        {
            result = chunk;
            first_chunk = false;
        }
        else
        {
            result.mul_add_limb(chunk_base, chunk);
        }
        digits_left -= current_chunk_digits;
        current_chunk_digits = chunk_digits;
    }

    return negative ? -result : result;
}
} // namespace detail

template <size_t Bits, typename Signed>
inline integer<Bits, Signed> from_string(const std::string & text, unsigned base)
{
    return detail::parse_string_range<Bits, Signed>(text.data(), text.data() + text.size(), base);
}

template <size_t Bits, typename Signed>
inline integer<Bits, Signed> from_string(const char * text, unsigned base)
{
    if (!text)
        GINT_THROW(std::invalid_argument("gint::from_string null string"));
    return detail::parse_string_range<Bits, Signed>(text, text + std::strlen(text), base);
}

template <typename Int>
inline Int from_string(const std::string & text, unsigned base)
{
    return from_string<Int::bits, typename Int::signed_tag>(text, base);
}

template <typename Int>
inline Int from_string(const char * text, unsigned base)
{
    return from_string<Int::bits, typename Int::signed_tag>(text, base);
}

template <size_t Bits, typename Signed>
inline std::ostream & operator<<(std::ostream & out, const integer<Bits, Signed> & value)
{
    const std::string text = detail::format_stream_value(value, out.flags());
    return detail::write_formatted_string(out, text);
} // LCOV_EXCL_LINE

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint
