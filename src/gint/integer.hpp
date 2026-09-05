#pragma once

#include "limb_ops.hpp"

// Integer declarations, type constraints, storage, operators, and conversions.
// This layer owns type/sign policies; raw calculations are delegated to limb_ops.hpp.

namespace gint
{
inline namespace GINT_DETAIL_CONFIG_NAMESPACE
{

//=== Forward declarations & type aliases ====================================
template <size_t Bits, typename Signed>
class integer;

using Int128 = integer<128, signed>;
using UInt128 = integer<128, unsigned>;
using Int256 = integer<256, signed>;
using UInt256 = integer<256, unsigned>;

//=== Internal declarations and integer type support =============================
namespace detail
{
#ifdef GINT_TEST_ACCESS
template <size_t Bits, typename Signed>
struct integer_test_access;
#endif

template <size_t Bits, typename Signed>
integer<Bits, Signed> parse_string_range(const char * begin, const char * end, unsigned base);

template <unsigned BitsPerDigit, size_t Bits, typename Signed>
integer<Bits, Signed> parse_power_of_two_range(const char * begin, const char * end);

template <size_t Bits>
struct storage_count
{
    static_assert(Bits >= 64, "Bits must be at least 64");
    static_assert(Bits <= 1024, "Bits must be at most 1024");
    static_assert((Bits & (Bits - 1)) == 0, "Bits must be a power of two");
    static constexpr size_t value = Bits / 64;
};

template <size_t Bits, bool NeedsLimit = (Bits < 256)>
struct signed_promotion_limit;

template <size_t Bits>
struct signed_promotion_limit<Bits, true>
{
    static constexpr unsigned __int128 value() noexcept { return static_cast<unsigned __int128>(1) << (Bits - 1); }
};

template <size_t Bits>
struct signed_promotion_limit<Bits, false>
{
    static constexpr unsigned __int128 value() noexcept { return 0; }
};

template <size_t... I>
struct index_sequence
{
};

template <size_t N, size_t... I>
struct make_index_sequence : make_index_sequence<N - 1, N - 1, I...>
{
};

template <size_t... I>
struct make_index_sequence<0, I...>
{
    using type = index_sequence<I...>;
};

// These custom type traits extend std::is_integral / std::is_signed / std::is_unsigned
// to support the GCC-specific built-in types __int128 and unsigned __int128 in
// strict -std=c++11 mode.
template <typename T>
struct is_integral : std::is_integral<T>
{
};

template <>
struct is_integral<__int128> : std::true_type
{
};

template <>
struct is_integral<unsigned __int128> : std::true_type
{
};

template <typename T>
struct is_signed : std::is_signed<T>
{
};

template <>
struct is_signed<__int128> : std::true_type
{
};

template <>
struct is_signed<unsigned __int128> : std::false_type
{
};

template <typename T>
struct is_unsigned : std::is_unsigned<T>
{
};

template <>
struct is_unsigned<__int128> : std::false_type
{
};

template <>
struct is_unsigned<unsigned __int128> : std::true_type
{
};

} // namespace detail

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
    template <size_t, typename>
    friend class integer;
    friend class std::numeric_limits<integer<Bits, Signed>>;
    friend struct std::hash<integer<Bits, Signed>>;
#ifdef GINT_TEST_ACCESS
    friend struct detail::integer_test_access<Bits, Signed>;
#endif

private:
    //=== Object construction and cross-width storage ========================
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
        detail::bit_and_assign<limbs>(data_, rhs.data_);
        return *this;
    }

    GINT_CONSTEXPR14 integer & operator|=(const integer & rhs) noexcept
    {
        detail::bit_or_assign<limbs>(data_, rhs.data_);
        return *this;
    }

    GINT_CONSTEXPR14 integer & operator^=(const integer & rhs) noexcept
    {
        detail::bit_xor_assign<limbs>(data_, rhs.data_);
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
            detail::fill_limbs<limbs>(data_, 0);
            return *this;
        }
        const size_t limb_shift = shift / 64;
        const int bit_shift = static_cast<int>(shift % 64);
        detail::limb_shift<limbs>::shift_left_assign(data_, limb_shift, bit_shift);
        return *this;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value && !std::is_same<T, int>::value, int>::type = 0>
    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator<<=(T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return *this;
        if (shift_amount_reaches_width(n))
        {
            detail::fill_limbs<limbs>(data_, 0);
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
            detail::fill_limbs<limbs>(data_, fill);
            return *this;
        }
        const size_t limb_shift = shift / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
        detail::limb_shift<limbs>::shift_right_assign(data_, limb_shift, bit_shift, fill);
        return *this;
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
            detail::fill_limbs<limbs>(data_, fill);
            return *this;
        }
        return *this >>= static_cast<int>(n);
    }

private:
    //=== Shift policies and object fast paths ================================
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
        detail::fill_limbs<limbs>(result.data_, fill);
        return result;
    }

    template <size_t L = limbs>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE typename std::enable_if<(L == 2 && std::is_same<Signed, signed>::value), integer>::type
    shift_right_int128_unsigned_value(const integer & lhs, size_t n) noexcept
    {
        if (GINT_LIKELY(n < 128U))
        {
            integer result(uninitialized_tag{});
            detail::shift_right_arithmetic_128(result.data_, lhs.data_, n);
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
            integer result(uninitialized_tag{});
            detail::shift_left_128(result.data_, lhs.data_, n);
            return result;
        }
        return integer();
    }

#if !GINT_GCC_TUNED_PATHS
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
#endif

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_unsigned_value(const integer & lhs, unsigned n) noexcept
    {
        if (GINT_UNLIKELY(n >= Bits))
            return integer();
#if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
        if (limbs <= 8)
        {
            integer result = lhs;
            result <<= static_cast<int>(n);
            return result;
        }
        return shift_left_value_by_size_in_range(lhs, n);
#else
        integer result = lhs;
        result <<= static_cast<int>(n);
        return result;
#endif
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_right_unsigned_value(const integer & lhs, unsigned n) noexcept
    {
        if (GINT_UNLIKELY(n >= Bits))
            return shifted_out_value(lhs);
#if !GINT_GCC_TUNED_PATHS
        return shift_right_positive_value(lhs, n);
#else
        integer result = lhs;
        result >>= static_cast<int>(n);
        return result;
#endif
    }

    template <typename T>
    static GINT_CONSTEXPR14 integer shift_left_integral_value(const integer & lhs, T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return lhs;
        if (shift_amount_reaches_width(n))
            return integer();
#if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
        if (limbs <= 8)
        {
            integer result = lhs;
            result <<= static_cast<int>(n);
            return result;
        }
        return shift_left_value(lhs, static_cast<int>(n));
#else
        integer result = lhs;
        result <<= static_cast<int>(n);
        return result;
#endif
    }

    template <typename T>
    static GINT_CONSTEXPR14 integer shift_right_integral_value(const integer & lhs, T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return lhs;
        if (shift_amount_reaches_width(n))
            return shifted_out_value(lhs);
#if !GINT_GCC_TUNED_PATHS
        return shift_right_positive_value(lhs, static_cast<size_t>(n));
#else
        integer result = lhs;
        result >>= static_cast<int>(n);
        return result;
#endif
    }

#if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_value_by_size_in_range(const integer & value, size_t shift) noexcept
    {
#    if __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
        detail::limb_shift<limbs>::shift_left_into(result.data_, value.data_, shift);
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

#endif

#if !GINT_GCC_TUNED_PATHS
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
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (!__builtin_is_constant_evaluated())
        {
            integer result(uninitialized_tag{});
            detail::limb_shift<limbs>::shift_right_into(result.data_, value.data_, shift, fill);
            return result;
        }
        integer result;
#    elif __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
        detail::limb_shift<limbs>::shift_right_into(result.data_, value.data_, shift, fill);
        return result;
    }
#endif

public:
    // Increment and decrement
    GINT_CONSTEXPR14 integer & operator++() noexcept
    {
        detail::increment_limbs<limbs>(data_);
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
        detail::decrement_limbs<limbs>(data_);
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
        detail::add_limb<limbs>(lhs.data_, rhs);
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
#if GINT_DETAIL_X86_64_GCC
        if (limbs == 4 && (rhs.data_[1] | rhs.data_[2] | rhs.data_[3]) == 0)
        {
            detail::sub_limbs4_by_limb(result.data_, lhs.data_, rhs.data_[0]);
            return result;
        }
#endif
        detail::sub_limbs_copy<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    GINT_CONSTEXPR14 friend integer operator-(integer lhs, limb_type rhs) noexcept
    {
        detail::sub_limb<limbs>(lhs.data_, rhs);
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
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (__builtin_is_constant_evaluated())
        {
            integer result;
            detail::bit_and_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
            return result;
        }
#endif
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
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (__builtin_is_constant_evaluated())
        {
            integer result;
            detail::bit_or_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
            return result;
        }
#endif
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
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (__builtin_is_constant_evaluated())
        {
            integer result;
            detail::bit_xor_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
            return result;
        }
#endif
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

#if GINT_GCC_TUNED_PATHS
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
#else
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
#endif

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

    //=== Multiplication operators ===========================================
    // Multiplication is left non-constexpr due to helper internals
    friend integer operator*(const integer & lhs, const integer & rhs) noexcept
    {
        integer result(uninitialized_tag{});
#if GINT_DETAIL_X86_64_GCC
        if (limbs > 4 && GINT_UNLIKELY(lhs.data_[limbs - 1] == 0 || rhs.data_[limbs - 1] == 0)
            && detail::mul_try_single_limb_operand<limbs>(result.data_, lhs.data_, rhs.data_))
            return result;
#endif
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

    //=== Division/remainder policies and kernel dispatch =====================
    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, const integer & rhs)
    {
#if GINT_GCC_TUNED_PATHS
        limb_type positive_limb_divisor;
        if (positive_single_limb_value(rhs, positive_limb_divisor))
        {
            GINT_DIVZERO_CHECK(positive_limb_divisor == 0);
#    if GINT_DETAIL_AARCH64_GCC
            if (limbs == 2 && std::is_same<Signed, signed>::value && positive_limb_divisor > 0xFFFFFFFFULL
                && (positive_limb_divisor & (positive_limb_divisor - 1)) == 0)
                return div_by_positive_power_of_two(lhs, static_cast<int>(__builtin_ctzll(positive_limb_divisor)));
#    endif
            return div_by_positive_limb(lhs, positive_limb_divisor);
        }
#elif GINT_DETAIL_AARCH64_CLANG
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
#endif

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
#if GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG
            if (lhs_neg && rhs_neg && negative_negative_div_quotient_is_zero(lhs, divisor))
                return integer();
#endif
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
#if GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG
        if (limbs == 2)
        {
            limb_type positive_limb_divisor;
            if (positive_single_limb_value(rhs, positive_limb_divisor))
            {
                integer result;
                if (std::is_same<Signed, signed>::value && (lhs.data_[1] >> 63))
                {
#    if GINT_DETAIL_AARCH64_GCC
                    return rem_negative_int128_by_positive_limb(lhs, positive_limb_divisor);
#    else
                    using Unsigned = integer<Bits, unsigned>;
                    Unsigned lhs_mag;
                    copy_abs_magnitude(lhs_mag, lhs, true);
                    result.data_[0] = lhs_mag.mod_small(positive_limb_divisor);
                    negate_for_division(result);
                    return result;
#    endif
                }

#    if GINT_DETAIL_AARCH64_GCC
                using u128 = unsigned __int128;
                const u128 lhs_raw = (static_cast<u128>(lhs.data_[1]) << 64) | lhs.data_[0];
                result.data_[0] = static_cast<limb_type>(lhs_raw % positive_limb_divisor);
#    else
                result.data_[0] = lhs.mod_small(positive_limb_divisor);
#    endif
                return result;
            }
        }
#endif
#if GINT_CLANG_TUNED_PATHS || GINT_ARCH_X86_64
        if (!(limbs == 2 && (GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG)))
        {
            limb_type positive_limb_divisor;
            if (positive_single_limb_value(rhs, positive_limb_divisor))
                return rem_by_positive_limb(lhs, positive_limb_divisor);
        }
#endif
#if GINT_GCC_TUNED_PATHS
        if (std::is_same<Signed, signed>::value)
        {
            const bool rhs_neg = rhs.data_[limbs - 1] >> 63;
#    if GINT_DETAIL_X86_64_GCC
            const bool lhs_neg = lhs.data_[limbs - 1] >> 63;
            if (!lhs_neg && !rhs_neg)
                return rem_unsigned_magnitude(lhs, rhs);
#    endif
            return rem_signed_magnitude(lhs, rhs, rhs_neg);
        }
        else
        {
            return rem_unsigned_magnitude(lhs, rhs);
        }
#else
#    if GINT_CLANG_TUNED_PATHS
        if (limbs >= 4)
        {
            if (std::is_same<Signed, signed>::value)
            {
                const bool rhs_neg = rhs.data_[limbs - 1] >> 63;
                return rem_signed_magnitude(lhs, rhs, rhs_neg);
            }
            return rem_unsigned_magnitude_with_large_direct(lhs, rhs);
        }
#    endif
        integer q = lhs / rhs;
        q *= rhs;
        lhs -= q;
        return lhs;
#endif
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

    //=== Comparisons: sign/type handling and magnitude kernels ===============
    friend constexpr bool operator==(const integer & lhs, const integer & rhs) noexcept
    {
        return detail::equal_limbs(lhs.data_, rhs.data_);
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
        return detail::limb_float<limbs>::template compare_with_float_abs<std::is_same<Signed, signed>::value>(lhs_abs.data_, rhs_abs);
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
        return detail::less_limbs<limbs>(lhs.data_, rhs.data_);
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
        detail::complement_limbs<limbs>(v.data_);
        return v;
    }

    GINT_CONSTEXPR14 friend integer operator-(const integer & v) noexcept
    {
        integer res;
        detail::negate_limbs_copy<limbs>(res.data_, v.data_);
        return res;
    }

    GINT_CONSTEXPR14 friend integer operator+(const integer & v) noexcept { return v; }

    //=== Text adapters with private-storage access ==========================
    friend std::string to_string<>(const integer & v);
    friend std::string detail::to_base_string<>(const integer & v, unsigned base, bool uppercase);
    template <size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> from_string(const std::string & text, unsigned base);
    template <size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> detail::parse_string_range(const char * begin, const char * end, unsigned base);
    template <unsigned BitsPerDigit, size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> detail::parse_power_of_two_range(const char * begin, const char * end);

private:
    //=== Parsing support: multiply-add into this object =====================
    GINT_FORCE_INLINE void mul_add_limb(limb_type multiplier, limb_type addend) noexcept
    {
        detail::mul_add_limb<limbs>(data_, multiplier, addend);
    }

    //=== Native assignment and floating-point conversion =====================
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
        detail::fill_limbs<limbs>(data_, 0);
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
        detail::limb_float<limbs>::assign_float_digits(data_, val);
        if (neg)
            *this = -*this;
    }

    GINT_CONSTEXPR14 bool is_zero() const noexcept { return detail::is_zero_limbs<limbs>(data_); }

    int highest_bit() const noexcept { return detail::highest_bit_limbs<limbs>(data_); }

    template <typename Float>
    Float to_binary_float() const noexcept
    {
        static_assert(std::numeric_limits<Float>::radix == 2, "floating-point conversion requires a binary radix");
        static_assert(std::numeric_limits<Float>::digits < 128, "floating-point conversion supports at most 127 significand bits");
        if (is_zero())
            return Float(0);

        const bool neg = std::is_same<Signed, signed>::value && (data_[limbs - 1] >> 63);
        integer mag = neg ? -*this : *this;
        return detail::limb_float<limbs>::template to_binary_float<Float>(mag.data_, neg);
    }

    // Unsigned kernels write into distinct integer storage; signs stay above.
    GINT_FORCE_INLINE limb_type div_mod_small(limb_type div, integer & quotient) const noexcept
    {
        return detail::limb_division<limbs>::div_mod_small(data_, div, quotient.data_);
    }

    GINT_FORCE_INLINE limb_type mod_small(limb_type div) const noexcept { return detail::limb_division<limbs>::mod_small(data_, div); }

    GINT_SMALL_DIV_INLINE signed_limb_type div_mod_small(signed_limb_type div, integer & quotient) const noexcept
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

    //=== Magnitude inspection, promotion, and signed division policies ======
    static size_t used_limbs(const integer & v) noexcept { return detail::used_limbs<limbs>(v.data_); }

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

    static GINT_FORCE_INLINE void negate_for_division(integer & v) noexcept { detail::negate_limbs<limbs>(v.data_); }

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
#if GINT_DETAIL_AARCH64_CLANG
        return positive_power_of_two_value(v, bit_index);
#else
        (void)v;
        (void)bit_index;
        return false;
#endif
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
    static GINT_FORCE_INLINE typename std::enable_if<(L == 2), bool>::type
    negative_negative_div_quotient_is_zero(const integer & lhs, const integer & rhs) noexcept
    {
        return detail::limb_division<limbs>::greater_limbs_128(lhs.data_, rhs.data_);
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
#if GINT_DETAIL_AARCH64_CLANG
            if (GINT_UNLIKELY(divisor > 0xFFFFFFFFULL && (divisor & (divisor - 1)) != 0)
                && div_signed_int128_by_positive_limb(lhs, divisor, result))
                return result;
#endif
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
#if GINT_DETAIL_AARCH64_CLANG
            if (GINT_UNLIKELY(divisor > 0xFFFFFFFFULL && (divisor & (divisor - 1)) != 0)
                && div_signed_int128_by_positive_limb(lhs, divisor, result))
                return result;
#endif
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
        integer result;
        detail::div_unsigned_int128_by_positive_limb(result.data_, lhs.data_, divisor);
        return result;
    }

    template <size_t L = limbs, typename std::enable_if<(L == 2 && std::is_same<Signed, signed>::value), int>::type = 0>
    static GINT_FORCE_INLINE bool div_signed_int128_by_positive_limb(const integer & lhs, limb_type divisor, integer & result) noexcept
    {
        detail::div_signed_int128_by_positive_limb(result.data_, lhs.data_, divisor);
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
        detail::copy_magnitude_limbs<limbs>(dst.data_, src.data_, neg);
    }

    static GINT_FORCE_INLINE integer rem_signed_magnitude(const integer & lhs, const integer & rhs, bool rhs_neg) noexcept
    {
        using Unsigned = integer<Bits, unsigned>;
        const bool lhs_neg = lhs.data_[limbs - 1] >> 63;
        Unsigned lhs_mag(typename Unsigned::uninitialized_tag{});
        Unsigned rhs_mag(typename Unsigned::uninitialized_tag{});
        copy_abs_magnitude(lhs_mag, lhs, lhs_neg);
        copy_abs_magnitude(rhs_mag, rhs, rhs_neg);

#if GINT_DETAIL_AARCH64_GCC
        Unsigned rem_mag = Unsigned::rem_unsigned_magnitude_with_large_direct(lhs_mag, rhs_mag);
#elif GINT_CLANG_TUNED_PATHS
        Unsigned rem_mag = rem_signed_magnitude_unsigned(lhs_mag, rhs_mag);
#else
        Unsigned rem_mag = Unsigned::rem_unsigned_magnitude(lhs_mag, rhs_mag);
#endif
        integer result(uninitialized_tag{});
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = rem_mag.data_[i];
        if (lhs_neg)
            negate_for_division(result);
        return result;
    }

#if GINT_CLANG_TUNED_PATHS
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
#endif

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

#if GINT_DETAIL_X86_64_GCC || GINT_DETAIL_AARCH64_CLANG
        if (limbs == 4 && divisor_limbs == 4)
            return rem_large_4(lhs, divisor);
#endif

        integer quotient;
        if (limbs == 2)
#if GINT_DETAIL_AARCH64_GCC
            quotient = div_128_native(lhs, divisor);
#else
            quotient = div_128(lhs, divisor);
#endif
        else if (divisor_limbs == 2)
            quotient = div_large_2(lhs, divisor);
        else if (divisor_limbs == 3)
            quotient = div_large_3(lhs, divisor);
        else if (limbs == 4 && divisor_limbs == 4)
            quotient = div_large_4(lhs, divisor);
        else
            quotient = div_large(lhs, divisor, divisor_limbs);

        result = lhs;
#if GINT_GCC_TUNED_PATHS
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
#endif
        quotient *= divisor;
        result -= quotient;
        return result;
    }

    static integer rem_unsigned_magnitude_with_large_direct(const integer & lhs, const integer & divisor) noexcept
    {
#if GINT_DETAIL_AARCH64_GCC || GINT_CLANG_TUNED_PATHS
        if (((GINT_DETAIL_AARCH64_GCC && limbs >= 4) || (GINT_CLANG_TUNED_PATHS && limbs >= 8))
            && GINT_UNLIKELY((divisor.data_[limbs - 1] | divisor.data_[limbs - 2]) != 0))
        {
            const size_t divisor_limbs = used_limbs(divisor);
            int pow_bit;
            if (is_power_of_two(divisor, pow_bit))
                return lhs & (divisor - integer(1));
            return rem_large(lhs, divisor, divisor_limbs);
        }
#endif
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
        return detail::power_of_two_limbs<limbs>(v.data_, bit_index);
    }

    //=== Unsigned division adapters =========================================
    static GINT_FORCE_INLINE integer div_128_native(const integer & lhs, const integer & rhs) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::div_128_native(lhs.data_, rhs.data_, result.data_);
        return result;
    }

    static GINT_FORCE_INLINE integer div_128(const integer & lhs, const integer & rhs) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::div_128(lhs.data_, rhs.data_, result.data_);
        return result;
    }

    static GINT_FORCE_INLINE integer div_large(integer lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::div_large(lhs.data_, divisor.data_, div_limbs, result.data_);
        return result;
    }

    static GINT_FORCE_INLINE integer rem_large(integer lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::rem_large(lhs.data_, divisor.data_, div_limbs, result.data_);
        return result;
    }

    static GINT_FORCE_INLINE integer div_large_4(integer lhs, const integer & divisor) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::div_large_4(lhs.data_, divisor.data_, result.data_);
        return result;
    }

    static GINT_FORCE_INLINE integer rem_large_4(integer lhs, const integer & divisor) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::rem_large_4(lhs.data_, divisor.data_, result.data_);
        return result;
    }

    // Optimized specialization: two-limb divisor (divisor_limbs == 2)
    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L >= 2), integer>::type div_large_2(integer lhs, const integer & divisor) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::div_large_2(lhs.data_, divisor.data_, result.data_);
        return result;
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
    static GINT_FORCE_INLINE typename std::enable_if<(L >= 3), integer>::type div_large_3(integer lhs, const integer & divisor) noexcept
    {
        integer result(uninitialized_tag{});
        detail::limb_division<limbs>::div_large_3(lhs.data_, divisor.data_, result.data_);
        return result;
    }

    // Safe fallback for a direct test/internal call on a type that cannot have
    // a three-limb divisor. Normal operator dispatch never reaches this overload.
    template <size_t L = limbs>
    static typename std::enable_if<(L < 3), integer>::type div_large_3(integer lhs, const integer & divisor) noexcept
    {
        return lhs / divisor;
    }

    //=== Object representation ==============================================
    alignas((GINT_ARCH_AARCH64 || Bits == 64) ? alignof(limb_type) : 16) limb_type data_[limbs];
};

//=== Public combined division ===============================================
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

#if __cplusplus < 201703L
template <size_t Bits, typename Signed>
constexpr size_t integer<Bits, Signed>::bits;

template <size_t Bits, typename Signed>
constexpr size_t integer<Bits, Signed>::limbs;
#endif

#ifdef GINT_TEST_ACCESS
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
#endif

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint
