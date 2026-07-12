#pragma once

#include "standard.hpp"

#if !defined(GINT_DETAIL_CORE_ONLY) && !defined(GINT_DETAIL_IO_DEFINITIONS_INCLUDED)

// A translation unit may include <gint/core.h> first and upgrade to the
// umbrella header later. The core pass cleans up its implementation macros, so
// recreate the small subset needed by the string/stream/fmt extension pass.
#    ifndef GINT_DETAIL_HEADER_PASS_ACTIVE
#        define GINT_DETAIL_HEADER_PASS_ACTIVE
#        if defined(__GNUC__) || defined(__clang__)
#            define GINT_FORCE_INLINE inline __attribute__((always_inline))
#            define GINT_NOINLINE __attribute__((noinline))
#        else
#            define GINT_FORCE_INLINE inline
#            define GINT_NOINLINE
#        endif
#        if defined(__x86_64__) && GINT_GCC_TUNED_PATHS
#            define GINT_WIDE_PARSE_INLINE inline GINT_NOINLINE
#        else
#            define GINT_WIDE_PARSE_INLINE GINT_FORCE_INLINE
#        endif
#        if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#            define GINT_DETAIL_EXCEPTIONS_ENABLED 1
#            define GINT_THROW(exception) throw exception
#        else
#            define GINT_DETAIL_EXCEPTIONS_ENABLED 0
#            define GINT_THROW(exception) ::std::abort()
#        endif
#        if defined(GINT_ENABLE_DIVZERO_CHECKS)
#            define GINT_DETAIL_DIVZERO_CHECKS 1
#        else
#            define GINT_DETAIL_DIVZERO_CHECKS 0
#        endif
#        if GINT_GCC_TUNED_PATHS
#            define GINT_DETAIL_GCC_TUNED_POLICY 1
#        else
#            define GINT_DETAIL_GCC_TUNED_POLICY 0
#        endif
#        if GINT_CLANG_TUNED_PATHS
#            define GINT_DETAIL_CLANG_TUNED_POLICY 1
#        else
#            define GINT_DETAIL_CLANG_TUNED_POLICY 0
#        endif
#        if GINT_ENABLE_AARCH64_LIMB_ASM
#            define GINT_DETAIL_AARCH64_ASM_POLICY 1
#        else
#            define GINT_DETAIL_AARCH64_ASM_POLICY 0
#        endif
#        define GINT_DETAIL_CONFIG_NAMESPACE_I(divzero, gcc_tuned, clang_tuned, aarch64_asm, exceptions) \
            config_d##divzero##_g##gcc_tuned##_c##clang_tuned##_a##aarch64_asm##_e##exceptions
#        define GINT_DETAIL_CONFIG_NAMESPACE_II(divzero, gcc_tuned, clang_tuned, aarch64_asm, exceptions) \
            GINT_DETAIL_CONFIG_NAMESPACE_I(divzero, gcc_tuned, clang_tuned, aarch64_asm, exceptions)
#        define GINT_DETAIL_CONFIG_NAMESPACE \
            GINT_DETAIL_CONFIG_NAMESPACE_II( \
                GINT_DETAIL_DIVZERO_CHECKS, \
                GINT_DETAIL_GCC_TUNED_POLICY, \
                GINT_DETAIL_CLANG_TUNED_POLICY, \
                GINT_DETAIL_AARCH64_ASM_POLICY, \
                GINT_DETAIL_EXCEPTIONS_ENABLED)
#    endif

namespace gint
{
inline namespace GINT_DETAIL_CONFIG_NAMESPACE
{

//=== String and stream definitions =========================================
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
    const typename Int::limb_type base = 10000000000000000000ULL; // 1e19
    const unsigned chunk_digits = 19;

    constexpr size_t max_chunks = (Bits + 62) / 63;
    std::array<typename Int::limb_type, max_chunks> chunks{};
    size_t chunk_count = 0;
    while (!tmp.is_zero())
    {
        Int q;
        typename Int::limb_type rem = tmp.div_mod_small(base, q);
        chunks[chunk_count++] = rem;
        tmp = q;
    }

    std::string out;
    out.reserve(chunk_count * chunk_digits + 1);
    // most significant chunk
    {
        char buf[32];
        unsigned idx = 32;
        typename Int::limb_type x = chunks[chunk_count - 1];
        while (x)
        {
            buf[--idx] = static_cast<char>('0' + (x % 10));
            x /= 10;
        }
        out.append(buf + idx, 32 - idx);
    }
    // zero-padded remaining chunks
    for (size_t chunk_index = chunk_count - 1; chunk_index-- > 0;)
    {
        char buf[32];
        unsigned idx = 32;
        typename Int::limb_type x = chunks[chunk_index];
        for (unsigned i = 0; i < chunk_digits; ++i)
        {
            buf[--idx] = static_cast<char>('0' + (x % 10));
            x /= 10;
        }
        out.append(buf + idx, chunk_digits);
    }
    if (neg)
        out.insert(out.begin(), '-');
    return out;
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

inline size_t stream_prefix_size(const std::string & text) noexcept
{
    if (text.empty())
        return 0;
    if (text[0] == '-' || text[0] == '+')
        return 1;
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        return 2;
    return 0;
}

inline std::ostream & write_formatted_string(std::ostream & out, const std::string & text)
{
    const std::streamsize width = out.width(0);
    const std::streamsize size = static_cast<std::streamsize>(text.size());
    if (width <= size)
        return out.write(text.data(), size);

    const std::streamsize padding = width - size;
    const char fill = out.fill();
    const std::ios_base::fmtflags adjust = out.flags() & std::ios_base::adjustfield;
    const size_t prefix = (adjust == std::ios_base::internal) ? stream_prefix_size(text) : 0;

    if (adjust == std::ios_base::left)
    {
        out.write(text.data(), size);
        for (std::streamsize i = 0; i < padding; ++i)
            out.put(fill);
        return out;
    }

    if (prefix)
        out.write(text.data(), static_cast<std::streamsize>(prefix));
    for (std::streamsize i = 0; i < padding; ++i)
        out.put(fill);
    out.write(text.data() + prefix, size - static_cast<std::streamsize>(prefix));
    return out;
}

template <size_t Bits, typename Signed>
inline std::string to_base_string(const integer<Bits, Signed> & value, unsigned base, bool uppercase)
{
    if (base == 10)
        return to_string(value);

    const unsigned bits_per_digit = (base == 16) ? 4 : (base == 8) ? 3 : 1;
    const unsigned mask = (1u << bits_per_digit) - 1u;
    const size_t digit_count = (Bits + bits_per_digit - 1) / bits_per_digit;
    std::string out;
    out.reserve(digit_count);

    bool seen = false;
    for (size_t digit_index = digit_count; digit_index-- > 0;)
    {
        const size_t bit = digit_index * bits_per_digit;
        const size_t limb_index = bit / 64;
        const unsigned shift = static_cast<unsigned>(bit % 64);
        unsigned digit = static_cast<unsigned>((value.data_[limb_index] >> shift) & mask);
        if (shift + bits_per_digit > 64 && limb_index + 1 < integer<Bits, Signed>::limbs)
            digit |= static_cast<unsigned>(value.data_[limb_index + 1] << (64 - shift)) & mask;

        const unsigned valid_bits = static_cast<unsigned>((Bits - bit < bits_per_digit) ? (Bits - bit) : bits_per_digit);
        digit &= (1u << valid_bits) - 1u;

        if (digit || seen)
        {
            out.push_back(format_digit(digit, uppercase));
            seen = true;
        }
    }

    return seen ? out : std::string("0");
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

template <unsigned BitsPerDigit, size_t Bits, typename Signed>
GINT_FORCE_INLINE integer<Bits, Signed> parse_power_of_two_range(const char * begin, const char * end)
{
    using Int = integer<Bits, Signed>;
    using limb_type = typename Int::limb_type;
    const unsigned base = 1u << BitsPerDigit;
    const size_t chunk_digits = 64u / BitsPerDigit;

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
                const unsigned shift = static_cast<unsigned>(current_chunk_digits * BitsPerDigit);
                if (shift == 64u)
                {
                    for (size_t i = Int::limbs; i-- > 1;)
                        result.data_[i] = result.data_[i - 1];
                    result.data_[0] = chunk;
                }
                else
                {
                    const unsigned inv_shift = 64u - shift;
                    for (size_t i = Int::limbs; i-- > 1;)
                        result.data_[i] = (result.data_[i] << shift) | (result.data_[i - 1] >> inv_shift);
                    result.data_[0] = (result.data_[0] << shift) | chunk;
                }
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
GINT_FORCE_INLINE integer<Bits, Signed> parse_power_of_two_range_selected(const char * begin, const char * end, std::false_type)
{
    return parse_power_of_two_range<BitsPerDigit, Bits, Signed>(begin, end);
}

template <unsigned BitsPerDigit, size_t Bits, typename Signed>
GINT_FORCE_INLINE integer<Bits, Signed> parse_power_of_two_range_selected(const char * begin, const char * end, std::true_type)
{
    return parse_wide_power_of_two_range<BitsPerDigit, Bits, Signed>(begin, end);
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
    {
        Int result = parse_power_of_two_range_selected<1, Bits, Signed>(pos, end, std::integral_constant<bool, (Int::limbs > 4)>());
        return negative ? -result : result;
    }
    if (base == 8)
    {
        Int result = parse_power_of_two_range_selected<3, Bits, Signed>(pos, end, std::integral_constant<bool, (Int::limbs > 4)>());
        return negative ? -result : result;
    }
    if (base == 16)
    {
        Int result = parse_power_of_two_range_selected<4, Bits, Signed>(pos, end, std::integral_constant<bool, (Int::limbs > 4)>());
        return negative ? -result : result;
    }

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

#endif // !GINT_DETAIL_CORE_ONLY && !GINT_DETAIL_IO_DEFINITIONS_INCLUDED
