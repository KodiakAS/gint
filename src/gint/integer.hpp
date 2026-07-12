#pragma once

#include "primitives.hpp"

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED

namespace gint
{
inline namespace GINT_DETAIL_CONFIG_NAMESPACE
{

//=== String and stream declarations =========================================
template <size_t Bits, typename Signed>
std::string to_string(const integer<Bits, Signed> & value);

namespace detail
{
template <size_t Bits, typename Signed>
std::string to_base_string(const integer<Bits, Signed> & value, unsigned base, bool uppercase);
}

template <size_t Bits, typename Signed>
integer<Bits, Signed> from_string(const std::string & text, unsigned base = 0);

template <size_t Bits, typename Signed>
integer<Bits, Signed> from_string(const char * text, unsigned base = 0);

template <typename Int>
Int from_string(const std::string & text, unsigned base = 0);

template <typename Int>
Int from_string(const char * text, unsigned base = 0);

template <size_t Bits, typename Signed>
std::ostream & operator<<(std::ostream & out, const integer<Bits, Signed> & value);

//=== Core integer type ======================================================
template <size_t Bits, typename Signed>
class integer
{
public:
    static constexpr size_t bits = Bits;
    static constexpr size_t limbs = detail::storage_count<Bits>::value;
    using limb_type = uint64_t;
    using signed_limb_type = int64_t;
    using signed_tag = Signed;
    // Constrain Signed to be exactly 'signed' or 'unsigned' tag types.
    static_assert(std::is_same<Signed, signed>::value || std::is_same<Signed, unsigned>::value, "Signed must be 'signed' or 'unsigned'.");
    template <size_t>
    friend struct detail::limbs_equal;
    template <size_t, typename>
    friend class integer;
    friend class std::numeric_limits<integer<Bits, Signed>>;
    friend struct std::hash<integer<Bits, Signed>>;
#    ifdef GINT_TEST_ACCESS
    friend struct detail::integer_test_access<Bits, Signed>;
#    endif

private:
    struct uninitialized_tag
    {
    };
    struct numeric_min_tag
    {
    };
    struct numeric_max_tag
    {
    };
    explicit integer(uninitialized_tag) noexcept { }

    template <size_t OtherBits, typename OtherSigned>
    GINT_CONSTEXPR14 void assign_integer(const integer<OtherBits, OtherSigned> & other) noexcept
    {
        const size_t src_limbs = integer<OtherBits, OtherSigned>::limbs;
        const size_t common_limbs = limbs < src_limbs ? limbs : src_limbs;
        for (size_t i = 0; i < common_limbs; ++i)
            data_[i] = other.data_[i];

        const bool source_negative = std::is_same<OtherSigned, signed>::value && (other.data_[src_limbs - 1] >> 63);
        const limb_type fill = source_negative ? ~limb_type(0) : limb_type(0);
        for (size_t i = common_limbs; i < limbs; ++i)
            data_[i] = fill;
    }

    template <size_t I, size_t OtherBits, typename OtherSigned>
    static constexpr typename std::enable_if<(I < integer<OtherBits, OtherSigned>::limbs), limb_type>::type
    limb_from_integer(const integer<OtherBits, OtherSigned> & other) noexcept
    {
        return other.data_[I];
    }

    template <size_t I, size_t OtherBits, typename OtherSigned>
    static constexpr typename std::enable_if<(I >= integer<OtherBits, OtherSigned>::limbs), limb_type>::type
    limb_from_integer(const integer<OtherBits, OtherSigned> & other) noexcept
    {
        return std::is_same<OtherSigned, signed>::value && (other.data_[integer<OtherBits, OtherSigned>::limbs - 1] >> 63) ? ~limb_type(0)
                                                                                                                           : limb_type(0);
    }

public:
    // Constructors
    constexpr integer() noexcept
        : data_{}
    {
    }
    constexpr integer(const integer &) noexcept = default;
    constexpr integer(integer &&) noexcept = default;

    template <
        size_t OtherBits,
        typename OtherSigned,
        typename std::enable_if<!(OtherBits == Bits && std::is_same<OtherSigned, Signed>::value), int>::type = 0>
    GINT_CONSTEXPR14 explicit integer(const integer<OtherBits, OtherSigned> & other) noexcept
        : integer(other, typename detail::make_index_sequence<limbs>::type())
    {
    }

    // Assignment operators
    GINT_CONSTEXPR14 integer & operator=(const integer &) noexcept = default;
    GINT_CONSTEXPR14 integer & operator=(integer &&) noexcept = default;

    template <
        size_t OtherBits,
        typename OtherSigned,
        typename std::enable_if<!(OtherBits == Bits && std::is_same<OtherSigned, Signed>::value), int>::type = 0>
    GINT_CONSTEXPR14 integer & operator=(const integer<OtherBits, OtherSigned> & other) noexcept
    {
        assign_integer(other);
        return *this;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    constexpr integer(T v) noexcept
        : integer(v, typename detail::make_index_sequence<limbs>::type())
    {
    }


    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    integer(T v) noexcept
    {
        assign_float(v);
    }

    explicit integer(const std::string & text) { *this = from_string<Bits, Signed>(text); }

    explicit integer(const char * text) { *this = from_string<Bits, Signed>(text); }

private:
    template <size_t OtherBits, typename OtherSigned, size_t... I>
    constexpr integer(const integer<OtherBits, OtherSigned> & other, detail::index_sequence<I...>) noexcept
        : data_{limb_from_integer<I>(other)...}
    {
    }

    template <typename T, size_t... I>
    constexpr integer(T v, detail::index_sequence<I...>) noexcept
        : data_{limb_from<T, I>(v)...}
    {
    }

    template <size_t I>
    static constexpr limb_type numeric_min_limb() noexcept
    {
        return std::is_same<Signed, signed>::value && I + 1 == limbs ? static_cast<limb_type>(1ULL << 63) : limb_type(0);
    }

    template <size_t I>
    static constexpr limb_type numeric_max_limb() noexcept
    {
        return std::is_same<Signed, signed>::value && I + 1 == limbs ? ~static_cast<limb_type>(1ULL << 63) : ~limb_type(0);
    }

    template <size_t... I>
    constexpr integer(numeric_min_tag, detail::index_sequence<I...>) noexcept
        : data_{numeric_min_limb<I>()...}
    {
    }

    template <size_t... I>
    constexpr integer(numeric_max_tag, detail::index_sequence<I...>) noexcept
        : data_{numeric_max_limb<I>()...}
    {
    }

    static constexpr integer numeric_min() noexcept
    {
        return integer(numeric_min_tag{}, typename detail::make_index_sequence<limbs>::type());
    }

    static constexpr integer numeric_max() noexcept
    {
        return integer(numeric_max_tag{}, typename detail::make_index_sequence<limbs>::type());
    }

    template <typename T, size_t I>
    static constexpr limb_type limb_from(T v) noexcept
    {
        // Build the 128-bit two's complement representation of v (sign-extended
        // to 128 bits if T is signed), then perform a logical right shift. This
        // avoids implementation-defined behavior of shifting negative signed values.
        typedef __int128 wide_signed;
        typedef unsigned __int128 wide_unsigned;
        return I < (sizeof(T) * 8 + 63) / 64
            ? static_cast<limb_type>(
                  (detail::is_signed<T>::value ? static_cast<wide_unsigned>(static_cast<wide_signed>(v)) : static_cast<wide_unsigned>(v))
                  >> (I * 64))
            : (detail::is_signed<T>::value && v < 0 ? ~0ULL : 0ULL);
    }

public:
    // Assignment operators
    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 integer & operator=(T v) noexcept
    {
        assign(v);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    integer & operator=(T v) noexcept
    {
        assign_float(v);
        return *this;
    }

    integer & operator=(const std::string & text)
    {
        *this = from_string<Bits, Signed>(text);
        return *this;
    }

    integer & operator=(const char * text)
    {
        *this = from_string<Bits, Signed>(text);
        return *this;
    }

    // Conversion operators
    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 explicit operator T() const noexcept
    {
        using u128 = unsigned __int128;
        using s128 = __int128;
        // Assemble up to the lower 128 bits from limbs
        u128 value = 0;
        const size_t t_limbs = (sizeof(T) + sizeof(limb_type) - 1) / sizeof(limb_type);
        for (size_t i = 0; i < limbs && i < t_limbs; ++i)
            value |= u128(data_[i]) << (i * 64);

        const unsigned wbits = static_cast<unsigned>(sizeof(T) * 8);
        if (std::is_same<Signed, signed>::value && Bits < 128 && wbits > Bits && (data_[limbs - 1] >> 63))
            value |= ~u128(0) << Bits;
        if (detail::is_unsigned<T>::value)
        {
            // Truncate to T's width and return
            if (wbits < 128)
            {
                const u128 mask = (u128(1) << wbits) - 1;
                value &= mask;
            }
            return static_cast<T>(value);
        }
        else
        {
            // Signed: perform explicit sign extension for widths < 128.
            if (wbits < 128)
            {
                const u128 mask = (u128(1) << wbits) - 1;
                value &= mask;
                const bool neg = (value >> (wbits - 1)) & 1;
                s128 s = static_cast<s128>(value);
                if (neg)
                    s -= static_cast<s128>(u128(1) << wbits);
                return static_cast<T>(s);
            }
            // wbits == 128: rely on two's complement conversion on GCC/Clang
            return static_cast<T>(static_cast<s128>(value));
        }
    }

    explicit operator long double() const noexcept { return to_binary_float<long double>(); }

    explicit operator double() const noexcept { return to_binary_float<double>(); }

    explicit operator float() const noexcept { return to_binary_float<float>(); }

    GINT_CONSTEXPR14 explicit operator bool() const noexcept { return !is_zero(); }

    // Arithmetic assignment operators
    GINT_CONSTEXPR14 integer & operator+=(const integer & rhs) noexcept
    {
        detail::add_limbs<limbs>(data_, rhs.data_);
        return *this;
    }

    GINT_CONSTEXPR14 integer & operator-=(const integer & rhs) noexcept
    {
        detail::sub_limbs<limbs>(data_, rhs.data_);
        return *this;
    }

    // Note: *= is not marked constexpr due to reliance on non-constexpr helpers
    integer & operator*=(const integer & rhs) noexcept
    {
        *this = *this * rhs;
        return *this;
    }

    // Note: /= is not constexpr because of optional zero-checks and complexity
    GINT_HIDDEN_VISIBILITY integer & operator/=(const integer & rhs)
    {
        *this = *this / rhs;
        return *this;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_HIDDEN_VISIBILITY integer & operator/=(T rhs)
    {
        *this = *this / rhs;
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    GINT_HIDDEN_VISIBILITY integer & operator/=(T rhs)
    {
        *this = *this / rhs;
        return *this;
    }

    // Note: %= is not constexpr because of optional zero-checks and complexity
    GINT_HIDDEN_VISIBILITY integer & operator%=(const integer & rhs)
    {
        *this = *this % rhs;
        return *this;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_HIDDEN_VISIBILITY integer & operator%=(T rhs)
    {
        *this = *this % rhs;
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    GINT_HIDDEN_VISIBILITY integer & operator%=(T rhs)
    {
        *this = *this % rhs;
        return *this;
    }

    // Bitwise assignment operators
    GINT_CONSTEXPR14 integer & operator&=(const integer & rhs) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] &= rhs.data_[i];
        return *this;
    }

    GINT_CONSTEXPR14 integer & operator|=(const integer & rhs) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] |= rhs.data_[i];
        return *this;
    }

    GINT_CONSTEXPR14 integer & operator^=(const integer & rhs) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] ^= rhs.data_[i];
        return *this;
    }

    // Shift operators
    // Notes:
    // - Non-positive shift amounts are a no-op by design (avoids UB).
    // - Right shift is arithmetic for signed integers (sign-propagating),
    //   and logical (zero-fill) for unsigned.
    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator<<=(int n) noexcept
    {
        if (n <= 0)
            return *this; // negative and zero shifts are no-ops by design
        size_t total_bits = Bits;
        size_t shift = static_cast<size_t>(n);
        if (shift >= total_bits)
        {
            for (size_t i = 0; i < limbs; ++i)
                data_[i] = 0;
            return *this;
        }
        size_t limb_shift = shift / 64;
        int bit_shift = shift % 64;
        if (limbs == 4)
        {
            const uint64_t src0 = static_cast<uint64_t>(data_[0]);
            const uint64_t src1 = static_cast<uint64_t>(data_[1]);
            const uint64_t src2 = static_cast<uint64_t>(data_[2]);
            const uint64_t src3 = static_cast<uint64_t>(data_[3]);
            uint64_t out0 = 0;
            uint64_t out1 = 0;
            uint64_t out2 = 0;
            uint64_t out3 = 0;
            if (bit_shift)
            {
                const unsigned inv_shift = 64U - static_cast<unsigned>(bit_shift);
                switch (limb_shift)
                {
                    case 0:
                        out0 = src0 << bit_shift;
                        out1 = (src1 << bit_shift) | (src0 >> inv_shift);
                        out2 = (src2 << bit_shift) | (src1 >> inv_shift);
                        out3 = (src3 << bit_shift) | (src2 >> inv_shift);
                        break;
                    case 1:
                        out1 = src0 << bit_shift;
                        out2 = (src1 << bit_shift) | (src0 >> inv_shift);
                        out3 = (src2 << bit_shift) | (src1 >> inv_shift);
                        break;
                    case 2:
                        out2 = src0 << bit_shift;
                        out3 = (src1 << bit_shift) | (src0 >> inv_shift);
                        break;
                    default:
                        out3 = src0 << bit_shift;
                        break;
                }
            }
            else
            {
                switch (limb_shift)
                {
                    case 1:
                        out1 = src0;
                        out2 = src1;
                        out3 = src2;
                        break;
                    case 2:
                        out2 = src0;
                        out3 = src1;
                        break;
                    default:
                        out3 = src0;
                        break;
                }
            }
            data_[0] = static_cast<limb_type>(out0);
            data_[1] = static_cast<limb_type>(out1);
            data_[2] = static_cast<limb_type>(out2);
            data_[3] = static_cast<limb_type>(out3);
            return *this;
        }
        if (limbs < 4)
        {
            if (limb_shift)
            {
                for (size_t i = limbs; i-- > limb_shift;)
                    data_[i] = data_[i - limb_shift];
                for (size_t i = 0; i < limb_shift; ++i)
                    data_[i] = 0;
            }
            if (bit_shift)
            {
                for (size_t i = limbs; i-- > 0;)
                {
                    unsigned __int128 part = static_cast<unsigned __int128>(data_[i]) << bit_shift;
                    if (i)
                        part |= data_[i - 1] >> (64 - bit_shift);
                    data_[i] = static_cast<limb_type>(part);
                }
            }
            return *this;
        }
        return shift_left_assign_wide(limb_shift, static_cast<unsigned>(bit_shift));
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value && !std::is_same<T, int>::value, int>::type = 0>
    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator<<=(T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return *this;
        if (shift_amount_reaches_width(n))
        {
            for (size_t i = 0; i < limbs; ++i)
                data_[i] = 0;
            return *this;
        }
        return *this <<= static_cast<int>(n);
    }

    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator>>=(int n) noexcept
    {
        if (n <= 0)
            return *this; // negative and zero shifts are no-ops by design
        const bool is_signed_t = std::is_same<Signed, signed>::value;
        const bool neg = is_signed_t && (data_[limbs - 1] >> 63);
        size_t total_bits = Bits;
        size_t shift = static_cast<size_t>(n);
        if (shift >= total_bits)
        {
            // Shifting out all bits: 0 for unsigned, -1 for signed negatives.
            const limb_type fill = (neg ? ~limb_type(0) : limb_type(0));
            for (size_t i = 0; i < limbs; ++i)
                data_[i] = fill;
            return *this;
        }
        size_t limb_shift = shift / 64;
        unsigned bit_shift = static_cast<unsigned>(shift % 64);
        const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
        if (limbs == 4)
        {
            const uint64_t src0 = static_cast<uint64_t>(data_[0]);
            const uint64_t src1 = static_cast<uint64_t>(data_[1]);
            const uint64_t src2 = static_cast<uint64_t>(data_[2]);
            const uint64_t src3 = static_cast<uint64_t>(data_[3]);
            const uint64_t fill_word = static_cast<uint64_t>(fill);
            uint64_t out0 = 0;
            uint64_t out1 = 0;
            uint64_t out2 = 0;
            uint64_t out3 = 0;
            if (bit_shift)
            {
                const unsigned inv_shift = 64U - bit_shift;
                switch (limb_shift)
                {
                    case 0:
                        out0 = (src0 >> bit_shift) | (src1 << inv_shift);
                        out1 = (src1 >> bit_shift) | (src2 << inv_shift);
                        out2 = (src2 >> bit_shift) | (src3 << inv_shift);
                        out3 = (src3 >> bit_shift) | (fill_word << inv_shift);
                        break;
                    case 1:
                        out0 = (src1 >> bit_shift) | (src2 << inv_shift);
                        out1 = (src2 >> bit_shift) | (src3 << inv_shift);
                        out2 = (src3 >> bit_shift) | (fill_word << inv_shift);
                        out3 = fill_word;
                        break;
                    case 2:
                        out0 = (src2 >> bit_shift) | (src3 << inv_shift);
                        out1 = (src3 >> bit_shift) | (fill_word << inv_shift);
                        out2 = fill_word;
                        out3 = fill_word;
                        break;
                    default:
                        out0 = (src3 >> bit_shift) | (fill_word << inv_shift);
                        out1 = fill_word;
                        out2 = fill_word;
                        out3 = fill_word;
                        break;
                }
            }
            else
            {
                switch (limb_shift)
                {
                    case 1:
                        out0 = src1;
                        out1 = src2;
                        out2 = src3;
                        out3 = fill_word;
                        break;
                    case 2:
                        out0 = src2;
                        out1 = src3;
                        out2 = fill_word;
                        out3 = fill_word;
                        break;
                    default:
                        out0 = src3;
                        out1 = fill_word;
                        out2 = fill_word;
                        out3 = fill_word;
                        break;
                }
            }
            data_[0] = static_cast<limb_type>(out0);
            data_[1] = static_cast<limb_type>(out1);
            data_[2] = static_cast<limb_type>(out2);
            data_[3] = static_cast<limb_type>(out3);
            return *this;
        }
        if (limbs < 4)
        {
            if (limb_shift)
            {
                for (size_t i = 0; i < limbs - limb_shift; ++i)
                    data_[i] = data_[i + limb_shift];
                for (size_t i = limbs - limb_shift; i < limbs; ++i)
                    data_[i] = fill;
            }
            if (bit_shift)
            {
                const unsigned inv_shift = 64U - bit_shift;
                const limb_type top = data_[limbs - 1];
                limb_type prev = top;
                for (size_t i = limbs - 1; i > 0; --i)
                {
                    limb_type cur = data_[i - 1];
                    data_[i - 1] = (cur >> bit_shift) | (prev << inv_shift);
                    prev = cur;
                }
                data_[limbs - 1] = (top >> bit_shift) | (fill << inv_shift);
            }
            return *this;
        }
        return shift_right_assign_wide(limb_shift, bit_shift, fill);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value && !std::is_same<T, int>::value, int>::type = 0>
    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator>>=(T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return *this;
        if (shift_amount_reaches_width(n))
        {
            const bool is_signed_t = std::is_same<Signed, signed>::value;
            const bool neg = is_signed_t && (data_[limbs - 1] >> 63);
            const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
            for (size_t i = 0; i < limbs; ++i)
                data_[i] = fill;
            return *this;
        }
        return *this >>= static_cast<int>(n);
    }

private:
    template <typename T>
    static GINT_CONSTEXPR14 typename std::enable_if<detail::is_signed<T>::value, bool>::type shift_amount_non_positive(T n) noexcept
    {
        return n <= 0;
    }

    template <typename T>
    static GINT_CONSTEXPR14 typename std::enable_if<!detail::is_signed<T>::value, bool>::type shift_amount_non_positive(T) noexcept
    {
        return false;
    }

    template <typename T>
    static GINT_CONSTEXPR14 typename std::enable_if<(sizeof(T) <= sizeof(size_t)), bool>::type shift_amount_reaches_width(T n) noexcept
    {
        return static_cast<size_t>(n) >= Bits;
    }

    template <typename T>
    static GINT_CONSTEXPR14 typename std::enable_if<(sizeof(T) > sizeof(size_t)), bool>::type shift_amount_reaches_width(T n) noexcept
    {
        return static_cast<unsigned __int128>(n) >= static_cast<unsigned __int128>(Bits);
    }

    static GINT_CONSTEXPR14 integer shifted_out_value(const integer & value) noexcept
    {
        const bool is_signed_t = std::is_same<Signed, signed>::value;
        const bool neg = is_signed_t && (value.data_[limbs - 1] >> 63);
        const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
        integer result;
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = fill;
        return result;
    }

    template <size_t L = limbs>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE typename std::enable_if<(L == 2 && std::is_same<Signed, signed>::value), integer>::type
    shift_right_int128_unsigned_value(const integer & lhs, size_t n) noexcept
    {
        if (GINT_LIKELY(n < 128U))
        {
            using s128 = __int128;
            using u128 = unsigned __int128;
            const u128 raw = (static_cast<u128>(lhs.data_[1]) << 64) | lhs.data_[0];
            const s128 shifted = static_cast<s128>(raw) >> n;
            const u128 shifted_raw = static_cast<u128>(shifted);
            integer result(uninitialized_tag{});
            result.data_[0] = static_cast<limb_type>(shifted_raw);
            result.data_[1] = static_cast<limb_type>(shifted_raw >> 64);
            return result;
        }

        return shifted_out_value(lhs);
    }

    template <size_t L = limbs>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE typename std::enable_if<(L == 2 && Bits == 128), integer>::type
    shift_left_int128_unsigned_value(const integer & lhs, unsigned n) noexcept
    {
        if (GINT_LIKELY(n < 128U))
        {
            using u128 = unsigned __int128;
            const u128 raw = (static_cast<u128>(lhs.data_[1]) << 64) | lhs.data_[0];
            const u128 shifted = raw << n;
            integer result(uninitialized_tag{});
            result.data_[0] = static_cast<limb_type>(shifted);
            result.data_[1] = static_cast<limb_type>(shifted >> 64);
            return result;
        }
        return integer();
    }

#    if !GINT_GCC_TUNED_PATHS
    template <size_t L = limbs>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE typename std::enable_if<(L == 2 && std::is_same<Signed, signed>::value), integer>::type
    shift_right_positive_value(const integer & lhs, size_t n) noexcept
    {
        return shift_right_int128_unsigned_value(lhs, n);
    }

    template <size_t L = limbs>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE typename std::enable_if<!(L == 2 && std::is_same<Signed, signed>::value), integer>::type
    shift_right_positive_value(const integer & lhs, size_t n) noexcept
    {
        return shift_right_value(lhs, static_cast<int>(n));
    }
#    endif

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_unsigned_value(const integer & lhs, unsigned n) noexcept
    {
        if (GINT_UNLIKELY(n >= Bits))
            return integer();
#    if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
        if (limbs <= 8)
        {
            integer result = lhs;
            result <<= static_cast<int>(n);
            return result;
        }
        return shift_left_value_by_size_in_range(lhs, n);
#    else
        integer result = lhs;
        result <<= static_cast<int>(n);
        return result;
#    endif
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_right_unsigned_value(const integer & lhs, unsigned n) noexcept
    {
        if (GINT_UNLIKELY(n >= Bits))
            return shifted_out_value(lhs);
#    if !GINT_GCC_TUNED_PATHS
        return shift_right_positive_value(lhs, n);
#    else
        integer result = lhs;
        result >>= static_cast<int>(n);
        return result;
#    endif
    }

    template <typename T>
    static GINT_CONSTEXPR14 integer shift_left_integral_value(const integer & lhs, T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return lhs;
        if (shift_amount_reaches_width(n))
            return integer();
#    if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
        if (limbs <= 8)
        {
            integer result = lhs;
            result <<= static_cast<int>(n);
            return result;
        }
        return shift_left_value(lhs, static_cast<int>(n));
#    else
        integer result = lhs;
        result <<= static_cast<int>(n);
        return result;
#    endif
    }

    template <typename T>
    static GINT_CONSTEXPR14 integer shift_right_integral_value(const integer & lhs, T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return lhs;
        if (shift_amount_reaches_width(n))
            return shifted_out_value(lhs);
#    if !GINT_GCC_TUNED_PATHS
        return shift_right_positive_value(lhs, static_cast<size_t>(n));
#    else
        integer result = lhs;
        result >>= static_cast<int>(n);
        return result;
#    endif
    }

    GINT_CONSTEXPR14 GINT_WIDE_SHIFT_INLINE integer & shift_left_assign_wide(size_t limb_shift, unsigned bit_shift) noexcept
    {
        if (bit_shift)
        {
            const unsigned shift_bits = static_cast<unsigned>(bit_shift);
            const unsigned inv_shift = 64U - shift_bits;
            for (size_t i = limbs; i-- > 0;)
            {
                if (i < limb_shift)
                {
                    data_[i] = 0;
                    continue;
                }

                const size_t src = i - limb_shift;
                limb_type part = data_[src] << shift_bits;
                if (src)
                    part |= data_[src - 1] >> inv_shift;
                data_[i] = part;
            }
        }
        else if (limb_shift)
        {
            for (size_t i = limbs; i-- > limb_shift;)
                data_[i] = data_[i - limb_shift];
            for (size_t i = 0; i < limb_shift; ++i)
                data_[i] = 0;
        }
        return *this;
    }

    GINT_CONSTEXPR14 GINT_WIDE_SHIFT_INLINE integer &
    shift_right_assign_wide(size_t limb_shift, unsigned bit_shift, limb_type fill) noexcept
    {
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            const size_t count = limbs - limb_shift;
            for (size_t i = 0; i < count; ++i)
            {
                const size_t src = i + limb_shift;
                limb_type part = data_[src] >> bit_shift;
                if (src + 1 < limbs)
                    part |= data_[src + 1] << inv_shift;
                else
                    part |= fill << inv_shift;
                data_[i] = part;
            }
            for (size_t i = count; i < limbs; ++i)
                data_[i] = fill;
        }
        else if (limb_shift)
        {
            const size_t count = limbs - limb_shift;
            for (size_t i = 0; i < count; ++i)
                data_[i] = data_[i + limb_shift];
            for (size_t i = count; i < limbs; ++i)
                data_[i] = fill;
        }
        return *this;
    }

#    if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_value_by_size_in_range(const integer & value, size_t shift) noexcept
    {
#        if __cplusplus >= 201402L
        integer result;
#        else
        integer result(uninitialized_tag{});
#        endif
        const size_t limb_shift = shift / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            for (size_t i = 0; i < limb_shift; ++i)
                result.data_[i] = 0;

            const size_t count = limbs - limb_shift;
            limb_type carry = 0;
            for (size_t i = 0; i < count; ++i)
            {
                const limb_type cur = value.data_[i];
                result.data_[i + limb_shift] = (cur << bit_shift) | carry;
                carry = cur >> inv_shift;
            }
        }
        else
        {
            for (size_t i = 0; i < limb_shift; ++i)
                result.data_[i] = 0;
            for (size_t i = limb_shift; i < limbs; ++i)
                result.data_[i] = value.data_[i - limb_shift];
        }
        return result;
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_value_by_size(const integer & value, size_t shift) noexcept
    {
        const size_t total_bits = Bits;
        if (shift >= total_bits)
            return integer();
        return shift_left_value_by_size_in_range(value, shift);
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_value(const integer & value, int n) noexcept
    {
        if (limbs == 4)
        {
            integer result = value;
            result <<= n;
            return result;
        }
        if (n <= 0)
            return value;

        return shift_left_value_by_size(value, static_cast<size_t>(n));
    }

#    endif

#    if !GINT_GCC_TUNED_PATHS
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE void
    shift_right_value_into(integer & result, const integer & value, size_t shift, limb_type fill) noexcept
    {
        if (shift >= Bits)
        {
            for (size_t i = 0; i < limbs; ++i)
                result.data_[i] = fill;
            return;
        }

        const size_t limb_shift = shift / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        const size_t count = limbs - limb_shift;
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            result.data_[0] = value.data_[limb_shift] >> bit_shift;
            for (size_t i = 1; i < count; ++i)
            {
                const limb_type cur = value.data_[limb_shift + i];
                result.data_[i - 1] |= cur << inv_shift;
                result.data_[i] = cur >> bit_shift;
            }
            result.data_[count - 1] |= fill << inv_shift;
        }
        else
        {
            for (size_t i = 0; i < count; ++i)
                result.data_[i] = value.data_[i + limb_shift];
        }
        for (size_t i = count; i < limbs; ++i)
            result.data_[i] = fill;
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_right_value(const integer & value, int n) noexcept
    {
        if (limbs == 4)
        {
            integer result = value;
            result >>= n;
            return result;
        }
        if (n <= 0)
            return value;

        const bool is_signed_t = std::is_same<Signed, signed>::value;
        const bool neg = is_signed_t && (value.data_[limbs - 1] >> 63);
        const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
        const size_t shift = static_cast<size_t>(n);
#        if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (!__builtin_is_constant_evaluated())
        {
            integer result(uninitialized_tag{});
            shift_right_value_into(result, value, shift, fill);
            return result;
        }
        integer result;
#        elif __cplusplus >= 201402L
        integer result;
#        else
        integer result(uninitialized_tag{});
#        endif
        shift_right_value_into(result, value, shift, fill);
        return result;
    }
#    endif

public:
    // Increment and decrement
    GINT_CONSTEXPR14 integer & operator++() noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
        {
            if (++data_[i])
                break;
        }
        return *this;
    }

    GINT_CONSTEXPR14 integer operator++(int) noexcept
    {
        integer tmp = *this;
        ++(*this);
        return tmp;
    }

    GINT_CONSTEXPR14 integer & operator--() noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
        {
            limb_type old = data_[i];
            --data_[i];
            if (old)
                break;
        }
        return *this;
    }

    GINT_CONSTEXPR14 integer operator--(int) noexcept
    {
        integer tmp = *this;
        --(*this);
        return tmp;
    }

    // Friend operators
    GINT_CONSTEXPR14 friend integer operator+(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
        detail::add_limbs_copy<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    GINT_CONSTEXPR14 friend integer operator+(integer lhs, limb_type rhs) noexcept
    {
        const limb_type old = lhs.data_[0];
        lhs.data_[0] += rhs;
        limb_type carry = lhs.data_[0] < old;
        for (size_t i = 1; i < limbs && carry; ++i)
            carry = ++lhs.data_[i] == 0;
        return lhs;
    }

    GINT_CONSTEXPR14 friend integer operator+(limb_type lhs, integer rhs) noexcept { return rhs + lhs; }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator+(const integer & lhs, T rhs) noexcept
    {
        integer rhs_int(rhs);
        integer result;
        detail::add_limbs_copy<limbs>(result.data_, lhs.data_, rhs_int.data_);
        return result;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator+(T lhs, const integer & rhs) noexcept
    {
        integer lhs_int(lhs);
        integer result;
        detail::add_limbs_copy<limbs>(result.data_, lhs_int.data_, rhs.data_);
        return result;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(const integer & lhs, T rhs) noexcept
    {
        integer rhs_int(rhs);
        integer result;
        detail::add_limbs_copy<limbs>(result.data_, lhs.data_, rhs_int.data_);
        return result;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(T lhs, const integer & rhs) noexcept
    {
        integer lhs_int(lhs);
        integer result;
        detail::add_limbs_copy<limbs>(result.data_, lhs_int.data_, rhs.data_);
        return result;
    }

    GINT_CONSTEXPR14 friend integer operator-(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
#    if GINT_DETAIL_X86_64_GCC
        if (limbs == 4 && (rhs.data_[1] | rhs.data_[2] | rhs.data_[3]) == 0)
        {
            detail::sub_limbs4_by_limb(result.data_, lhs.data_, rhs.data_[0]);
            return result;
        }
#    endif
        detail::sub_limbs_copy<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    GINT_CONSTEXPR14 friend integer operator-(integer lhs, limb_type rhs) noexcept
    {
        const limb_type old = lhs.data_[0];
        lhs.data_[0] -= rhs;
        limb_type borrow = old < rhs;
        for (size_t i = 1; i < limbs && borrow; ++i)
        {
            const limb_type current = lhs.data_[i];
            --lhs.data_[i];
            borrow = current == 0;
        }
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator-(const integer & lhs, T rhs) noexcept
    {
        integer rhs_int(rhs);
        integer result;
        detail::sub_limbs_copy<limbs>(result.data_, lhs.data_, rhs_int.data_);
        return result;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator-(T lhs, const integer & rhs) noexcept
    {
        integer lhs_int(lhs);
        integer result;
        detail::sub_limbs_copy<limbs>(result.data_, lhs_int.data_, rhs.data_);
        return result;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(const integer & lhs, T rhs) noexcept
    {
        integer rhs_int(rhs);
        integer result;
        detail::sub_limbs_copy<limbs>(result.data_, lhs.data_, rhs_int.data_);
        return result;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(T lhs, const integer & rhs) noexcept
    {
        integer lhs_int(lhs);
        integer result;
        detail::sub_limbs_copy<limbs>(result.data_, lhs_int.data_, rhs.data_);
        return result;
    }

    GINT_CONSTEXPR14 friend integer operator&(const integer & lhs, const integer & rhs) noexcept
    {
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (__builtin_is_constant_evaluated())
        {
            integer result;
            detail::bit_and_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
            return result;
        }
#    endif
        integer result(uninitialized_tag{});
        detail::bit_and_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator&(const integer & lhs, T rhs) noexcept
    {
        return lhs & integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator&(T lhs, const integer & rhs) noexcept
    {
        return integer(lhs) & rhs;
    }

    GINT_CONSTEXPR14 friend integer operator|(const integer & lhs, const integer & rhs) noexcept
    {
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (__builtin_is_constant_evaluated())
        {
            integer result;
            detail::bit_or_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
            return result;
        }
#    endif
        integer result(uninitialized_tag{});
        detail::bit_or_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator|(const integer & lhs, T rhs) noexcept
    {
        return lhs | integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator|(T lhs, const integer & rhs) noexcept
    {
        return integer(lhs) | rhs;
    }

    GINT_CONSTEXPR14 friend integer operator^(const integer & lhs, const integer & rhs) noexcept
    {
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (__builtin_is_constant_evaluated())
        {
            integer result;
            detail::bit_xor_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
            return result;
        }
#    endif
        integer result(uninitialized_tag{});
        detail::bit_xor_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator^(const integer & lhs, T rhs) noexcept
    {
        return lhs ^ integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator^(T lhs, const integer & rhs) noexcept
    {
        return integer(lhs) ^ rhs;
    }

#    if GINT_GCC_TUNED_PATHS
    GINT_CONSTEXPR14 friend integer operator<<(integer lhs, int n) noexcept
    {
        lhs <<= n;
        return lhs;
    }

    GINT_CONSTEXPR14 friend integer operator>>(integer lhs, int n) noexcept
    {
        lhs >>= n;
        return lhs;
    }
#    else
    template <size_t L = limbs, typename std::enable_if<(L <= 8), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator<<(integer lhs, int n) noexcept
    {
        lhs <<= n;
        return lhs;
    }

    template <size_t L = limbs, typename std::enable_if<(L > 8), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator<<(const integer & lhs, int n) noexcept
    {
        return shift_left_value(lhs, n);
    }

    template <size_t L = limbs, typename std::enable_if<(L <= 4), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(integer lhs, int n) noexcept
    {
        lhs >>= n;
        return lhs;
    }

    template <size_t L = limbs, typename std::enable_if<(L > 4), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, int n) noexcept
    {
        return shift_right_value(lhs, n);
    }
#    endif

    template <size_t L = limbs, typename std::enable_if<(L == 2 && Bits == 128), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator<<(const integer & lhs, unsigned n) noexcept
    {
        return shift_left_int128_unsigned_value(lhs, n);
    }

    template <size_t L = limbs, typename std::enable_if<!(L == 2 && Bits == 128), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator<<(const integer & lhs, unsigned n) noexcept
    {
        return shift_left_unsigned_value(lhs, n);
    }

    template <size_t L = limbs, typename std::enable_if<(L == 2 && std::is_same<Signed, signed>::value), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, unsigned n) noexcept
    {
        return shift_right_unsigned_value(lhs, n);
    }

    template <size_t L = limbs, typename std::enable_if<(L <= 4 && !(L == 2 && std::is_same<Signed, signed>::value)), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(integer lhs, unsigned n) noexcept
    {
        lhs >>= static_cast<int>(n);
        if (GINT_UNLIKELY(n >= Bits))
            return shifted_out_value(lhs);
        return lhs;
    }

    template <size_t L = limbs, typename std::enable_if<(L > 4), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, unsigned n) noexcept
    {
        return shift_right_unsigned_value(lhs, n);
    }

    template <size_t L = limbs, typename std::enable_if<(L == 4 && !std::is_same<size_t, unsigned>::value), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, size_t n) noexcept
    {
        integer result = lhs >> static_cast<unsigned>(n);
        if (GINT_UNLIKELY(n >= Bits))
            return shifted_out_value(lhs);
        return result;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value && !std::is_same<T, int>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator<<(const integer & lhs, T n) noexcept
    {
        return shift_left_integral_value(lhs, n);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value && !std::is_same<T, int>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, T n) noexcept
    {
        return shift_right_integral_value(lhs, n);
    }

    // Multiplication is left non-constexpr due to helper internals
    friend integer operator*(const integer & lhs, const integer & rhs) noexcept
    {
        integer result(uninitialized_tag{});
#    if GINT_DETAIL_X86_64_GCC
        if (limbs > 4 && GINT_UNLIKELY(lhs.data_[limbs - 1] == 0 || rhs.data_[limbs - 1] == 0)
            && detail::mul_try_single_limb_operand<limbs>(result.data_, lhs.data_, rhs.data_))
            return result;
#    endif
        // Dispatch to the limb-wise multiplication routine which selects the
        // appropriate algorithm based on operand size.
        detail::mul_limbs_result<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    friend integer operator*(integer lhs, limb_type rhs) noexcept
    {
        detail::mul_limb<limbs>(lhs.data_, rhs);
        return lhs;
    }

    friend integer operator*(limb_type lhs, integer rhs) noexcept { return rhs * lhs; }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator*(integer lhs, T rhs) noexcept
    {
        return lhs * integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator*(T lhs, integer rhs) noexcept
    {
        return integer(lhs) * rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator*(integer lhs, T rhs) noexcept
    {
        lhs *= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator*(T lhs, integer rhs) noexcept
    {
        rhs *= integer(lhs);
        return rhs;
    }

    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, const integer & rhs)
    {
#    if GINT_GCC_TUNED_PATHS
        limb_type positive_limb_divisor;
        if (positive_single_limb_value(rhs, positive_limb_divisor))
        {
            GINT_DIVZERO_CHECK(positive_limb_divisor == 0);
#        if GINT_DETAIL_AARCH64_GCC
            if (limbs == 2 && std::is_same<Signed, signed>::value && positive_limb_divisor > 0xFFFFFFFFULL
                && (positive_limb_divisor & (positive_limb_divisor - 1)) == 0)
                return div_by_positive_power_of_two(lhs, static_cast<int>(__builtin_ctzll(positive_limb_divisor)));
#        endif
            return div_by_positive_limb(lhs, positive_limb_divisor);
        }
#    elif GINT_DETAIL_AARCH64_CLANG
        if (limbs == 2)
        {
            int positive_pow_bit;
            if (positive_power_of_two_fastpath_divisor(rhs, positive_pow_bit))
                return div_by_positive_power_of_two(lhs, positive_pow_bit);

            limb_type positive_limb_divisor;
            if (positive_single_limb_value(rhs, positive_limb_divisor))
            {
                GINT_DIVZERO_CHECK(positive_limb_divisor == 0);
                return div_by_positive_limb(lhs, positive_limb_divisor);
            }
        }
#    endif

        int positive_pow_bit;
        if (positive_power_of_two_fastpath_divisor(rhs, positive_pow_bit))
            return div_by_positive_power_of_two(lhs, positive_pow_bit);

        bool lhs_neg = false;
        bool rhs_neg = false;
        bool lhs_is_min = false;
        bool rhs_is_min = false;
        integer divisor = rhs;
        if (std::is_same<Signed, signed>::value)
        {
            lhs_neg = lhs.data_[limbs - 1] >> 63;
            rhs_neg = divisor.data_[limbs - 1] >> 63;
#    if GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG
            if (lhs_neg && rhs_neg && negative_negative_div_quotient_is_zero(lhs, divisor))
                return integer();
#    endif
            const limb_type min_magnitude = static_cast<limb_type>(1ULL << 63);
            // Check the full min pattern only after the high limb matches it.
            if (lhs_neg)
            {
                if (GINT_UNLIKELY(lhs.data_[limbs - 1] == min_magnitude && is_min_value(lhs)))
                    lhs_is_min = true;
                else
                    negate_for_division(lhs);
            }
            // Apply the same high-limb gate to the divisor.
            if (rhs_neg)
            {
                if (GINT_UNLIKELY(divisor.data_[limbs - 1] == min_magnitude && is_min_value(divisor)))
                    rhs_is_min = true;
                else
                    negate_for_division(divisor);
            }
            if (GINT_UNLIKELY(lhs_is_min || rhs_is_min))
                return div_unsigned_path(lhs, divisor, lhs_neg, rhs_neg, lhs_is_min, rhs_is_min);
        }
        integer result;
        size_t divisor_limbs = limbs;
        while (divisor_limbs > 0 && divisor.data_[divisor_limbs - 1] == 0)
            --divisor_limbs;
        GINT_DIVZERO_CHECK(divisor_limbs == 0);
        bool small_divisor = divisor_limbs == 1;
        if (small_divisor)
        {
            // single-limb divisor: use fast division/remainder routine
            lhs.div_mod_small(divisor.data_[0], result);
        }
        else
        {
            int pow_bit;
            if (is_power_of_two(divisor, pow_bit))
            {
                // power-of-two divisor turns into a simple shift
                result = lhs >> pow_bit;
            }
            else if (limbs == 2)
            {
                // both operands are 128-bit wide
                result = div_128(lhs, divisor);
            }
            else if (divisor_limbs == 2)
            {
                // Specialized fast path: two-limb divisor
                result = div_large_2(lhs, divisor);
            }
            else if (divisor_limbs == 3)
            {
                // Specialized fast path: three-limb divisor
                result = div_large_3(lhs, divisor);
            }
            else if (limbs == 4 && divisor_limbs == 4)
            {
                // Specialized fast path: full-width 256-bit divisor produces a single quotient limb.
                result = div_large_4(lhs, divisor);
            }
            else
            {
                // Multi-limb divisor: use Knuth's Algorithm D (div_large)
                result = div_large(lhs, divisor, divisor_limbs);
            }
        }
        if (std::is_same<Signed, signed>::value && lhs_neg != rhs_neg)
            negate_for_division(result);
        return result;
    }

    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, limb_type rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        if (needs_unsigned_signed_promotion(rhs))
            return truncate_promoted_signed(promote_signed_self(lhs) / promote_integral_value(rhs));
        if (rhs <= static_cast<limb_type>(std::numeric_limits<signed_limb_type>::max()))
            return lhs / static_cast<signed_limb_type>(rhs);
        return lhs / integer(rhs);
    }

    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, signed_limb_type rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        if (GINT_UNLIKELY(rhs == 0))
            return integer();
        // For unsigned integers, mimic native casts: reinterpret negative divisors as their two's complement magnitude.
        if (std::is_same<Signed, unsigned>::value && rhs < 0)
            return lhs / integer(rhs);
        integer q;
        lhs.div_mod_small(rhs, q);
        return q;
    }

    friend GINT_HIDDEN_VISIBILITY integer operator/(limb_type lhs, integer rhs)
    {
        if (needs_unsigned_signed_promotion(lhs))
            return truncate_promoted_signed(promote_integral_value(lhs) / promote_signed_self(rhs));
        return integer(lhs) / rhs;
    }

    friend GINT_HIDDEN_VISIBILITY integer operator%(integer lhs, const integer & rhs)
    {
        GINT_MODZERO_CHECK(rhs.is_zero());
#    if GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG
        if (limbs == 2)
        {
            limb_type positive_limb_divisor;
            if (positive_single_limb_value(rhs, positive_limb_divisor))
            {
                integer result;
                if (std::is_same<Signed, signed>::value && (lhs.data_[1] >> 63))
                {
#        if GINT_DETAIL_AARCH64_GCC
                    return rem_negative_int128_by_positive_limb(lhs, positive_limb_divisor);
#        else
                    using Unsigned = integer<Bits, unsigned>;
                    Unsigned lhs_mag;
                    copy_abs_magnitude(lhs_mag, lhs, true);
                    result.data_[0] = lhs_mag.mod_small(positive_limb_divisor);
                    negate_for_division(result);
                    return result;
#        endif
                }

#        if GINT_DETAIL_AARCH64_GCC
                using u128 = unsigned __int128;
                const u128 lhs_raw = (static_cast<u128>(lhs.data_[1]) << 64) | lhs.data_[0];
                result.data_[0] = static_cast<limb_type>(lhs_raw % positive_limb_divisor);
#        else
                result.data_[0] = lhs.mod_small(positive_limb_divisor);
#        endif
                return result;
            }
        }
#    endif
#    if GINT_CLANG_TUNED_PATHS || GINT_ARCH_X86_64
        if (!(limbs == 2 && (GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG)))
        {
            limb_type positive_limb_divisor;
            if (positive_single_limb_value(rhs, positive_limb_divisor))
                return rem_by_positive_limb(lhs, positive_limb_divisor);
        }
#    endif
#    if GINT_GCC_TUNED_PATHS
        if (std::is_same<Signed, signed>::value)
        {
            const bool rhs_neg = rhs.data_[limbs - 1] >> 63;
#        if GINT_DETAIL_X86_64_GCC
            const bool lhs_neg = lhs.data_[limbs - 1] >> 63;
            if (!lhs_neg && !rhs_neg)
                return rem_unsigned_magnitude(lhs, rhs);
#        endif
            return rem_signed_magnitude(lhs, rhs, rhs_neg);
        }
        else
        {
            return rem_unsigned_magnitude(lhs, rhs);
        }
#    else
#        if GINT_CLANG_TUNED_PATHS
        if (limbs >= 4)
        {
            if (std::is_same<Signed, signed>::value)
            {
                const bool rhs_neg = rhs.data_[limbs - 1] >> 63;
                return rem_signed_magnitude(lhs, rhs, rhs_neg);
            }
            return rem_unsigned_magnitude_with_large_direct(lhs, rhs);
        }
#        endif
        integer q = lhs / rhs;
        q *= rhs;
        lhs -= q;
        return lhs;
#    endif
    }

    friend GINT_HIDDEN_VISIBILITY integer operator%(integer lhs, limb_type rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        if (needs_unsigned_signed_promotion(rhs))
            return truncate_promoted_signed(promote_signed_self(lhs) % promote_integral_value(rhs));
        if (rhs <= static_cast<limb_type>(std::numeric_limits<signed_limb_type>::max()))
            return lhs % static_cast<signed_limb_type>(rhs);
        return lhs % integer(rhs);
    }

    friend GINT_HIDDEN_VISIBILITY integer operator%(integer lhs, signed_limb_type rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        if (GINT_UNLIKELY(rhs == 0))
            return lhs;
        // For unsigned integers, mimic native casts: reinterpret negative divisors as their two's complement magnitude.
        if (std::is_same<Signed, unsigned>::value && rhs < 0)
            return lhs % integer(rhs);
        integer q;
        signed_limb_type r = lhs.div_mod_small(rhs, q);
        return integer(r);
    }

    friend GINT_HIDDEN_VISIBILITY integer operator%(limb_type lhs, integer rhs)
    {
        if (needs_unsigned_signed_promotion(lhs))
            return truncate_promoted_signed(promote_integral_value(lhs) % promote_signed_self(rhs));
        return integer(lhs) % rhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, T rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        if (needs_unsigned_signed_promotion(rhs))
            return truncate_promoted_signed(promote_signed_self(lhs) / promote_integral_value(rhs));
        if (sizeof(T) <= sizeof(limb_type)
            && (!detail::is_unsigned<T>::value || rhs <= static_cast<T>(std::numeric_limits<signed_limb_type>::max())))
            return lhs / static_cast<signed_limb_type>(rhs);
        return lhs / integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(T lhs, integer rhs)
    {
        if (needs_unsigned_signed_promotion(lhs))
            return truncate_promoted_signed(promote_integral_value(lhs) / promote_signed_self(rhs));
        return integer(lhs) / rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, T rhs)
    {
        if (std::isnan(rhs))
            GINT_THROW(std::domain_error("division by NaN"));
        if (std::isinf(rhs))
            return integer(); // finite / ±inf -> 0
        integer div(rhs);
        GINT_DIVZERO_CHECK(div.is_zero());
        return lhs / div;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(T lhs, integer rhs)
    {
        if (std::isnan(lhs))
            GINT_THROW(std::domain_error("division by NaN"));
        if (std::isinf(lhs))
            GINT_THROW(std::domain_error("infinite dividend"));
        GINT_DIVZERO_CHECK(rhs.is_zero());
        return integer(lhs) / rhs;
    } // LCOV_EXCL_LINE

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(integer lhs, T rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        if (GINT_UNLIKELY(rhs == 0))
            return lhs;
        if (needs_unsigned_signed_promotion(rhs))
            return truncate_promoted_signed(promote_signed_self(lhs) % promote_integral_value(rhs));
        // For unsigned integers, mimic native casts: reinterpret negative divisors as their two's complement magnitude.
        if (std::is_same<Signed, unsigned>::value && detail::is_signed<T>::value && rhs < 0)
            return lhs % integer(rhs);
        if (sizeof(T) <= sizeof(limb_type)
            && (!detail::is_unsigned<T>::value || rhs <= static_cast<T>(std::numeric_limits<signed_limb_type>::max())))
        {
            integer q;
            signed_limb_type r = lhs.div_mod_small(static_cast<signed_limb_type>(rhs), q);
            return integer(r);
        }
        return lhs % integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(T lhs, integer rhs)
    {
        if (needs_unsigned_signed_promotion(lhs))
            return truncate_promoted_signed(promote_integral_value(lhs) % promote_signed_self(rhs));
        return integer(lhs) % rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(integer lhs, T rhs)
    {
        if (std::isnan(rhs))
            GINT_THROW(std::domain_error("modulo by NaN"));
        if (std::isinf(rhs))
            return lhs; // finite % ±inf -> lhs
        integer div(rhs);
        GINT_MODZERO_CHECK(div.is_zero());
        return lhs % div;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(T lhs, integer rhs)
    {
        if (std::isnan(lhs))
            GINT_THROW(std::domain_error("modulo by NaN"));
        if (std::isinf(lhs))
            GINT_THROW(std::domain_error("infinite dividend in modulo"));
        return integer(lhs) % rhs;
    } // LCOV_EXCL_LINE

    friend constexpr bool operator==(const integer & lhs, const integer & rhs) noexcept
    {
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L && !GINT_GCC_TUNED_PATHS
        if (!__builtin_is_constant_evaluated() && limbs == 16)
            return detail::limbs_equal_runtime_1024(lhs.data_, rhs.data_);
#    endif
        return detail::limbs_equal<limbs - 1>::eval(lhs, rhs);
    }

    friend constexpr bool operator!=(const integer & lhs, const integer & rhs) noexcept { return !(lhs == rhs); }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend constexpr bool operator==(const integer & lhs, T rhs) noexcept
    {
        return needs_unsigned_signed_promotion(rhs) ? false : lhs == integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend constexpr bool operator==(T lhs, const integer & rhs) noexcept
    {
        return rhs == lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend constexpr bool operator!=(const integer & lhs, T rhs) noexcept
    {
        return !(lhs == rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend constexpr bool operator!=(T lhs, const integer & rhs) noexcept
    {
        return !(lhs == rhs);
    }

    // Compare a non-negative wide integer against a non-negative floating point
    // value by aligning exponents and comparing significands:
    // 1) Decompose rhs_abs = m * 2^e via frexp, with 0.5 <= m < 1.
    // 2) Let hb be the highest set bit of lhs_abs; compare hb with e-1. If they
    //    differ, the one with larger exponent wins.
    // 3) Align lhs_abs to the p-bit significand of T (p = digits), by shifting
    //    so that its top bit aligns to p-1. Mask out lower bits beyond p.
    // 4) Compare the p-bit significands; if equal, inspect fractional tail of
    //    rhs to decide strictness. If lhs has extra non-zero low bits when
    //    shifting back, it is considered larger.
    template <typename T>
    static int compare_with_float_abs(const integer & lhs_abs, T rhs_abs) noexcept
    {
        // Both are non-negative.
        if (lhs_abs.is_zero())
            return rhs_abs == T(0) ? 0 : -1;
        int e = 0;
        T m = std::frexp(rhs_abs, &e); // rhs_abs = m * 2^e, 0.5<=m<1
        if (m == T(0))
            return 1; // lhs_abs > 0 > rhs_abs
        int hb = lhs_abs.highest_bit();
        int k = e - 1; // index of highest set bit of rhs_abs
        if (hb != k)
            return hb < k ? -1 : 1;
        const int p = std::numeric_limits<T>::digits;
        int shift = hb - (p - 1);
        integer scaled = lhs_abs;
        unsigned __int128 sigA = 0;
        if (limbs >= 2)
            sigA = (static_cast<unsigned __int128>(lhs_abs.data_[1]) << 64) | lhs_abs.data_[0];
        else
            sigA = lhs_abs.data_[0];
        if (shift > 0)
        {
            scaled >>= shift;
            if (limbs >= 2)
                sigA = (static_cast<unsigned __int128>(scaled.data_[1]) << 64) | scaled.data_[0];
            else
                sigA = scaled.data_[0];
        }
        else if (shift < 0)
        {
            sigA <<= -shift;
        }
        if (p < 128)
            sigA &= ((static_cast<unsigned __int128>(1) << p) - 1);
        T scaled_rhs = std::ldexp(m, p);
        unsigned __int128 sigB = static_cast<unsigned __int128>(scaled_rhs);
        if (sigA < sigB)
            return -1;
        if (sigA > sigB)
            return 1;
        if (shift <= 0)
        {
            // If rhs has any fractional beyond p bits, then rhs > lhs.
            T frac = scaled_rhs - std::floor(scaled_rhs);
            return (frac > T(0)) ? -1 : 0;
        }
        else
        {
            integer rec = scaled;
            rec <<= shift;
            return (rec == lhs_abs) ? 0 : 1; // lhs has extra low bits -> larger
        }
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator==(const integer & lhs, T rhs) noexcept
    {
        if (std::isnan(rhs))
            return false;
        if (std::isinf(rhs))
            return false; // finite integer is never equal to ±inf
        if (rhs == T(0))
            return lhs.is_zero();
        bool lhs_neg = std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63);
        bool rhs_neg = std::signbit(rhs);
        if (lhs_neg != rhs_neg)
            return false;
        const integer lhs_abs = lhs_neg ? -lhs : lhs;
        const T rhs_abs = rhs_neg ? T(-rhs) : rhs;
        return compare_with_float_abs(lhs_abs, rhs_abs) == 0;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator==(T lhs, const integer & rhs) noexcept
    {
        return rhs == lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator!=(const integer & lhs, T rhs) noexcept
    {
        return !(lhs == rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator!=(T lhs, const integer & rhs) noexcept
    {
        return !(lhs == rhs);
    }

    GINT_CONSTEXPR14 friend bool operator<(const integer & lhs, const integer & rhs) noexcept
    {
        if (std::is_same<Signed, signed>::value)
        {
            bool lhs_neg = lhs.data_[limbs - 1] >> 63;
            bool rhs_neg = rhs.data_[limbs - 1] >> 63;
            if (lhs_neg != rhs_neg)
                return lhs_neg;
        }
        for (size_t i = limbs; i-- > 0;)
        {
            if (lhs.data_[i] != rhs.data_[i])
                return lhs.data_[i] < rhs.data_[i];
        }
        return false;
    }

    GINT_CONSTEXPR14 friend bool operator>(const integer & lhs, const integer & rhs) noexcept { return rhs < lhs; }

    GINT_CONSTEXPR14 friend bool operator<=(const integer & lhs, const integer & rhs) noexcept { return !(rhs < lhs); }

    GINT_CONSTEXPR14 friend bool operator>=(const integer & lhs, const integer & rhs) noexcept { return !(lhs < rhs); }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<(const integer & lhs, T rhs) noexcept
    {
        if (needs_unsigned_signed_promotion(rhs))
            return promote_signed_self(lhs) < promote_integral_value(rhs);
        return lhs < integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<(T lhs, const integer & rhs) noexcept
    {
        if (needs_unsigned_signed_promotion(lhs))
            return promote_integral_value(lhs) < promote_signed_self(rhs);
        return integer(lhs) < rhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>(const integer & lhs, T rhs) noexcept
    {
        return rhs < lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>(T lhs, const integer & rhs) noexcept
    {
        return rhs < lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<=(const integer & lhs, T rhs) noexcept
    {
        return !(rhs < lhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<=(T lhs, const integer & rhs) noexcept
    {
        return !(rhs < lhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>=(const integer & lhs, T rhs) noexcept
    {
        return !(lhs < rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>=(T lhs, const integer & rhs) noexcept
    {
        return !(lhs < rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<(const integer & lhs, T rhs) noexcept
    {
        if (std::isnan(rhs))
            return false;
        if (std::isinf(rhs))
            return rhs > 0; // lhs (finite) is always < +inf, never < -inf
        if (rhs == T(0))
            return (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63)) && !lhs.is_zero();
        bool lhs_neg = std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63);
        bool rhs_neg = std::signbit(rhs);
        if (lhs_neg != rhs_neg)
            return lhs_neg;
        integer lhs_abs = lhs_neg ? -lhs : lhs;
        T rhs_abs = rhs_neg ? T(-rhs) : rhs;
        int cmp = compare_with_float_abs(lhs_abs, rhs_abs);
        // When both are negative, ordering reverses on absolute values.
        return lhs_neg ? (cmp > 0) : (cmp < 0);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<(T lhs, const integer & rhs) noexcept
    {
        return rhs > lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>(const integer & lhs, T rhs) noexcept
    {
        if (std::isnan(rhs))
            return false;
        if (std::isinf(rhs))
            return rhs < 0; // lhs (finite) is always > -inf, never > +inf
        if (rhs == T(0))
            return !(std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63)) && !lhs.is_zero();
        bool lhs_neg = std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63);
        bool rhs_neg = std::signbit(rhs);
        if (lhs_neg != rhs_neg)
            return !lhs_neg;
        integer lhs_abs = lhs_neg ? -lhs : lhs;
        T rhs_abs = rhs_neg ? T(-rhs) : rhs;
        int cmp = compare_with_float_abs(lhs_abs, rhs_abs);
        // When both are negative, ordering reverses on absolute values.
        return lhs_neg ? (cmp < 0) : (cmp > 0);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>(T lhs, const integer & rhs) noexcept
    {
        return rhs < lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<=(const integer & lhs, T rhs) noexcept
    {
        if (std::isnan(rhs))
            return false;
        if (std::isinf(rhs))
            return rhs > 0; // lhs (finite) <= +inf; lhs (finite) <= -inf is false
        return !(lhs > rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<=(T lhs, const integer & rhs) noexcept
    {
        if (std::isnan(lhs))
            return false;
        return !(lhs > rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>=(const integer & lhs, T rhs) noexcept
    {
        if (std::isnan(rhs))
            return false;
        if (std::isinf(rhs))
            return rhs < 0; // lhs (finite) >= -inf; lhs (finite) >= +inf is false
        return !(lhs < rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>=(T lhs, const integer & rhs) noexcept
    {
        if (std::isnan(lhs))
            return false;
        return !(lhs < rhs);
    }

    GINT_CONSTEXPR14 friend integer operator~(integer v) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            v.data_[i] = ~v.data_[i];
        return v;
    }

    GINT_CONSTEXPR14 friend integer operator-(const integer & v) noexcept
    {
        integer res = ~v;
        limb_type carry = 1;
        for (size_t i = 0; i < limbs; ++i)
        {
            unsigned __int128 sum = static_cast<unsigned __int128>(res.data_[i]) + carry;
            res.data_[i] = static_cast<limb_type>(sum);
            carry = sum >> 64;
            if (!carry)
                break;
        }
        return res;
    }

    GINT_CONSTEXPR14 friend integer operator+(const integer & v) noexcept { return v; }

    friend std::string to_string<>(const integer & v);
    friend std::string detail::to_base_string<>(const integer & v, unsigned base, bool uppercase);
    template <size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> from_string(const std::string & text, unsigned base);
    template <size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> detail::parse_string_range(const char * begin, const char * end, unsigned base);
    template <unsigned BitsPerDigit, size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> detail::parse_power_of_two_range(const char * begin, const char * end);

private:
    GINT_FORCE_INLINE void mul_add_limb(limb_type multiplier, limb_type addend) noexcept
    {
        unsigned __int128 carry = addend;
        for (size_t i = 0; i < limbs; ++i)
        {
            const unsigned __int128 product = static_cast<unsigned __int128>(data_[i]) * multiplier + carry;
            data_[i] = static_cast<limb_type>(product);
            carry = product >> 64;
        }
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 void assign(T v) noexcept
    {
        typedef __int128 wide_signed;
        typedef unsigned __int128 wide_unsigned;
        const bool sign_fill = detail::is_signed<T>::value && v < 0;
        const limb_type fill = sign_fill ? ~limb_type(0) : limb_type(0);
        wide_unsigned val = sign_fill ? static_cast<wide_unsigned>(static_cast<wide_signed>(v)) : static_cast<wide_unsigned>(v);

        data_[0] = static_cast<limb_type>(val);
        val >>= 64;
        if (limbs > 1)
            data_[1] = static_cast<limb_type>(val);
        for (size_t i = 2; i < limbs; ++i)
            data_[i] = fill;
    }

    template <typename T>
    void assign_float(T v) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] = 0;
        if (v == 0 || std::isnan(v))
            return;
        if (std::isinf(v))
        {
            if (v > 0)
            {
                if (std::is_same<Signed, unsigned>::value)
                {
                    for (size_t i = 0; i < limbs; ++i)
                        data_[i] = ~limb_type(0);
                }
                else
                {
                    if (limbs > 1)
                    {
                        for (size_t i = 0; i < limbs - 1; ++i)
                            data_[i] = ~limb_type(0);
                    }
                    data_[limbs - 1] = (~limb_type(0)) >> 1;
                }
            }
            else // -inf
            {
                if (std::is_same<Signed, signed>::value)
                    data_[limbs - 1] = limb_type(1) << 63;
                // Unsigned negative infinity saturates to zero (already zeroed above)
            }
            return;
        }
        bool neg = v < 0;
        if (neg)
            v = -v;
        long double val = static_cast<long double>(v);
        long double intpart;
        std::modf(val, &intpart);
        val = intpart;
        long double base = std::ldexp(1.0L, 64);
        for (size_t i = 0; i < limbs && val > 0; ++i)
        {
            long double rem = std::fmod(val, base);
            data_[i] = static_cast<limb_type>(rem);
            val = std::floor(val / base);
        }
        if (neg)
            *this = -*this;
    }

    GINT_CONSTEXPR14 bool is_zero() const noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            if (data_[i] != 0)
                return false;
        return true;
    }

    int highest_bit() const noexcept
    {
        for (int i = static_cast<int>(limbs) - 1; i >= 0; --i)
        {
            if (data_[i])
                return i * 64 + 63 - __builtin_clzll(data_[i]);
        }
        return -1;
    }

    static bool test_bit(const integer & value, int bit) noexcept
    {
        return bit >= 0 && bit < static_cast<int>(Bits) && ((value.data_[static_cast<size_t>(bit) / 64] >> (bit % 64)) & 1);
    }

    static bool has_any_bit_below(const integer & value, int bit) noexcept
    {
        if (bit <= 0)
            return false;
        size_t full_limbs = static_cast<size_t>(bit) / 64;
        const unsigned rem = static_cast<unsigned>(bit % 64);
        if (full_limbs > limbs)
            full_limbs = limbs;
        for (size_t i = 0; i < full_limbs; ++i)
            if (value.data_[i])
                return true;
        if (full_limbs < limbs && rem != 0)
        {
            const limb_type mask = (limb_type(1) << rem) - 1;
            return (value.data_[full_limbs] & mask) != 0;
        }
        return false;
    }

    static limb_type low_limb_after_logical_right_shift(const integer & value, int shift) noexcept
    {
        const size_t limb_shift = static_cast<size_t>(shift) / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        if (limb_shift >= limbs)
            return 0;

        limb_type result = value.data_[limb_shift] >> bit_shift;
        if (bit_shift != 0 && limb_shift + 1 < limbs)
            result |= value.data_[limb_shift + 1] << (64 - bit_shift);
        return result;
    }

    static unsigned __int128 low_u128_after_logical_right_shift(const integer & value, int shift) noexcept
    {
        using u128 = unsigned __int128;
        const size_t limb_shift = static_cast<size_t>(shift) / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        if (limb_shift >= limbs)
            return 0;

        u128 result = static_cast<u128>(value.data_[limb_shift]) >> bit_shift;
        if (limb_shift + 1 < limbs)
            result |= static_cast<u128>(value.data_[limb_shift + 1]) << (64 - bit_shift);
        if (bit_shift != 0 && limb_shift + 2 < limbs)
            result |= static_cast<u128>(value.data_[limb_shift + 2]) << (128 - bit_shift);
        return result;
    }

    template <typename Float>
    static typename std::enable_if<(std::numeric_limits<Float>::digits < 64), limb_type>::type
    binary_float_significand(const integer & value, int shift) noexcept
    {
        return low_limb_after_logical_right_shift(value, shift);
    }

    template <typename Float>
    static typename std::enable_if<(std::numeric_limits<Float>::digits >= 64), unsigned __int128>::type
    binary_float_significand(const integer & value, int shift) noexcept
    {
        return low_u128_after_logical_right_shift(value, shift);
    }

    template <typename Float>
    Float to_binary_float() const noexcept
    {
        static_assert(std::numeric_limits<Float>::radix == 2, "floating-point conversion requires a binary radix");
        static_assert(std::numeric_limits<Float>::digits < 128, "floating-point conversion supports at most 127 significand bits");
        if (is_zero())
            return Float(0);

        const bool neg = std::is_same<Signed, signed>::value && (data_[limbs - 1] >> 63);
        integer mag = neg ? -*this : *this;
        const int hb = mag.highest_bit();
        const int digits = std::numeric_limits<Float>::digits;
        if (hb < digits)
        {
            Float res = 0;
            for (size_t i = limbs; i-- > 0;)
            {
                res = std::ldexp(res, 64);
                res += static_cast<Float>(mag.data_[i]);
            }
            return neg ? -res : res;
        }

        int scale = hb - (digits - 1);
        typedef typename std::conditional<(std::numeric_limits<Float>::digits < 64), limb_type, unsigned __int128>::type significand_type;
        significand_type significand = binary_float_significand<Float>(mag, scale);
        const bool guard = test_bit(mag, scale - 1);
        const bool sticky = has_any_bit_below(mag, scale - 1);
        const bool discarded = guard || sticky;
        bool increment = false;
        switch (std::fegetround())
        {
            case FE_TONEAREST:
                increment = guard && (sticky || (significand & 1));
                break;
            case FE_UPWARD:
                increment = !neg && discarded;
                break;
            case FE_DOWNWARD:
                increment = neg && discarded;
                break;
            case FE_TOWARDZERO:
                increment = false;
                break;
            default:
                increment = guard && (sticky || (significand & 1));
                break;
        }
        if (increment)
        {
            ++significand;
            if (significand == (significand_type(1) << digits))
            {
                significand >>= 1;
                ++scale;
            }
        }

        // Apply the sign before ldexp so directed overflow is rounded in the
        // direction of the signed result and still raises the usual fenv flags.
        const Float signed_significand = neg ? -static_cast<Float>(significand) : static_cast<Float>(significand);
        return std::ldexp(signed_significand, scale);
    }

    template <size_t L = limbs>
    typename std::enable_if<(L == 1), limb_type>::type div_mod_small(limb_type div, integer & quotient) const noexcept
    {
        // SFINAE provides a dedicated implementation for single-limb integers,
        // avoiding multi-limb code that would trigger -Warray-bounds warnings.
        quotient = integer();
        if (data_[0] == 0)
            return 0;
        quotient.data_[0] = static_cast<limb_type>(data_[0] / div);
        return static_cast<limb_type>(data_[0] % div);
    }

    template <size_t L = limbs>
    typename std::enable_if<(L == 1), limb_type>::type mod_small(limb_type div) const noexcept
    {
        return static_cast<limb_type>(data_[0] % div);
    }

    template <size_t L = limbs>
    typename std::enable_if<(L > 1), limb_type>::type div_mod_small(limb_type div, integer & quotient) const noexcept
    {
        using u128 = unsigned __int128;
        // This overload is only instantiated for multi-limb integers, preventing
        // compilers from inspecting out-of-bounds accesses in single-limb cases.
        quotient = integer();
        size_t n = limbs;
        while (n > 0 && data_[n - 1] == 0)
            --n;
        if (n == 0)
            return 0;
        // Power-of-two divisor becomes a simple shift/modulo by mask.
        if ((div & (div - 1)) == 0)
        {
            const unsigned s = static_cast<unsigned>(__builtin_ctzll(div));
            if (s == 0)
            {
                quotient = *this;
            }
            else
            {
                quotient = integer();
                const limb_type mask = (limb_type(1) << s) - 1;
                limb_type carry = 0;
                for (size_t i = limbs; i-- > 0;)
                {
                    const limb_type cur = data_[i];
                    quotient.data_[i] = (cur >> s) | (carry << (64 - s));
                    carry = cur & mask;
                }
            }
            return static_cast<limb_type>(data_[0] & (div - 1));
        }
#    if GINT_DETAIL_AARCH64_CLANG
        if (limbs == 2 && div > 0xFFFFFFFFULL)
        {
            const u128 num = (static_cast<u128>(data_[1]) << 64) | data_[0];
            const u128 q = num / div;
            quotient.data_[0] = static_cast<limb_type>(q);
            quotient.data_[1] = static_cast<limb_type>(q >> 64);
            return static_cast<limb_type>(num % div);
        }
#    endif
#    if GINT_ARCH_X86_64
#        if GINT_CLANG_TUNED_PATHS
        if (div != 10000000000000000000ULL)
#        endif
        {
            u128 rem = 0;
            for (size_t i = n; i-- > 0;)
            {
                u128 num = (rem << 64) | data_[i];
                quotient.data_[i] = static_cast<limb_type>(num / div);
                rem = num % div;
            }
            return static_cast<limb_type>(rem);
        }
#    endif
        // Fast path: 32-bit divisor using reciprocal-multiply in base 2^32.
        // Compute rinv = floor((2^64-1)/d32). For each 64-bit chunk T
        // (formed by (rem<<32)|word32), q_est = high64(T * rinv); correct by
        // at most +1 via a single branch. This avoids hardware division in
        // the loop and performs well across GCC/Clang.
        if (div <= 0xFFFFFFFFULL)
        {
            using u128x = unsigned __int128;
            const uint32_t d32 = static_cast<uint32_t>(div);
            const uint64_t rinv = ~uint64_t(0) / static_cast<uint64_t>(d32);
            uint64_t rem = 0; // always < d32
            for (size_t i = n; i-- > 0;)
            {
                const uint64_t cur = data_[i];
                const uint32_t hi = static_cast<uint32_t>(cur >> 32);
                const uint32_t lo = static_cast<uint32_t>(cur);

                // High 32 bits
                uint64_t t = (rem << 32) | hi;
                uint64_t qhi = static_cast<uint64_t>((u128x(t) * rinv) >> 64);
                uint64_t r = t - qhi * d32;
                if (r >= d32)
                {
                    ++qhi;
                    r -= d32;
                }

                // Low 32 bits
                t = (r << 32) | lo;
                uint64_t qlo = static_cast<uint64_t>((u128x(t) * rinv) >> 64);
                r = t - qlo * d32;
                if (r >= d32)
                {
                    ++qlo;
                    r -= d32;
                }

                rem = r;
                quotient.data_[i] = (static_cast<uint64_t>(qhi) << 32) | static_cast<uint32_t>(qlo);
            }
            return static_cast<limb_type>(rem);
        }
        // 64-bit divisors: two viable strategies exist.
        // We observed broadly good cross-compiler results by using the
        // reciprocal-multiply estimate with one correction on modern GCC/Clang.
        // However, Clang on some older Linux toolchains may favor 128/64
        // divisions. After experimentation, we combine both ideas by using the
        // reciprocal path but keep the code structure tight and inlined.
        const u128 inv = static_cast<u128>(~static_cast<u128>(0)) / static_cast<u128>(div);
        // Single branch correction was the most stable variant in local compiler tests.
        auto corr = [&](u128 & q, u128 & rem)
        {
            if (rem >= div)
            {
                ++q;
                rem -= div;
            }
        };
        // Unroll for common 256-bit case (4 limbs) to reduce loop overhead
        if (limbs == 4)
        {
            switch (n)
            {
                case 1: {
                    u128 num = data_[0];
                    u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
                    u128 rem = num - q * div;
                    corr(q, rem);
                    quotient.data_[0] = static_cast<limb_type>(q);
                    return static_cast<limb_type>(rem);
                }
                case 2: {
                    u128 num = (static_cast<u128>(data_[1]) << 64) | data_[0];
                    u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
                    u128 rem = num - q * div;
                    corr(q, rem);
                    quotient.data_[0] = static_cast<limb_type>(q);
                    quotient.data_[1] = static_cast<limb_type>(q >> 64);
                    return static_cast<limb_type>(rem);
                }
                case 3: {
                    u128 rem = 0;
                    u128 num = (rem << 64) | data_[2];
                    u128 q2 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q2 * div;
                    corr(q2, rem);
                    quotient.data_[2] = static_cast<limb_type>(q2);
                    num = (rem << 64) | data_[1];
                    u128 q1 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q1 * div;
                    corr(q1, rem);
                    quotient.data_[1] = static_cast<limb_type>(q1);
                    num = (rem << 64) | data_[0];
                    u128 q0 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q0 * div;
                    corr(q0, rem);
                    quotient.data_[0] = static_cast<limb_type>(q0);
                    return static_cast<limb_type>(rem);
                }
                case 4:
                default: {
                    u128 rem = 0;
                    u128 num = (rem << 64) | data_[3];
                    u128 q3 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q3 * div;
                    corr(q3, rem);
                    quotient.data_[3] = static_cast<limb_type>(q3);
                    num = (rem << 64) | data_[2];
                    u128 q2 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q2 * div;
                    corr(q2, rem);
                    quotient.data_[2] = static_cast<limb_type>(q2);
                    num = (rem << 64) | data_[1];
                    u128 q1 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q1 * div;
                    corr(q1, rem);
                    quotient.data_[1] = static_cast<limb_type>(q1);
                    num = (rem << 64) | data_[0];
                    u128 q0 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q0 * div;
                    corr(q0, rem);
                    quotient.data_[0] = static_cast<limb_type>(q0);
                    return static_cast<limb_type>(rem);
                }
            }
        }
        // Generic path for other limb counts
        u128 rem = 0;
        for (size_t i = n; i-- > 0;)
        {
            u128 num = (rem << 64) | data_[i];
            u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
            rem = num - q * div;
            corr(q, rem);
            quotient.data_[i] = static_cast<limb_type>(q);
        }
        return static_cast<limb_type>(rem);
    }

    template <size_t L = limbs>
    typename std::enable_if<(L > 1), limb_type>::type mod_small(limb_type div) const noexcept
    {
        using u128 = unsigned __int128;
        size_t n = limbs;
        while (n > 0 && data_[n - 1] == 0)
            --n;
        if (n == 0)
            return 0;

        if ((div & (div - 1)) == 0)
            return static_cast<limb_type>(data_[0] & (div - 1));

#    if GINT_DETAIL_AARCH64_CLANG || GINT_DETAIL_AARCH64_GCC
        if (limbs == 2 && div > 0xFFFFFFFFULL)
        {
            const u128 num = (static_cast<u128>(data_[1]) << 64) | data_[0];
            return static_cast<limb_type>(num % div);
        }
#    endif

#    if GINT_ARCH_X86_64
        {
            u128 rem = 0;
            for (size_t i = n; i-- > 0;)
                rem = ((rem << 64) | data_[i]) % div;
            return static_cast<limb_type>(rem);
        }
#    endif

        if (div <= 0xFFFFFFFFULL)
        {
            using u128x = unsigned __int128;
            const uint32_t d32 = static_cast<uint32_t>(div);
            const uint64_t rinv = ~uint64_t(0) / static_cast<uint64_t>(d32);
            uint64_t rem = 0;
            for (size_t i = n; i-- > 0;)
            {
                const uint64_t cur = data_[i];
                const uint32_t hi = static_cast<uint32_t>(cur >> 32);
                const uint32_t lo = static_cast<uint32_t>(cur);

                uint64_t t = (rem << 32) | hi;
                uint64_t q = static_cast<uint64_t>((u128x(t) * rinv) >> 64);
                uint64_t r = t - q * d32;
                if (r >= d32)
                    r -= d32;

                t = (r << 32) | lo;
                q = static_cast<uint64_t>((u128x(t) * rinv) >> 64);
                r = t - q * d32;
                if (r >= d32)
                    r -= d32;
                rem = r;
            }
            return static_cast<limb_type>(rem);
        }

        const u128 inv = static_cast<u128>(~static_cast<u128>(0)) / static_cast<u128>(div);
        auto corr = [&](u128 & q, u128 & rem)
        {
            if (rem >= div)
            {
                ++q;
                rem -= div;
            }
        };

        u128 rem = 0;
        for (size_t i = n; i-- > 0;)
        {
            u128 num = (rem << 64) | data_[i];
            u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
            rem = num - q * div;
            corr(q, rem);
        }
        return static_cast<limb_type>(rem);
    }


    signed_limb_type div_mod_small(signed_limb_type div, integer & quotient) const noexcept
    {
        integer tmp = *this;
        bool lhs_neg = false;
        if (std::is_same<Signed, signed>::value && (tmp.data_[limbs - 1] >> 63))
        {
            negate_for_division(tmp);
            lhs_neg = true;
        }
        bool div_neg = div < 0;
        limb_type abs_div = static_cast<limb_type>(div);
        if (div_neg)
            abs_div = limb_type(0) - abs_div;
        signed_limb_type rem = static_cast<signed_limb_type>(tmp.div_mod_small(abs_div, quotient));
        if (lhs_neg)
            rem = -rem;
        if (lhs_neg != div_neg)
            negate_for_division(quotient);
        return rem;
    }

    static GINT_HIDDEN_VISIBILITY integer
    div_unsigned_path(const integer & lhs_value, const integer & rhs_value, bool lhs_neg, bool rhs_neg, bool lhs_is_min, bool rhs_is_min)
    {
        using Unsigned = integer<Bits, unsigned>;
        Unsigned lhs_mag;
        Unsigned divisor_mag;

        if (lhs_is_min)
        {
            // Build the absolute magnitude directly; negating two's-complement min would keep the sign bit set.
            for (size_t i = 0; i + 1 < limbs; ++i)
                lhs_mag.data_[i] = 0;
            lhs_mag.data_[limbs - 1] = static_cast<limb_type>(1ULL << 63);
        }
        else
        {
            for (size_t i = 0; i < limbs; ++i)
                lhs_mag.data_[i] = lhs_value.data_[i];
        }

        if (rhs_is_min)
        {
            // Copy the min-value magnitude directly so the unsigned division path can be reused.
            for (size_t i = 0; i + 1 < limbs; ++i)
                divisor_mag.data_[i] = 0;
            divisor_mag.data_[limbs - 1] = static_cast<limb_type>(1ULL << 63);
        }
        else
        {
            for (size_t i = 0; i < limbs; ++i)
                divisor_mag.data_[i] = rhs_value.data_[i];
        }

        Unsigned quotient_mag;
        size_t divisor_limbs = Unsigned::used_limbs(divisor_mag);
        GINT_DIVZERO_CHECK(divisor_limbs == 0);
        if (divisor_limbs == 1)
        {
            lhs_mag.div_mod_small(divisor_mag.data_[0], quotient_mag);
        }
        else
        {
            int pow_bit;
            if (Unsigned::is_power_of_two(divisor_mag, pow_bit))
            {
                quotient_mag = lhs_mag >> pow_bit;
            }
            else if (limbs == 2)
            {
                quotient_mag = Unsigned::div_128(lhs_mag, divisor_mag);
            }
            else if (divisor_limbs == 2)
            {
                quotient_mag = Unsigned::div_large_2(lhs_mag, divisor_mag);
            }
            else if (divisor_limbs == 3)
            {
                quotient_mag = Unsigned::div_large_3(lhs_mag, divisor_mag);
            }
            else
            {
                quotient_mag = Unsigned::div_large(lhs_mag, divisor_mag, divisor_limbs);
            }
        }

        integer result;
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = quotient_mag.data_[i];

        if (lhs_neg != rhs_neg)
            negate_for_division(result);
        return result;
    } // LCOV_EXCL_LINE

    static size_t used_limbs(const integer & v) noexcept
    {
        size_t n = limbs;
        while (n > 0 && v.data_[n - 1] == 0)
            --n;
        return n;
    }

    static bool positive_single_limb_value(const integer & v, limb_type & value) noexcept
    {
        value = v.data_[0];
        if (std::is_same<Signed, signed>::value && limbs == 1 && (value >> 63))
            return false;
        if (value == 0)
            return false;
        if (limbs == 1)
            return true;
        if (limbs == 2)
            return v.data_[1] == 0;
        if (limbs == 4)
            return v.data_[3] == 0 && v.data_[2] == 0 && v.data_[1] == 0;

        limb_type high_or = 0;
        for (size_t i = 1; i < limbs; ++i)
            high_or |= v.data_[i];
        return high_or == 0;
    }

    static constexpr size_t promoted_signed_bits = Bits < 256 ? 256 : Bits;
    using promoted_signed_type = integer<promoted_signed_bits, signed>;

    template <typename T>
    static constexpr bool needs_unsigned_signed_promotion(T value) noexcept
    {
        return std::is_same<Signed, signed>::value && detail::is_unsigned<T>::value && Bits < 256
            && static_cast<unsigned __int128>(value) >= detail::signed_promotion_limit<Bits>::value();
    }

    static GINT_CONSTEXPR14 promoted_signed_type promote_signed_self(const integer & value) noexcept
    {
        promoted_signed_type result;
        const limb_type fill = std::is_same<Signed, signed>::value && (value.data_[limbs - 1] >> 63) ? ~limb_type(0) : limb_type(0);
        for (size_t i = 0; i < promoted_signed_type::limbs; ++i)
            result.data_[i] = i < limbs ? value.data_[i] : fill;
        return result;
    }

    template <typename T>
    static GINT_CONSTEXPR14 promoted_signed_type promote_integral_value(T value) noexcept
    {
        return promoted_signed_type(value);
    }

    static GINT_CONSTEXPR14 integer truncate_promoted_signed(const promoted_signed_type & value) noexcept
    {
        integer result;
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = value.data_[i];
        return result;
    }

    static GINT_FORCE_INLINE void negate_in_place(integer & v) noexcept
    {
        limb_type carry = 1;
        for (size_t i = 0; i < limbs; ++i)
        {
            const limb_type inv = ~v.data_[i];
            const limb_type sum = inv + carry;
            v.data_[i] = sum;
            carry = carry && (sum == 0);
        }
    }

    static GINT_FORCE_INLINE void negate_for_division(integer & v) noexcept { negate_in_place(v); }

    static bool positive_power_of_two_value(const integer & v, int & bit_index) noexcept
    {
        if (std::is_same<Signed, signed>::value && (v.data_[limbs - 1] >> 63))
            return false;
        return is_power_of_two(v, bit_index);
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L >= 16), bool>::type
    positive_power_of_two_fastpath_divisor(const integer & v, int & bit_index) noexcept
    {
        return v.data_[0] == 0 && positive_power_of_two_value(v, bit_index);
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L < 16 && L != 2), bool>::type
    positive_power_of_two_fastpath_divisor(const integer &, int &) noexcept
    {
        return false;
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L == 2), bool>::type
    positive_power_of_two_fastpath_divisor(const integer & v, int & bit_index) noexcept
    {
#    if GINT_DETAIL_AARCH64_CLANG
        return positive_power_of_two_value(v, bit_index);
#    else
        (void)v;
        (void)bit_index;
        return false;
#    endif
    }

    static GINT_FORCE_INLINE integer div_by_positive_power_of_two(integer lhs, int pow_bit) noexcept
    {
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
            const limb_type min_magnitude = static_cast<limb_type>(1ULL << 63);
            if (GINT_UNLIKELY(lhs.data_[limbs - 1] == min_magnitude && is_min_value(lhs)))
                return div_unsigned_path(lhs, integer(1) << pow_bit, true, false, true, false);
            negate_for_division(lhs);
            integer result = lhs >> pow_bit;
            negate_for_division(result);
            return result;
        }
        return lhs >> pow_bit;
    }

    template <size_t L = limbs>
    static GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR typename std::enable_if<(L == 2), bool>::type
    negative_negative_div_quotient_is_zero(const integer & lhs, const integer & rhs) noexcept
    {
#    if GINT_ARCH_AARCH64
        // For two negative two's-complement values, larger raw bits mean smaller magnitude.
        unsigned result;
        __asm__("cmp %[lhs_hi], %[rhs_hi]\n"
                "ccmp %[lhs_lo], %[rhs_lo], #0, eq\n"
                "cset %w[result], hi"
                : [result] "=r"(result)
                : [lhs_hi] "r"(lhs.data_[1]), [rhs_hi] "r"(rhs.data_[1]), [lhs_lo] "r"(lhs.data_[0]), [rhs_lo] "r"(rhs.data_[0])
                : "cc");
        return result != 0;
#    else
        return lhs.data_[1] > rhs.data_[1] || (lhs.data_[1] == rhs.data_[1] && lhs.data_[0] > rhs.data_[0]);
#    endif
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L != 2), bool>::type
    negative_negative_div_quotient_is_zero(const integer &, const integer &) noexcept
    {
        return false;
    }

    template <size_t L = limbs, typename std::enable_if<!(GINT_DETAIL_AARCH64_GCC && L == 2), int>::type = 0>
    static integer div_by_positive_limb(integer lhs, limb_type divisor) noexcept
    {
        integer result;
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
#    if GINT_DETAIL_AARCH64_CLANG
            if (GINT_UNLIKELY(divisor > 0xFFFFFFFFULL && (divisor & (divisor - 1)) != 0)
                && div_signed_int128_by_positive_limb(lhs, divisor, result))
                return result;
#    endif
            negate_for_division(lhs);
            lhs.div_mod_small(divisor, result);
            negate_for_division(result);
            return result;
        }

        lhs.div_mod_small(divisor, result);
        return result;
    }

    template <size_t L = limbs, typename std::enable_if<(GINT_DETAIL_AARCH64_GCC && L == 2), int>::type = 0>
    static integer div_by_positive_limb(integer lhs, limb_type divisor) noexcept
    {
        integer result;
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
#    if GINT_DETAIL_AARCH64_CLANG
            if (GINT_UNLIKELY(divisor > 0xFFFFFFFFULL && (divisor & (divisor - 1)) != 0)
                && div_signed_int128_by_positive_limb(lhs, divisor, result))
                return result;
#    endif
            negate_for_division(lhs);
            lhs.div_mod_small(divisor, result);
            negate_for_division(result);
            return result;
        }

        if (divisor > 0xFFFFFFFFULL && (divisor & (divisor - 1)) != 0)
            return div_unsigned_int128_by_positive_limb(lhs, divisor);

        lhs.div_mod_small(divisor, result);
        return result;
    }

    template <size_t L = limbs, typename std::enable_if<(L == 2), int>::type = 0>
    static GINT_FORCE_INLINE integer div_unsigned_int128_by_positive_limb(const integer & lhs, limb_type divisor) noexcept
    {
        using u128 = unsigned __int128;
        const u128 lhs_raw = (static_cast<u128>(lhs.data_[1]) << 64) | lhs.data_[0];
        const u128 quotient = lhs_raw / divisor;
        integer result;
        result.data_[0] = static_cast<limb_type>(quotient);
        result.data_[1] = static_cast<limb_type>(quotient >> 64);
        return result;
    }

    template <size_t L = limbs, typename std::enable_if<(L == 2 && std::is_same<Signed, signed>::value), int>::type = 0>
    static GINT_FORCE_INLINE bool div_signed_int128_by_positive_limb(const integer & lhs, limb_type divisor, integer & result) noexcept
    {
        using u128 = unsigned __int128;
        using s128 = __int128;
        const u128 lhs_raw = (static_cast<u128>(lhs.data_[1]) << 64) | lhs.data_[0];
        const s128 quotient = static_cast<s128>(lhs_raw) / static_cast<s128>(divisor);
        const u128 quotient_raw = static_cast<u128>(quotient);
        result.data_[0] = static_cast<limb_type>(quotient_raw);
        result.data_[1] = static_cast<limb_type>(quotient_raw >> 64);
        return true;
    }

    template <size_t L = limbs, typename std::enable_if<(L != 2 || !std::is_same<Signed, signed>::value), int>::type = 0>
    static GINT_FORCE_INLINE bool div_signed_int128_by_positive_limb(const integer &, limb_type, integer &) noexcept
    {
        return false;
    }

    template <size_t L = limbs, typename std::enable_if<(L == 2), int>::type = 0>
    static GINT_NOINLINE integer rem_negative_int128_by_positive_limb(const integer & lhs, limb_type divisor) noexcept
    {
        integer result;
        using Unsigned = integer<Bits, unsigned>;
        Unsigned lhs_mag;
        copy_abs_magnitude(lhs_mag, lhs, true);
        result.data_[0] = lhs_mag.mod_small(divisor);
        negate_for_division(result);
        return result;
    }

    template <size_t L = limbs, typename std::enable_if<(L != 2), int>::type = 0>
    static GINT_NOINLINE integer rem_negative_int128_by_positive_limb(const integer &, limb_type) noexcept
    {
        return integer();
    }

    static integer rem_by_positive_limb(const integer & lhs, limb_type divisor) noexcept
    {
        integer result;
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
            using Unsigned = integer<Bits, unsigned>;
            Unsigned lhs_mag;
            copy_abs_magnitude(lhs_mag, lhs, true);
            result.data_[0] = lhs_mag.mod_small(divisor);
            negate_for_division(result);
            return result;
        }

        result.data_[0] = lhs.mod_small(divisor);
        return result;
    }

    template <typename SrcInt>
    static void copy_abs_magnitude(integer<Bits, unsigned> & dst, const SrcInt & src, bool neg) noexcept
    {
        if (!neg)
        {
            for (size_t i = 0; i < limbs; ++i)
                dst.data_[i] = src.data_[i];
            return;
        }

        limb_type carry = 1;
        for (size_t i = 0; i < limbs; ++i)
        {
            const limb_type inv = ~src.data_[i];
            const limb_type sum = inv + carry;
            dst.data_[i] = sum;
            carry = carry && (sum == 0);
        }
    }

    static GINT_FORCE_INLINE integer rem_signed_magnitude(const integer & lhs, const integer & rhs, bool rhs_neg) noexcept
    {
        using Unsigned = integer<Bits, unsigned>;
        const bool lhs_neg = lhs.data_[limbs - 1] >> 63;
        Unsigned lhs_mag(typename Unsigned::uninitialized_tag{});
        Unsigned rhs_mag(typename Unsigned::uninitialized_tag{});
        copy_abs_magnitude(lhs_mag, lhs, lhs_neg);
        copy_abs_magnitude(rhs_mag, rhs, rhs_neg);

#    if GINT_DETAIL_AARCH64_GCC
        Unsigned rem_mag = Unsigned::rem_unsigned_magnitude_with_large_direct(lhs_mag, rhs_mag);
#    elif GINT_CLANG_TUNED_PATHS
        Unsigned rem_mag = rem_signed_magnitude_unsigned(lhs_mag, rhs_mag);
#    else
        Unsigned rem_mag = Unsigned::rem_unsigned_magnitude(lhs_mag, rhs_mag);
#    endif
        integer result(uninitialized_tag{});
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = rem_mag.data_[i];
        if (lhs_neg)
            negate_for_division(result);
        return result;
    }

#    if GINT_CLANG_TUNED_PATHS
    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L >= 8), integer<Bits, unsigned>>::type
    rem_signed_magnitude_unsigned(const integer<Bits, unsigned> & lhs_mag, const integer<Bits, unsigned> & rhs_mag) noexcept
    {
        return integer<Bits, unsigned>::rem_unsigned_magnitude_with_large_direct(lhs_mag, rhs_mag);
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L < 8), integer<Bits, unsigned>>::type
    rem_signed_magnitude_unsigned(const integer<Bits, unsigned> & lhs_mag, const integer<Bits, unsigned> & rhs_mag) noexcept
    {
        return integer<Bits, unsigned>::rem_unsigned_magnitude(lhs_mag, rhs_mag);
    }
#    endif

    static integer rem_unsigned_magnitude(const integer & lhs, const integer & divisor) noexcept
    {
        integer result(uninitialized_tag{});
        size_t divisor_limbs = used_limbs(divisor);

        if (divisor_limbs == 1)
        {
            for (size_t i = 1; i < limbs; ++i)
                result.data_[i] = 0;
            result.data_[0] = lhs.mod_small(divisor.data_[0]);
            return result;
        }

        int pow_bit;
        if (is_power_of_two(divisor, pow_bit))
        {
            result = lhs & (divisor - integer(1));
            return result;
        }

#    if GINT_DETAIL_X86_64_GCC || GINT_DETAIL_AARCH64_CLANG
        if (limbs == 4 && divisor_limbs == 4)
            return rem_large_4(lhs, divisor);
#    endif

        integer quotient;
        if (limbs == 2)
#    if GINT_DETAIL_AARCH64_GCC
            quotient = div_128_native(lhs, divisor);
#    else
            quotient = div_128(lhs, divisor);
#    endif
        else if (divisor_limbs == 2)
            quotient = div_large_2(lhs, divisor);
        else if (divisor_limbs == 3)
            quotient = div_large_3(lhs, divisor);
        else if (limbs == 4 && divisor_limbs == 4)
            quotient = div_large_4(lhs, divisor);
        else
            quotient = div_large(lhs, divisor, divisor_limbs);

        result = lhs;
#    if GINT_GCC_TUNED_PATHS
        if (limbs == 4 && quotient.data_[2] == 0 && quotient.data_[3] == 0)
        {
            integer product(uninitialized_tag{});
            if (quotient.data_[1] == 0)
                detail::mul_limbs4_by_limb(product.data_, divisor.data_, quotient.data_[0]);
            else
                detail::mul_limbs4_by_2limb(product.data_, divisor.data_, quotient.data_[0], quotient.data_[1]);
            result -= product;
            return result;
        }
#    endif
        quotient *= divisor;
        result -= quotient;
        return result;
    }

    static integer rem_unsigned_magnitude_with_large_direct(const integer & lhs, const integer & divisor) noexcept
    {
#    if GINT_DETAIL_AARCH64_GCC || GINT_CLANG_TUNED_PATHS
        if (((GINT_DETAIL_AARCH64_GCC && limbs >= 4) || (GINT_CLANG_TUNED_PATHS && limbs >= 8))
            && GINT_UNLIKELY((divisor.data_[limbs - 1] | divisor.data_[limbs - 2]) != 0))
        {
            const size_t divisor_limbs = used_limbs(divisor);
            int pow_bit;
            if (is_power_of_two(divisor, pow_bit))
                return lhs & (divisor - integer(1));
            return rem_large(lhs, divisor, divisor_limbs);
        }
#    endif
        return rem_unsigned_magnitude(lhs, divisor);
    }

    static bool is_min_value(const integer & v) noexcept
    {
        if (!std::is_same<Signed, signed>::value)
            return false;
        if (v.data_[limbs - 1] != static_cast<limb_type>(1ULL << 63))
            return false;
        for (size_t i = 0; i + 1 < limbs; ++i)
        {
            if (v.data_[i] != 0)
                return false;
        }
        return true;
    }

    static bool is_power_of_two(const integer & v, int & bit_index) noexcept
    {
        bit_index = -1;
        bool found = false;
        for (size_t i = 0; i < limbs; ++i)
        {
            limb_type limb = v.data_[i];
            if (limb)
            {
                if (limb & (limb - 1))
                    return false;
                if (found)
                    return false;
                bit_index = static_cast<int>(i * 64 + __builtin_ctzll(limb));
                found = true;
            }
        }
        return found;
    }

    // Left-shift an array of limbs by 'shift' bits (0..63) into dst, returning the carry-out limb.
    // The source and destination may alias; the operation proceeds from low to high.
    GINT_FORCE_INLINE static limb_type lshift_limbs_to(const limb_type * src, size_t n, limb_type * dst, int shift) noexcept
    {
        limb_type carry = 0;
        if (shift)
        {
            for (size_t i = 0; i < n; ++i)
            {
                limb_type cur = src[i];
                dst[i] = (cur << shift) | carry;
                carry = static_cast<limb_type>(cur >> (64 - shift));
            }
        }
        else
        {
            for (size_t i = 0; i < n; ++i)
                dst[i] = src[i];
            carry = 0;
        }
        return carry;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), integer>::type div_128(const integer & lhs, const integer & rhs) noexcept
    {
#    if GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG
        integer result;
        if (GINT_UNLIKELY((rhs.data_[1] | rhs.data_[0]) == 0))
            return result;
        if (rhs.data_[1] >= (limb_type(1) << 62))
        {
            limb_type rem_hi = lhs.data_[1];
            limb_type rem_lo = lhs.data_[0];
            limb_type q = 0;
            for (limb_type i = 0; i < 3; ++i)
            {
                if (rem_hi < rhs.data_[1] || (rem_hi == rhs.data_[1] && rem_lo < rhs.data_[0]))
                    break;
                limb_type next_lo = rem_lo - rhs.data_[0];
                limb_type borrow = rem_lo < rhs.data_[0];
                rem_hi = rem_hi - rhs.data_[1] - borrow;
                rem_lo = next_lo;
                ++q;
            }
            result.data_[0] = q;
            result.data_[1] = 0;
            return result;
        }
#    endif
        return div_128_native(lhs, rhs);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), integer>::type div_128_native(const integer & lhs, const integer & rhs) noexcept
    {
        unsigned __int128 a = (static_cast<unsigned __int128>(lhs.data_[1]) << 64) | lhs.data_[0];
        unsigned __int128 b = (static_cast<unsigned __int128>(rhs.data_[1]) << 64) | rhs.data_[0];
        integer result;
        if (GINT_UNLIKELY(b == 0))
            return result;
        unsigned __int128 q = a / b;
        result.data_[0] = static_cast<limb_type>(q);
        result.data_[1] = static_cast<limb_type>(q >> 64);
        return result;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), integer>::type div_128(const integer & lhs, const integer & rhs) noexcept
    {
        return div_128_native(lhs, rhs);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), integer>::type div_128_native(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
        if (GINT_UNLIKELY(rhs.data_[0] == 0))
            return result;
        result.data_[0] = lhs.data_[0] / rhs.data_[0];
        return result;
    }

#    if GINT_GCC_TUNED_PATHS
    GINT_FORCE_INLINE static limb_type left_shifted_limb_at(const limb_type * src, size_t i, int shift) noexcept
    {
        limb_type cur = src[i];
        if (shift == 0)
            return cur;
        limb_type prev = i == 0 ? 0 : src[i - 1];
        return static_cast<limb_type>((cur << shift) | (prev >> (64 - shift)));
    }

    static bool mul_by_limb_greater_than(const integer & lhs, const integer & divisor, size_t div_limbs, limb_type q) noexcept
    {
        if (q == 0)
            return false;

        std::array<limb_type, limbs + 1> product;
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor.data_[i]) * q + carry;
            product[i] = static_cast<limb_type>(p);
            carry = p >> 64;
        }
        if (carry != 0)
            return true;

        for (size_t i = div_limbs; i-- > 0;)
        {
            if (product[i] != lhs.data_[i])
                return product[i] > lhs.data_[i];
        }
        return false;
    }

    static integer div_large_single_limb_quotient(const integer & lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        integer quotient;
        if (div_limbs < 2)
            return quotient;

        using u128 = unsigned __int128;
        const int shift = __builtin_clzll(divisor.data_[div_limbs - 1]);
        const limb_type vtop = left_shifted_limb_at(divisor.data_, div_limbs - 1, shift);
        const limb_type vnext = left_shifted_limb_at(divisor.data_, div_limbs - 2, shift);
        const limb_type utop = shift ? static_cast<limb_type>(lhs.data_[div_limbs - 1] >> (64 - shift)) : 0;
        const limb_type unext = left_shifted_limb_at(lhs.data_, div_limbs - 1, shift);
        const limb_type uthird = left_shifted_limb_at(lhs.data_, div_limbs - 2, shift);

        u128 numerator = (static_cast<u128>(utop) << 64) | unext;
        u128 qhat = numerator / vtop;
        u128 rhat = numerator - qhat * vtop;
        while (qhat == (static_cast<u128>(1) << 64) || qhat * vnext > ((rhat << 64) | uthird))
        {
            --qhat;
            rhat += vtop;
            if (rhat >= (static_cast<u128>(1) << 64))
                break;
        }

        limb_type q = static_cast<limb_type>(qhat);
        while (mul_by_limb_greater_than(lhs, divisor, div_limbs, q))
            --q;
        quotient.data_[0] = q;
        return quotient;
    }
#    endif

    template <bool WantRemainder>
    static GINT_NOINLINE integer div_or_rem_large_core(integer lhs, const integer & divisor, size_t v_limbs, size_t u_limbs) noexcept
    {
        integer result;
        if (GINT_UNLIKELY(v_limbs == 0) || u_limbs < v_limbs)
            return WantRemainder ? lhs : result;

        std::array<limb_type, limbs + 1> u;
        std::array<limb_type, limbs + 1> v;

        int shift = __builtin_clzll(divisor.data_[v_limbs - 1]);
        limb_type carry = lshift_limbs_to(lhs.data_, u_limbs, u.data(), shift);
        u[u_limbs] = carry;

        carry = lshift_limbs_to(divisor.data_, v_limbs, v.data(), shift);

        for (int j = static_cast<int>(u_limbs - v_limbs); j >= 0; --j)
        {
            unsigned __int128 numerator = (static_cast<unsigned __int128>(u[j + v_limbs]) << 64) | u[j + v_limbs - 1];
            // Single 128/64 division: compute quotient, derive remainder by multiply-back
            unsigned __int128 qhat = numerator / v[v_limbs - 1];
            unsigned __int128 rhat = numerator - qhat * v[v_limbs - 1];

            if (v_limbs > 1)
            {
                while (qhat == (static_cast<unsigned __int128>(1) << 64) || qhat * v[v_limbs - 2] > ((rhat << 64) | u[j + v_limbs - 2]))
                {
                    --qhat;
                    rhat += v[v_limbs - 1];
                    if (rhat >= (static_cast<unsigned __int128>(1) << 64))
                        break;
                }
            }

            unsigned __int128 borrow = 0;
            for (size_t i = 0; i < v_limbs; ++i)
            {
                unsigned __int128 p = qhat * v[i] + borrow;
                if (u[j + i] < static_cast<limb_type>(p))
                {
                    u[j + i] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + i]) - p);
                    borrow = (p >> 64) + 1;
                }
                else
                {
                    u[j + i] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + i]) - p);
                    borrow = p >> 64;
                }
            }
            if (static_cast<unsigned __int128>(u[j + v_limbs]) < borrow)
            {
                unsigned __int128 carry2 = 0;
                for (size_t i = 0; i < v_limbs; ++i)
                {
                    unsigned __int128 t2 = static_cast<unsigned __int128>(u[j + i]) + v[i] + carry2;
                    u[j + i] = static_cast<limb_type>(t2);
                    carry2 = t2 >> 64;
                }
                u[j + v_limbs] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + v_limbs]) + carry2);
                --qhat;
            }
            else
            {
                u[j + v_limbs] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + v_limbs]) - borrow);
            }
            if (!WantRemainder)
                result.data_[j] = static_cast<limb_type>(qhat);
        }

        if (WantRemainder)
        {
            if (shift == 0)
            {
                for (size_t i = 0; i < v_limbs; ++i)
                    result.data_[i] = u[i];
            }
            else
            {
                const int inv_shift = 64 - shift;
                for (size_t i = 0; i < v_limbs; ++i)
                {
                    const limb_type next = (i + 1 < v_limbs) ? u[i + 1] : 0;
                    result.data_[i] = (u[i] >> shift) | (next << inv_shift);
                }
            }
        }
        return result;
    }

    static integer div_large(integer lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        const size_t dividend_limbs = used_limbs(lhs);
#    if GINT_GCC_TUNED_PATHS
        if (dividend_limbs == div_limbs && div_limbs >= 2)
            return div_large_single_limb_quotient(lhs, divisor, div_limbs);
#    endif
        return div_or_rem_large_core<false>(lhs, divisor, div_limbs, dividend_limbs);
    }

#    if GINT_DETAIL_AARCH64_GCC
    static limb_type rem_estimate_single_limb_quotient(const integer & lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        using u128 = unsigned __int128;
        const int shift = __builtin_clzll(divisor.data_[div_limbs - 1]);
        const limb_type vtop = left_shifted_limb_at(divisor.data_, div_limbs - 1, shift);
        const limb_type vnext = left_shifted_limb_at(divisor.data_, div_limbs - 2, shift);
        const limb_type utop = shift ? static_cast<limb_type>(lhs.data_[div_limbs - 1] >> (64 - shift)) : 0;
        const limb_type unext = left_shifted_limb_at(lhs.data_, div_limbs - 1, shift);
        const limb_type uthird = left_shifted_limb_at(lhs.data_, div_limbs - 2, shift);

        u128 numerator = (static_cast<u128>(utop) << 64) | unext;
        u128 qhat = numerator / vtop;
        u128 rhat = numerator - qhat * vtop;
        while (qhat == (static_cast<u128>(1) << 64) || qhat * vnext > ((rhat << 64) | uthird))
        {
            --qhat;
            rhat += vtop;
            if (rhat >= (static_cast<u128>(1) << 64))
                break;
        }

        return static_cast<limb_type>(qhat);
    }

    static bool rem_sub_mul_limb(integer & lhs, const integer & divisor, size_t div_limbs, limb_type q) noexcept
    {
        unsigned __int128 carry = 0;
        limb_type borrow = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor.data_[i]) * q + carry;
            carry = p >> 64;

            unsigned __int128 subtrahend = static_cast<unsigned __int128>(static_cast<limb_type>(p)) + borrow;
            limb_type next_borrow = static_cast<unsigned __int128>(lhs.data_[i]) < subtrahend;
            lhs.data_[i] = static_cast<limb_type>(static_cast<unsigned __int128>(lhs.data_[i]) - subtrahend);
            borrow = next_borrow;
        }
        return carry != 0 || borrow != 0;
    }

    static bool rem_sub_mul_limb_full_width(integer & lhs, const integer & divisor, limb_type q) noexcept
    {
        unsigned __int128 carry = 0;
        limb_type borrow = 0;
        for (size_t i = 0; i < limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor.data_[i]) * q + carry;
            carry = p >> 64;

            unsigned __int128 subtrahend = static_cast<unsigned __int128>(static_cast<limb_type>(p)) + borrow;
            limb_type next_borrow = static_cast<unsigned __int128>(lhs.data_[i]) < subtrahend;
            lhs.data_[i] = static_cast<limb_type>(static_cast<unsigned __int128>(lhs.data_[i]) - subtrahend);
            borrow = next_borrow;
        }
        return carry != 0 || borrow != 0;
    }

    static void rem_add_divisor(integer & lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 sum = static_cast<unsigned __int128>(lhs.data_[i]) + divisor.data_[i] + carry;
            lhs.data_[i] = static_cast<limb_type>(sum);
            carry = sum >> 64;
        }
    }

    static void rem_add_divisor_full_width(integer & lhs, const integer & divisor) noexcept
    {
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < limbs; ++i)
        {
            unsigned __int128 sum = static_cast<unsigned __int128>(lhs.data_[i]) + divisor.data_[i] + carry;
            lhs.data_[i] = static_cast<limb_type>(sum);
            carry = sum >> 64;
        }
    }

    static integer rem_large_single_limb_quotient(integer lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        if (div_limbs < 2)
            return lhs;

        const limb_type q = rem_estimate_single_limb_quotient(lhs, divisor, div_limbs);
        if (div_limbs == limbs)
        {
            if (rem_sub_mul_limb_full_width(lhs, divisor, q))
                rem_add_divisor_full_width(lhs, divisor);
        }
        else if (rem_sub_mul_limb(lhs, divisor, div_limbs, q))
        {
            rem_add_divisor(lhs, divisor, div_limbs);
        }
        return lhs;
    }
#    endif

    static integer rem_large(integer lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        const size_t dividend_limbs = used_limbs(lhs);
#    if GINT_DETAIL_AARCH64_GCC
        if (dividend_limbs == div_limbs && div_limbs >= 2)
            return rem_large_single_limb_quotient(lhs, divisor, div_limbs);
#    endif
        return div_or_rem_large_core<true>(lhs, divisor, div_limbs, dividend_limbs);
    }

    // Optimized specialization: full-width 256-bit divisor (divisor_limbs == 4)
    template <size_t L = limbs>
    static GINT_NOINLINE typename std::enable_if<(L == 4), integer>::type div_large_4(integer lhs, const integer & divisor) noexcept
    {
        integer quotient;
        if (lhs.data_[3] == 0)
            return quotient;

        using u128 = unsigned __int128;

        const int shift = __builtin_clzll(divisor.data_[3]);
        limb_type u0;
        limb_type u1;
        limb_type u2;
        limb_type u3;
        limb_type u4;
        limb_type v0;
        limb_type v1;
        limb_type v2;
        limb_type v3;

        if (shift == 0)
        {
            u0 = lhs.data_[0];
            u1 = lhs.data_[1];
            u2 = lhs.data_[2];
            u3 = lhs.data_[3];
            u4 = 0;
            v0 = divisor.data_[0];
            v1 = divisor.data_[1];
            v2 = divisor.data_[2];
            v3 = divisor.data_[3];
        }
        else
        {
            const int inv_shift = 64 - shift;
            u0 = lhs.data_[0] << shift;
            u1 = (lhs.data_[1] << shift) | (lhs.data_[0] >> inv_shift);
            u2 = (lhs.data_[2] << shift) | (lhs.data_[1] >> inv_shift);
            u3 = (lhs.data_[3] << shift) | (lhs.data_[2] >> inv_shift);
            u4 = lhs.data_[3] >> inv_shift;
            v0 = divisor.data_[0] << shift;
            v1 = (divisor.data_[1] << shift) | (divisor.data_[0] >> inv_shift);
            v2 = (divisor.data_[2] << shift) | (divisor.data_[1] >> inv_shift);
            v3 = (divisor.data_[3] << shift) | (divisor.data_[2] >> inv_shift);
        }

        const u128 numerator = (static_cast<u128>(u4) << 64) | u3;
        u128 qhat = numerator / v3;
        u128 rhat = numerator - qhat * v3;

        while (qhat == (static_cast<u128>(1) << 64) || qhat * v2 > ((rhat << 64) | u2))
        {
            --qhat;
            rhat += v3;
            if (rhat >= (static_cast<u128>(1) << 64))
                break;
        }

        u128 borrow = 0;
        {
            u128 p = qhat * v0 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u0 = static_cast<limb_type>(static_cast<u128>(u0) - p);
            borrow = (p >> 64) + (u0 > ~p_low);
        }
        {
            u128 p = qhat * v1 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u1 = static_cast<limb_type>(static_cast<u128>(u1) - p);
            borrow = (p >> 64) + (u1 > ~p_low);
        }
        {
            u128 p = qhat * v2 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u2 = static_cast<limb_type>(static_cast<u128>(u2) - p);
            borrow = (p >> 64) + (u2 > ~p_low);
        }
        {
            u128 p = qhat * v3 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u3 = static_cast<limb_type>(static_cast<u128>(u3) - p);
            borrow = (p >> 64) + (u3 > ~p_low);
        }

        if (static_cast<u128>(u4) < borrow)
            --qhat;

        quotient.data_[0] = static_cast<limb_type>(qhat);
        return quotient;
    }

    static GINT_NOINLINE integer rem_large_4_impl(integer lhs, const integer & divisor) noexcept
    {
        integer result;
        if (lhs.data_[3] == 0)
            return lhs;

        using u128 = unsigned __int128;

        const int shift = __builtin_clzll(divisor.data_[3]);
        limb_type u0;
        limb_type u1;
        limb_type u2;
        limb_type u3;
        limb_type u4;
        limb_type v0;
        limb_type v1;
        limb_type v2;
        limb_type v3;

        if (shift == 0)
        {
            u0 = lhs.data_[0];
            u1 = lhs.data_[1];
            u2 = lhs.data_[2];
            u3 = lhs.data_[3];
            u4 = 0;
            v0 = divisor.data_[0];
            v1 = divisor.data_[1];
            v2 = divisor.data_[2];
            v3 = divisor.data_[3];
        }
        else
        {
            const int inv_shift = 64 - shift;
            u0 = lhs.data_[0] << shift;
            u1 = (lhs.data_[1] << shift) | (lhs.data_[0] >> inv_shift);
            u2 = (lhs.data_[2] << shift) | (lhs.data_[1] >> inv_shift);
            u3 = (lhs.data_[3] << shift) | (lhs.data_[2] >> inv_shift);
            u4 = lhs.data_[3] >> inv_shift;
            v0 = divisor.data_[0] << shift;
            v1 = (divisor.data_[1] << shift) | (divisor.data_[0] >> inv_shift);
            v2 = (divisor.data_[2] << shift) | (divisor.data_[1] >> inv_shift);
            v3 = (divisor.data_[3] << shift) | (divisor.data_[2] >> inv_shift);
        }

        const u128 numerator = (static_cast<u128>(u4) << 64) | u3;
        u128 qhat = numerator / v3;
        u128 rhat = numerator - qhat * v3;

        while (qhat == (static_cast<u128>(1) << 64) || qhat * v2 > ((rhat << 64) | u2))
        {
            --qhat;
            rhat += v3;
            if (rhat >= (static_cast<u128>(1) << 64))
                break;
        }

        u128 borrow = 0;
        {
            u128 p = qhat * v0 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u0 = static_cast<limb_type>(static_cast<u128>(u0) - p);
            borrow = (p >> 64) + (u0 > ~p_low);
        }
        {
            u128 p = qhat * v1 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u1 = static_cast<limb_type>(static_cast<u128>(u1) - p);
            borrow = (p >> 64) + (u1 > ~p_low);
        }
        {
            u128 p = qhat * v2 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u2 = static_cast<limb_type>(static_cast<u128>(u2) - p);
            borrow = (p >> 64) + (u2 > ~p_low);
        }
        {
            u128 p = qhat * v3 + borrow;
            const limb_type p_low = static_cast<limb_type>(p);
            u3 = static_cast<limb_type>(static_cast<u128>(u3) - p);
            borrow = (p >> 64) + (u3 > ~p_low);
        }

        if (static_cast<u128>(u4) < borrow)
        {
            u128 carry = 0;
            u128 t = static_cast<u128>(u0) + v0 + carry;
            u0 = static_cast<limb_type>(t);
            carry = t >> 64;
            t = static_cast<u128>(u1) + v1 + carry;
            u1 = static_cast<limb_type>(t);
            carry = t >> 64;
            t = static_cast<u128>(u2) + v2 + carry;
            u2 = static_cast<limb_type>(t);
            carry = t >> 64;
            t = static_cast<u128>(u3) + v3 + carry;
            u3 = static_cast<limb_type>(t);
            carry = t >> 64;
            // The subtract phase has not yet applied this borrow to u4.
            u4 = static_cast<limb_type>(static_cast<u128>(u4) + carry - borrow);
        }
        else
        {
            u4 = static_cast<limb_type>(static_cast<u128>(u4) - borrow);
        }

        if (shift == 0)
        {
            result.data_[0] = u0;
            result.data_[1] = u1;
            result.data_[2] = u2;
            result.data_[3] = u3;
        }
        else
        {
            const int inv_shift = 64 - shift;
            result.data_[0] = (u0 >> shift) | (u1 << inv_shift);
            result.data_[1] = (u1 >> shift) | (u2 << inv_shift);
            result.data_[2] = (u2 >> shift) | (u3 << inv_shift);
            result.data_[3] = (u3 >> shift) | (u4 << inv_shift);
        }
        return result;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L == 4), integer>::type rem_large_4(integer lhs, const integer & divisor) noexcept
    {
        return rem_large_4_impl(lhs, divisor);
    }

    // Stub for non-256-bit instantiations to keep dependent calls well-formed.
    template <size_t L = limbs>
    static typename std::enable_if<(L != 4), integer>::type div_large_4(integer lhs, const integer & divisor) noexcept
    {
        return div_large(lhs, divisor, 4);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L != 4), integer>::type rem_large_4(const integer & lhs, const integer &) noexcept
    {
        return lhs;
    }

    // Optimized specialization: two-limb divisor (divisor_limbs == 2)
    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), integer>::type div_large_2(integer lhs, const integer & divisor) noexcept GINT_CLANG_NOINLINE
    {
        integer quotient;
        size_t n = limbs;
        while (n > 0 && lhs.data_[n - 1] == 0)
            --n;
        if (n < 2)
            return quotient;

        std::array<limb_type, limbs + 1> u = {};

        // Normalize divisor so that the top limb has its MSB set.
        const limb_type d0 = divisor.data_[0];
        const limb_type d1 = divisor.data_[1];
        int shift = __builtin_clzll(d1);

        limb_type carry = lshift_limbs_to(lhs.data_, n, u.data(), shift);
        u[n] = carry;

        limb_type v0 = (d0 << shift);
        limb_type v1 = (d1 << shift) | (shift ? static_cast<limb_type>(d0 >> (64 - shift)) : 0);
        using u128 = unsigned __int128;
        // Precompute 128-bit reciprocal for v1 and use it to form an exact qhat
        const u128 inv128 = v1 ? (static_cast<u128>(~static_cast<u128>(0)) / static_cast<u128>(v1)) : 0;
        const bool v1_is_half_base = v1 == (limb_type(1) << 63);

        if (n == 4)
        {
            auto step = [&](int j)
            {
                limb_type & uj0 = u[j + 0];
                limb_type & uj1 = u[j + 1];
                limb_type & uj2 = u[j + 2];
                u128 numerator = (static_cast<u128>(uj2) << 64) | uj1;
                // 1) Initial estimate via reciprocal multiply
                u128 qhat = v1_is_half_base ? (numerator >> 63) : detail::mulhi_u128_no_middle_wrap(numerator, inv128);
                u128 qhat_v1 = v1_is_half_base ? (qhat << 63) : qhat * v1;
                // The reciprocal estimate cannot overshoot; correct only the
                // possible one-step underestimate.
                if ((numerator - qhat_v1) >= v1)
                {
                    ++qhat;
                    qhat_v1 += v1;
                }
                // Second test (Knuth): at most one adjust in practice for two-limb divisor
                u128 rhat = numerator - qhat_v1;
                if (qhat == (static_cast<u128>(1) << 64) || qhat * v0 > ((rhat << 64) | uj0))
                {
                    --qhat;
                    rhat += v1;
                }
                // Reuse high-limb product
                qhat_v1 = numerator - rhat;

                unsigned __int128 borrow = 0;
                {
                    unsigned __int128 p = qhat * v0 + borrow;
                    if (uj0 < static_cast<limb_type>(p))
                    {
                        uj0 = static_cast<limb_type>(static_cast<unsigned __int128>(uj0) - p);
                        borrow = (p >> 64) + 1;
                    }
                    else
                    {
                        uj0 = static_cast<limb_type>(static_cast<unsigned __int128>(uj0) - p);
                        borrow = p >> 64;
                    }
                }
                {
                    unsigned __int128 p = static_cast<unsigned __int128>(qhat_v1) + borrow;
                    if (uj1 < static_cast<limb_type>(p))
                    {
                        uj1 = static_cast<limb_type>(static_cast<unsigned __int128>(uj1) - p);
                        borrow = (p >> 64) + 1;
                    }
                    else
                    {
                        uj1 = static_cast<limb_type>(static_cast<unsigned __int128>(uj1) - p);
                        borrow = p >> 64;
                    }
                }
                if (static_cast<unsigned __int128>(uj2) < borrow)
                {
                    unsigned __int128 carry2 = 0;
                    unsigned __int128 t0 = static_cast<unsigned __int128>(uj0) + v0 + carry2;
                    uj0 = static_cast<limb_type>(t0);
                    carry2 = t0 >> 64;
                    unsigned __int128 t1 = static_cast<unsigned __int128>(uj1) + v1 + carry2;
                    uj1 = static_cast<limb_type>(t1);
                    carry2 = t1 >> 64;
                    uj2 = static_cast<limb_type>(static_cast<unsigned __int128>(uj2) + carry2);
                    --qhat;
                }
                else
                {
                    uj2 = static_cast<limb_type>(static_cast<unsigned __int128>(uj2) - borrow);
                }
                quotient.data_[j] = static_cast<limb_type>(qhat);
            };
            step(2);
            step(1);
            step(0);
        }
        else
        {
            for (int j = static_cast<int>(n - 2); j >= 0; --j)
            {
                limb_type & uj0 = u[j + 0];
                limb_type & uj1 = u[j + 1];
                limb_type & uj2 = u[j + 2];
                u128 numerator = (static_cast<u128>(uj2) << 64) | uj1;
                // 1) Initial estimate via reciprocal multiply
                u128 qhat = v1_is_half_base ? (numerator >> 63) : detail::mulhi_u128_no_middle_wrap(numerator, inv128);
                u128 qhat_v1 = v1_is_half_base ? (qhat << 63) : qhat * v1;
                if ((numerator - qhat_v1) >= v1)
                {
                    ++qhat;
                    qhat_v1 += v1;
                }
                // Second test
                u128 rhat = numerator - qhat_v1;
                if (qhat == (static_cast<u128>(1) << 64) || qhat * v0 > ((rhat << 64) | uj0))
                {
                    --qhat;
                    rhat += v1;
                }
                qhat_v1 = numerator - rhat;

                unsigned __int128 borrow = 0;
                {
                    unsigned __int128 p = qhat * v0 + borrow;
                    if (uj0 < static_cast<limb_type>(p))
                    {
                        uj0 = static_cast<limb_type>(static_cast<unsigned __int128>(uj0) - p);
                        borrow = (p >> 64) + 1;
                    }
                    else
                    {
                        uj0 = static_cast<limb_type>(static_cast<unsigned __int128>(uj0) - p);
                        borrow = p >> 64;
                    }
                }
                {
                    unsigned __int128 p = static_cast<unsigned __int128>(qhat_v1) + borrow;
                    if (uj1 < static_cast<limb_type>(p))
                    {
                        uj1 = static_cast<limb_type>(static_cast<unsigned __int128>(uj1) - p);
                        borrow = (p >> 64) + 1;
                    }
                    else
                    {
                        uj1 = static_cast<limb_type>(static_cast<unsigned __int128>(uj1) - p);
                        borrow = p >> 64;
                    }
                }
                if (static_cast<unsigned __int128>(uj2) < borrow)
                {
                    unsigned __int128 carry2 = 0;
                    unsigned __int128 t0 = static_cast<unsigned __int128>(uj0) + v0 + carry2;
                    uj0 = static_cast<limb_type>(t0);
                    carry2 = t0 >> 64;
                    unsigned __int128 t1 = static_cast<unsigned __int128>(uj1) + v1 + carry2;
                    uj1 = static_cast<limb_type>(t1);
                    carry2 = t1 >> 64;
                    uj2 = static_cast<limb_type>(static_cast<unsigned __int128>(uj2) + carry2);
                    --qhat;
                }
                else
                {
                    uj2 = static_cast<limb_type>(static_cast<unsigned __int128>(uj2) - borrow);
                }
                quotient.data_[j] = static_cast<limb_type>(qhat);
            }
        }
        return quotient;
    }

    // Safe fallback for a direct test/internal call on a type that cannot have
    // a two-limb divisor. Normal operator dispatch never reaches this overload.
    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), integer>::type div_large_2(integer lhs, const integer & divisor) noexcept
    {
        return lhs / divisor;
    }

    // Optimized specialization: three-limb divisor (divisor_limbs == 3)
    template <size_t L = limbs>
    static typename std::enable_if<(L >= 3), integer>::type div_large_3(integer lhs, const integer & divisor) noexcept
    {
        integer quotient;
        size_t n = limbs;
        while (n > 0 && lhs.data_[n - 1] == 0)
            --n;
        if (n < 3)
            return quotient;

        std::array<limb_type, limbs + 1> u = {};
        std::array<limb_type, 3> v = {};

        // Normalize divisor: ensure MSB of v[2] is set
        int shift = __builtin_clzll(divisor.data_[2]);
        limb_type carry = 0;
        for (size_t i = 0; i < n; ++i)
        {
            limb_type cur = lhs.data_[i];
            u[i] = (cur << shift) | carry;
            carry = shift ? static_cast<limb_type>(cur >> (64 - shift)) : 0;
        }
        u[n] = carry;

        carry = lshift_limbs_to(divisor.data_, 3, v.data(), shift);

        for (int j = static_cast<int>(n - 3); j >= 0; --j)
        {
            unsigned __int128 numerator = (static_cast<unsigned __int128>(u[j + 3]) << 64) | u[j + 2];
            // Single 128/64 division: compute quotient and derive remainder
            unsigned __int128 qhat = numerator / v[2];
            unsigned __int128 rhat = numerator - qhat * v[2];

            while (qhat == (static_cast<unsigned __int128>(1) << 64) || qhat * v[1] > ((rhat << 64) | u[j + 1]))
            {
                --qhat;
                rhat += v[2];
                if (rhat >= (static_cast<unsigned __int128>(1) << 64))
                    break;
            }

            // Reuse high-limb product: qhat*v[2] = numerator - rhat
            const unsigned __int128 qhat_v2 = numerator - rhat;

            unsigned __int128 borrow = 0;
            {
                unsigned __int128 p = qhat * v[0] + borrow;
                if (u[j + 0] < static_cast<limb_type>(p))
                {
                    u[j + 0] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 0]) - p);
                    borrow = (p >> 64) + 1;
                }
                else
                {
                    u[j + 0] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 0]) - p);
                    borrow = p >> 64;
                }
            }
            {
                unsigned __int128 p = qhat * v[1] + borrow;
                if (u[j + 1] < static_cast<limb_type>(p))
                {
                    u[j + 1] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 1]) - p);
                    borrow = (p >> 64) + 1;
                }
                else
                {
                    u[j + 1] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 1]) - p);
                    borrow = p >> 64;
                }
            }
            {
                unsigned __int128 p = qhat_v2 + borrow;
                if (u[j + 2] < static_cast<limb_type>(p))
                {
                    u[j + 2] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 2]) - p);
                    borrow = (p >> 64) + 1;
                }
                else
                {
                    u[j + 2] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 2]) - p);
                    borrow = p >> 64;
                }
            }

            if (static_cast<unsigned __int128>(u[j + 3]) < borrow)
            {
                unsigned __int128 carry2 = 0;
                for (size_t i = 0; i < 3; ++i)
                {
                    unsigned __int128 t2 = static_cast<unsigned __int128>(u[j + i]) + v[i] + carry2;
                    u[j + i] = static_cast<limb_type>(t2);
                    carry2 = t2 >> 64;
                }
                u[j + 3] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 3]) + carry2);
                --qhat;
            }
            else
            {
                u[j + 3] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + 3]) - borrow);
            }
            quotient.data_[j] = static_cast<limb_type>(qhat);
        }
        return quotient;
    }

    // Safe fallback for a direct test/internal call on a type that cannot have
    // a three-limb divisor. Normal operator dispatch never reaches this overload.
    template <size_t L = limbs>
    static typename std::enable_if<(L < 3), integer>::type div_large_3(integer lhs, const integer & divisor) noexcept
    {
        return lhs / divisor;
    }

    alignas((GINT_ARCH_AARCH64 || Bits == 64) ? alignof(limb_type) : 16) limb_type data_[limbs];
};

/// Quotient and remainder produced by a single public division operation.
template <typename Integer>
struct divmod_result
{
    Integer quotient;
    Integer remainder;
};

/// Compute quotient and remainder while sharing the expensive quotient work.
///
/// The remainder is reconstructed from the quotient so this is substantially
/// cheaper than evaluating `/` and `%` independently for wide divisors, while
/// preserving the exact signed and unsigned semantics of those operators.
template <size_t Bits, typename Signed>
inline divmod_result<integer<Bits, Signed>> divmod(const integer<Bits, Signed> & dividend, const integer<Bits, Signed> & divisor)
{
    const integer<Bits, Signed> quotient = dividend / divisor;
    const divmod_result<integer<Bits, Signed>> result = {quotient, dividend - quotient * divisor};
    return result;
}

#    if __cplusplus < 201703L
template <size_t Bits, typename Signed>
constexpr size_t integer<Bits, Signed>::bits;

template <size_t Bits, typename Signed>
constexpr size_t integer<Bits, Signed>::limbs;
#    endif

#    ifdef GINT_TEST_ACCESS
namespace detail
{
template <size_t Bits, typename Signed>
struct integer_test_access
{
    using Int = integer<Bits, Signed>;
    using limb_type = typename Int::limb_type;

    static limb_type & limb(Int & value, size_t index) noexcept { return value.data_[index]; }

    static const limb_type & limb(const Int & value, size_t index) noexcept { return value.data_[index]; }

    static int highest_bit(const Int & value) noexcept { return value.highest_bit(); }

    static bool is_min_value(const Int & value) noexcept { return Int::is_min_value(value); }

    static Int div_unsigned_path(const Int & lhs, const Int & rhs, bool lhs_neg, bool rhs_neg, bool lhs_is_min, bool rhs_is_min)
    {
        return Int::div_unsigned_path(lhs, rhs, lhs_neg, rhs_neg, lhs_is_min, rhs_is_min);
    }

    static Int div_128(const Int & lhs, const Int & rhs) noexcept { return Int::div_128(lhs, rhs); }

    static Int div_large(Int lhs, const Int & rhs, size_t div_limbs) noexcept { return Int::div_large(lhs, rhs, div_limbs); }

    static Int rem_large(Int lhs, const Int & rhs, size_t div_limbs) noexcept { return Int::rem_large(lhs, rhs, div_limbs); }

    static Int div_large_2(Int lhs, const Int & rhs) noexcept { return Int::div_large_2(lhs, rhs); }

    static Int div_large_3(Int lhs, const Int & rhs) noexcept { return Int::div_large_3(lhs, rhs); }

    static Int div_large_4(Int lhs, const Int & rhs) noexcept { return Int::div_large_4(lhs, rhs); }

    template <typename T>
    static int compare_with_float_abs(const Int & lhs_abs, T rhs_abs) noexcept
    {
        return Int::template compare_with_float_abs<T>(lhs_abs, rhs_abs);
    }
};
} // namespace detail
#    endif

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint

#endif // GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
