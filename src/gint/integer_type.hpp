#pragma once

#include "limb_ops.hpp"

// Integer storage, conversions, operators, and common helpers.

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
struct division_ops;

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
#ifdef GINT_TEST_ACCESS
    friend struct detail::integer_test_access<Bits, Signed>;
#endif

private:
    template <size_t, typename>
    friend struct detail::division_ops;
    using Division = detail::division_ops<Bits, Signed>;

    // Select the inline unsigned 256-bit entry without changing its operand types.
    struct inactive_dividend;
    static constexpr bool inline_unsigned_division = GINT_DETAIL_AARCH64_GCC && limbs == 4 && std::is_same<Signed, unsigned>::value;
    using inline_dividend = typename std::conditional<inline_unsigned_division, integer, inactive_dividend &>::type;
    using regular_dividend = typename std::conditional<inline_unsigned_division, inactive_dividend &, integer>::type;

    // The x86-64 wide scalar remainder path reads the existing limbs directly.
    using scalar_mod_dividend = typename std::conditional<GINT_ARCH_X86_64 && (limbs > 2), const integer &, integer>::type;

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

    template <typename T>
    struct is_limb_scalar
        : std::integral_constant<bool, detail::is_integral<T>::value && !std::is_same<T, bool>::value && sizeof(T) <= sizeof(limb_type)>
    {
    };

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
        return *this = integer(other);
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
        // Bound the shift even for sign-fill limbs where the branch is discarded.
        typedef __int128 wide_signed;
        typedef unsigned __int128 wide_unsigned;
        return I < (sizeof(T) * 8 + 63) / 64
            ? static_cast<limb_type>(
                  (detail::is_signed<T>::value ? static_cast<wide_unsigned>(static_cast<wide_signed>(v)) : static_cast<wide_unsigned>(v))
                  >> ((I % 2) * 64))
            : (detail::is_signed<T>::value && static_cast<wide_signed>(v) < 0 ? ~0ULL : 0ULL);
    }

public:
    // Assignment operators
    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 integer & operator=(T v) noexcept
    {
        return *this = integer(v);
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
        // Bound shift counts even in branches that older GCC versions diagnose before discarding.
        if (std::is_same<Signed, signed>::value && Bits < 128 && wbits > Bits && (data_[limbs - 1] >> 63))
            value |= ~u128(0) << (Bits % 128);
        if (wbits < 128)
        {
            const u128 modulus = u128(1) << (wbits % 128);
            value &= modulus - 1;
            if (!detail::is_unsigned<T>::value && ((value >> (wbits - 1)) & 1))
                return static_cast<T>(static_cast<s128>(value) - static_cast<s128>(modulus));
            return static_cast<T>(value);
        }
        // GCC/Clang interpret a signed 128-bit target as two's complement.
        return detail::is_unsigned<T>::value ? static_cast<T>(value) : static_cast<T>(static_cast<s128>(value));
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

    template <typename T, typename std::enable_if<is_limb_scalar<T>::value, int>::type = 0>
    integer & operator*=(T rhs) noexcept
    {
        multiply_limb_scalar(rhs, std::is_signed<T>{});
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
        if (n < 0 || (limbs != 4 && n == 0))
            return *this;
        size_t total_bits = Bits;
        size_t shift = static_cast<size_t>(n);
        if (shift >= total_bits)
            return fill_shifted_out(0);
        if (limbs == 2)
        {
            detail::store_u128(data_, detail::load_u128(data_) << shift);
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
                    case 0:
                        return *this;
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
            return fill_shifted_out(0);
        return *this <<= static_cast<int>(n);
    }

    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator>>=(int n) noexcept
    {
        if (n < 0 || ((!GINT_ARCH_X86_64 || limbs != 4) && n == 0))
            return *this;
        const limb_type fill = right_shift_fill(*this);
        const size_t shift = static_cast<size_t>(n);
        if (shift >= Bits)
            return fill_shifted_out(fill);
        if (limbs == 2)
        {
            detail::store_u128(data_, shift_right_u128(detail::load_u128(data_), shift));
            return *this;
        }
        size_t limb_shift = shift / 64;
        unsigned bit_shift = static_cast<unsigned>(shift % 64);
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
                    case 0:
                        return *this;
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
            return fill_shifted_out(right_shift_fill(*this));
        return *this >>= static_cast<int>(n);
    }

private:
    template <typename T>
    static constexpr bool shift_amount_non_positive(T n) noexcept
    {
        return detail::is_signed<T>::value && n <= 0;
    }

    template <typename T>
    static constexpr bool shift_amount_reaches_width(T n) noexcept
    {
        using distance_type = typename std::conditional<(sizeof(T) > sizeof(size_t)), unsigned __int128, size_t>::type;
        return static_cast<distance_type>(n) >= Bits;
    }

    template <detail::bitwise_op Op>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer bitwise_value(const integer & lhs, const integer & rhs) noexcept
    {
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (__builtin_is_constant_evaluated())
        {
            integer result;
            detail::bitwise_limbs<Op, limbs>(result.data_, lhs.data_, rhs.data_);
            return result;
        }
#endif
        integer result(uninitialized_tag{});
        detail::bitwise_limbs<Op, limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    static constexpr limb_type right_shift_fill(const integer & value) noexcept
    {
        return std::is_same<Signed, signed>::value && (value.data_[limbs - 1] >> 63) ? ~limb_type(0) : limb_type(0);
    }

    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & fill_shifted_out(limb_type fill) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] = fill;
        return *this;
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shifted_out_value(const integer & value) noexcept
    {
        integer result;
        result.fill_shifted_out(right_shift_fill(value));
        return result;
    }

    template <bool Right>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_copy(integer value, int n) noexcept
    {
        if (Right)
            value >>= n;
        else
            value <<= n;
        return value;
    }

    static constexpr unsigned __int128 shift_right_u128(unsigned __int128 value, size_t n) noexcept
    {
        // The supported GNU-compatible frontends use two's-complement conversion and arithmetic signed right shift.
        return std::is_same<Signed, signed>::value ? static_cast<unsigned __int128>(static_cast<__int128>(value) >> n) : value >> n;
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_int128_unsigned_value(const integer & lhs, unsigned n) noexcept
    {
        if (GINT_UNLIKELY(n >= Bits))
            return integer();
        integer result;
        detail::store_u128(result.data_, detail::load_u128(lhs.data_) << n);
        return result;
    }

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
        return shift_right_in_range(lhs, n);
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_right_in_range(const integer & lhs, size_t n) noexcept
    {
        if (limbs == 2)
            return integer(shift_right_u128(detail::load_u128(lhs.data_), n));
#if !GINT_GCC_TUNED_PATHS
        return shift_right_value(lhs, static_cast<int>(n));
#else
        return shift_copy<true>(lhs, static_cast<int>(n));
#endif
    }

    template <typename T>
    static GINT_CONSTEXPR14 integer shift_left_integral_value(const integer & lhs, T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return lhs;
        if (shift_amount_reaches_width(n))
            return integer();
        return shift_left_unsigned_value(lhs, static_cast<unsigned>(n));
    }

    template <typename T>
    static GINT_CONSTEXPR14 integer shift_right_integral_value(const integer & lhs, T n) noexcept
    {
        if (shift_amount_non_positive(n))
            return lhs;
        if (shift_amount_reaches_width(n))
            return shifted_out_value(lhs);
        return shift_right_in_range(lhs, static_cast<size_t>(n));
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

#if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_value_by_size_in_range(const integer & value, size_t shift) noexcept
    {
#    if __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
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
            return shift_copy<false>(value, n);
        }
        if (n <= 0)
            return value;

        return shift_left_value_by_size(value, static_cast<size_t>(n));
    }

#endif

#if !GINT_GCC_TUNED_PATHS
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE void
    shift_right_value_into(integer & result, const integer & value, size_t shift, limb_type fill) noexcept
    {
        if (shift >= Bits)
        {
            result.fill_shifted_out(fill);
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
            return shift_copy<true>(value, n);
        }
        if (n <= 0)
            return value;

        const limb_type fill = right_shift_fill(value);
        const size_t shift = static_cast<size_t>(n);
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
        if (!__builtin_is_constant_evaluated())
        {
            integer result(uninitialized_tag{});
            shift_right_value_into(result, value, shift, fill);
            return result;
        }
        integer result;
#    elif __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
        shift_right_value_into(result, value, shift, fill);
        return result;
    }
#endif

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
        if (detail::use_fixed_limb_add_sub<limbs>())
        {
            integer result;
            detail::add_limbs4_by_limb(result.data_, lhs.data_, rhs);
            return result;
        }
        if (limbs == 2)
            return integer(detail::load_u128(lhs.data_) + rhs);
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
        return add_copy(lhs, integer(rhs));
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator+(T lhs, const integer & rhs) noexcept
    {
        return add_copy(integer(lhs), rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(const integer & lhs, T rhs) noexcept
    {
        return add_copy(lhs, integer(rhs));
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(T lhs, const integer & rhs) noexcept
    {
        return add_copy(integer(lhs), rhs);
    }

    GINT_CONSTEXPR14 friend integer operator-(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
        detail::sub_limbs_copy_disjoint<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    GINT_CONSTEXPR14 friend integer operator-(integer lhs, limb_type rhs) noexcept
    {
        if (detail::use_fixed_limb_add_sub<limbs>())
        {
            integer result;
            detail::sub_limbs4_by_limb_for_scalar(result.data_, lhs.data_, rhs);
            return result;
        }
        if (limbs == 2)
            return integer(detail::load_u128(lhs.data_) - rhs);
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
        return sub_copy(lhs, integer(rhs));
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator-(T lhs, const integer & rhs) noexcept
    {
        return sub_copy(integer(lhs), rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(const integer & lhs, T rhs) noexcept
    {
        return sub_copy(lhs, integer(rhs));
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(T lhs, const integer & rhs) noexcept
    {
        return sub_copy(integer(lhs), rhs);
    }

    GINT_CONSTEXPR14 friend integer operator&(const integer & lhs, const integer & rhs) noexcept
    {
        return bitwise_value<detail::bitwise_op::bit_and>(lhs, rhs);
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
        return bitwise_value<detail::bitwise_op::bit_or>(lhs, rhs);
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
        return bitwise_value<detail::bitwise_op::bit_xor>(lhs, rhs);
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
        if (limbs == 2)
            return shift_right_unsigned_value(lhs, n);
        const bool in_range = n < Bits;
        const unsigned clamped_shift = in_range ? n : (GINT_ARCH_X86_64 ? Bits : 0);
        lhs >>= static_cast<int>(clamped_shift);
        return GINT_ARCH_X86_64 || in_range ? lhs : shifted_out_value(lhs);
    }

    template <size_t L = limbs, typename std::enable_if<(L > 4), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, unsigned n) noexcept
    {
        return shift_right_unsigned_value(lhs, n);
    }

    template <size_t L = limbs, typename std::enable_if<(L == 4 && !std::is_same<size_t, unsigned>::value), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, size_t n) noexcept
    {
        if (GINT_UNLIKELY(n >= Bits))
            return shifted_out_value(lhs);
        return lhs >> static_cast<unsigned>(n);
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
        lhs *= rhs;
        return lhs;
    }

    friend integer operator*(limb_type lhs, integer rhs) noexcept { return rhs * lhs; }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_FORCE_INLINE integer operator*(integer lhs, T rhs) noexcept
    {
        return lhs.multiply_scalar_if_limb(rhs, is_limb_scalar<T>{});
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator*(T lhs, integer rhs) noexcept
    {
        return rhs * lhs;
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
        return rhs * lhs;
    }

    friend GINT_HIDDEN_VISIBILITY GINT_FORCE_INLINE integer operator/(inline_dividend lhs, const integer & rhs)
    {
        return Division::div_unsigned_256(lhs, rhs);
    }

    friend GINT_HIDDEN_VISIBILITY integer operator/(regular_dividend lhs, const integer & rhs)
    {
        if (limbs == 2)
            return Division::template div_or_mod_128<false>(lhs, rhs);
#if !GINT_DETAIL_X86_64_CLANG
        limb_type positive_limb_divisor;
        if (Division::positive_single_limb_value(rhs, positive_limb_divisor))
        {
            GINT_DIVZERO_CHECK(positive_limb_divisor == 0);
            return Division::div_by_positive_limb(lhs, positive_limb_divisor);
        }
#endif

        int positive_pow_bit;
        if (Division::positive_power_of_two_fastpath_divisor(rhs, positive_pow_bit))
            return Division::div_by_positive_power_of_two(lhs, positive_pow_bit);

        bool lhs_neg = false;
        bool rhs_neg = false;
        bool lhs_is_min = false;
        bool rhs_is_min = false;
        integer divisor = rhs;
        if (std::is_same<Signed, signed>::value)
        {
            lhs_neg = lhs.data_[limbs - 1] >> 63;
            rhs_neg = divisor.data_[limbs - 1] >> 63;
            const limb_type min_magnitude = static_cast<limb_type>(1ULL << 63);
            // Check the full min pattern only after the high limb matches it.
            if (lhs_neg)
            {
                if (GINT_UNLIKELY(lhs.data_[limbs - 1] == min_magnitude && is_min_value(lhs)))
                    lhs_is_min = true;
                else
                    negate_in_place(lhs);
            }
            // Apply the same high-limb gate to the divisor.
            if (rhs_neg)
            {
                if (GINT_UNLIKELY(divisor.data_[limbs - 1] == min_magnitude && is_min_value(divisor)))
                    rhs_is_min = true;
                else
                    negate_in_place(divisor);
            }
            if (GINT_UNLIKELY(lhs_is_min || rhs_is_min))
                return Division::div_unsigned_path(lhs, divisor, lhs_neg, rhs_neg, lhs_is_min, rhs_is_min);
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
            Division::div_mod_small(lhs, divisor.data_[0], result);
        }
        else
        {
            int pow_bit;
            if (Division::is_power_of_two(divisor, pow_bit))
            {
                // power-of-two divisor turns into a simple shift
                result = lhs >> pow_bit;
            }
            else if (divisor_limbs == 2)
            {
                // Specialized fast path: two-limb divisor
                result = Division::div_large_2(lhs, divisor);
            }
            else if (divisor_limbs == 3)
            {
                // Specialized fast path: three-limb divisor
                result = Division::div_large_3(lhs, divisor);
            }
            else if (limbs == 4 && divisor_limbs == 4)
            {
                // Specialized fast path: full-width 256-bit divisor produces a single quotient limb.
                result = Division::div_large_4(lhs, divisor);
            }
            else
            {
                // Dispatch general multi-limb division, including equal-length quotient estimation.
                result = Division::div_large(lhs, divisor, divisor_limbs);
            }
        }
        if (std::is_same<Signed, signed>::value && lhs_neg != rhs_neg)
            negate_in_place(result);
        return result;
    }

    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, limb_type rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        if (uses_unsigned_limb_divisor<limb_type>())
            return rhs == 0 ? integer() : Division::div_by_positive_limb(lhs, rhs);
        if (needs_unsigned_signed_promotion(rhs))
            return integer(promoted_signed_type(lhs) / promoted_signed_type(rhs));
        if (fits_signed_limb(rhs))
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
        Division::div_mod_small(lhs, rhs, q);
        return q;
    }

    friend GINT_HIDDEN_VISIBILITY integer operator/(limb_type lhs, integer rhs)
    {
        if (needs_unsigned_signed_promotion(lhs))
            return integer(promoted_signed_type(lhs) / promoted_signed_type(rhs));
        return integer(lhs) / rhs;
    }

    friend GINT_HIDDEN_VISIBILITY integer operator%(integer lhs, const integer & rhs) { return Division::modulo(lhs, rhs); }

    friend GINT_HIDDEN_VISIBILITY integer operator%(scalar_mod_dividend lhs, limb_type rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        if (uses_unsigned_limb_divisor<limb_type>())
            return rhs == 0 ? lhs : Division::rem_by_positive_limb(lhs, rhs);
        if (needs_unsigned_signed_promotion(rhs))
            return integer(promoted_signed_type(lhs) % promoted_signed_type(rhs));
        if (fits_signed_limb(rhs))
            return lhs % static_cast<signed_limb_type>(rhs);
        return lhs % integer(rhs);
    }

    friend GINT_HIDDEN_VISIBILITY integer operator%(scalar_mod_dividend lhs, signed_limb_type rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        if (GINT_UNLIKELY(rhs == 0))
            return lhs;
        // For unsigned integers, mimic native casts: reinterpret negative divisors as their two's complement magnitude.
        if (std::is_same<Signed, unsigned>::value && rhs < 0)
            return lhs % integer(rhs);
#if GINT_ARCH_X86_64
        if (limbs > 2)
        {
            const limb_type divisor = static_cast<limb_type>(rhs);
            return Division::rem_by_positive_limb(lhs, rhs < 0 ? limb_type(0) - divisor : divisor);
        }
#endif
        integer q;
        signed_limb_type r = Division::div_mod_small(lhs, rhs, q);
        return integer(r);
    }

    friend GINT_HIDDEN_VISIBILITY integer operator%(limb_type lhs, integer rhs)
    {
        if (needs_unsigned_signed_promotion(lhs))
            return integer(promoted_signed_type(lhs) % promoted_signed_type(rhs));
        return integer(lhs) % rhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, T rhs)
    {
        if (uses_unsigned_limb_divisor<T>())
            return lhs / static_cast<limb_type>(rhs);
        if (is_int64_with_int128<T>())
            return Division::template divmod_int64_int128<false>(static_cast<signed_limb_type>(lhs.data_[0]), rhs);
        if (outside_uint64_operand_range(rhs))
            return integer(static_cast<T>(lhs.data_[0]) / rhs);
        GINT_DIVZERO_CHECK(rhs == 0);
        if (needs_unsigned_signed_promotion(rhs))
            return integer(promoted_signed_type(lhs) / promoted_signed_type(rhs));
        if (fits_signed_limb(rhs))
            return lhs / static_cast<signed_limb_type>(rhs);
        return lhs / integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(T lhs, integer rhs)
    {
        if (is_int64_with_int128<T>())
            return Division::template divmod_int64_int128<false>(lhs, static_cast<signed_limb_type>(rhs.data_[0]));
        if (outside_uint64_operand_range(lhs) && rhs.data_[0] != 0)
            return integer(lhs / static_cast<T>(rhs.data_[0]));
        if (needs_unsigned_signed_promotion(lhs))
            return integer(promoted_signed_type(lhs) / promoted_signed_type(rhs));
        return integer(lhs) / rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(integer lhs, T rhs)
    {
        if (infinite_divmod_operand<false>(rhs))
            return integer(); // finite / ±inf -> 0
        integer div(rhs);
        GINT_DIVZERO_CHECK(div.is_zero());
        return lhs / div;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator/(T lhs, integer rhs)
    {
        if (infinite_divmod_operand<false>(lhs))
            GINT_THROW(std::domain_error("infinite dividend"));
        GINT_DIVZERO_CHECK(rhs.is_zero());
        return integer(lhs) / rhs;
    } // LCOV_EXCL_LINE

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(scalar_mod_dividend lhs, T rhs)
    {
        if (uses_unsigned_limb_divisor<T>())
            return lhs % static_cast<limb_type>(rhs);
        if (limbs == 4 && std::is_same<Signed, unsigned>::value && detail::is_unsigned<T>::value && sizeof(T) <= sizeof(uint32_t))
        {
            GINT_MODZERO_CHECK(rhs == 0);
            return rhs == 0 ? lhs : Division::rem_by_positive_limb(lhs, static_cast<uint32_t>(rhs));
        }
        if (is_int64_with_int128<T>())
            return Division::template divmod_int64_int128<true>(static_cast<signed_limb_type>(lhs.data_[0]), rhs);
        if (outside_uint64_operand_range(rhs))
            return integer(static_cast<T>(lhs.data_[0]) % rhs);
        GINT_MODZERO_CHECK(rhs == 0);
        if (GINT_UNLIKELY(rhs == 0))
            return lhs;
        if (needs_unsigned_signed_promotion(rhs))
            return integer(promoted_signed_type(lhs) % promoted_signed_type(rhs));
        // For unsigned integers, mimic native casts: reinterpret negative divisors as their two's complement magnitude.
        if (std::is_same<Signed, unsigned>::value && detail::is_signed<T>::value && static_cast<__int128>(rhs) < 0)
            return lhs % integer(rhs);
        if (fits_signed_limb(rhs))
            return lhs % static_cast<signed_limb_type>(rhs);
        return lhs % integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(T lhs, integer rhs)
    {
        if (is_int64_with_int128<T>())
            return Division::template divmod_int64_int128<true>(lhs, static_cast<signed_limb_type>(rhs.data_[0]));
        if (outside_uint64_operand_range(lhs) && rhs.data_[0] != 0)
            return integer(lhs % static_cast<T>(rhs.data_[0]));
        if (needs_unsigned_signed_promotion(lhs))
            return integer(promoted_signed_type(lhs) % promoted_signed_type(rhs));
        return integer(lhs) % rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(integer lhs, T rhs)
    {
        if (infinite_divmod_operand<true>(rhs))
            return lhs; // finite % ±inf -> lhs
        integer div(rhs);
        GINT_MODZERO_CHECK(div.is_zero());
        return lhs % div;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend GINT_HIDDEN_VISIBILITY integer operator%(T lhs, integer rhs)
    {
        if (infinite_divmod_operand<true>(lhs))
            GINT_THROW(std::domain_error("infinite dividend in modulo"));
        return integer(lhs) % rhs;
    } // LCOV_EXCL_LINE

    friend constexpr bool operator==(const integer & lhs, const integer & rhs) noexcept
    {
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L && !GINT_GCC_TUNED_PATHS
        if (!__builtin_is_constant_evaluated() && limbs == 16)
            return detail::limbs_equal_runtime_1024(lhs.data_, rhs.data_);
#endif
        return detail::limbs_equal<limbs - 1>::eval(lhs, rhs);
    }

    friend constexpr bool operator!=(const integer & lhs, const integer & rhs) noexcept { return !(lhs == rhs); }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend constexpr bool operator==(const integer & lhs, T rhs) noexcept
    {
        return is_int64_with_int128<T>()
            ? static_cast<signed_limb_type>(lhs.data_[0]) == static_cast<__int128>(rhs)
            : !outside_uint64_operand_range(rhs) && !needs_unsigned_signed_promotion(rhs) && lhs == integer(rhs);
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
    // 3) Use the leading p bits of lhs_abs as its significand (p = digits),
    //    or left-align its low limbs if it has fewer than p bits.
    // 4) Compare the significands. On a tie, a fractional tail makes rhs_abs
    //    larger, or any discarded low bit makes lhs_abs larger.
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
        unsigned __int128 sigA;
        if (shift > 0)
            sigA = binary_float_significand<T>(lhs_abs, shift);
        else
        {
            if (limbs >= 2)
                sigA = (static_cast<unsigned __int128>(lhs_abs.data_[1]) << 64) | lhs_abs.data_[0];
            else
                sigA = lhs_abs.data_[0];
            if (shift < 0)
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
        return has_any_bit_below(lhs_abs, shift) ? 1 : 0;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator==(const integer & lhs, T rhs) noexcept
    {
        return compare_with_float(lhs, rhs) == float_order::equal;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator==(T lhs, const integer & rhs) noexcept
    {
        return rhs == lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator!=(const integer & lhs, T rhs) noexcept
    {
        return compare_with_float(lhs, rhs) != float_order::equal;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator!=(T lhs, const integer & rhs) noexcept
    {
        return rhs != lhs;
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
        if (is_int64_with_int128<T>())
            return static_cast<signed_limb_type>(lhs.data_[0]) < static_cast<__int128>(rhs);
        if (outside_uint64_operand_range(rhs))
            return rhs > 0;
        if (needs_unsigned_signed_promotion(rhs))
            return promoted_signed_type(lhs) < promoted_signed_type(rhs);
        return lhs < integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<(T lhs, const integer & rhs) noexcept
    {
        if (is_int64_with_int128<T>())
            return static_cast<__int128>(lhs) < static_cast<signed_limb_type>(rhs.data_[0]);
        if (outside_uint64_operand_range(lhs))
            return detail::is_signed<T>::value && static_cast<__int128>(lhs) < 0;
        if (needs_unsigned_signed_promotion(lhs))
            return promoted_signed_type(lhs) < promoted_signed_type(rhs);
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
        return compare_with_float(lhs, rhs) == float_order::less;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<(T lhs, const integer & rhs) noexcept
    {
        return rhs > lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>(const integer & lhs, T rhs) noexcept
    {
        return compare_with_float(lhs, rhs) == float_order::greater;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>(T lhs, const integer & rhs) noexcept
    {
        return rhs < lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<=(const integer & lhs, T rhs) noexcept
    {
        const float_order order = compare_with_float(lhs, rhs);
        return order == float_order::less || order == float_order::equal;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<=(T lhs, const integer & rhs) noexcept
    {
        return rhs >= lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>=(const integer & lhs, T rhs) noexcept
    {
        const float_order order = compare_with_float(lhs, rhs);
        return order == float_order::greater || order == float_order::equal;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>=(T lhs, const integer & rhs) noexcept
    {
        return rhs <= lhs;
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

    template <size_t OtherBits, typename OtherSigned>
    friend divmod_result<integer<OtherBits, OtherSigned>>
    divmod(const integer<OtherBits, OtherSigned> &, const integer<OtherBits, OtherSigned> &);

    friend std::string to_string<>(const integer & v);
    friend std::string detail::to_base_string<>(const integer & v, unsigned base, bool uppercase);
    template <size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> from_string(const std::string & text, unsigned base);
    template <size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> detail::parse_string_range(const char * begin, const char * end, unsigned base);
    template <unsigned BitsPerDigit, size_t OtherBits, typename OtherSigned>
    friend integer<OtherBits, OtherSigned> detail::parse_power_of_two_range(const char * begin, const char * end);

private:
    template <typename T>
    GINT_FORCE_INLINE integer multiply_scalar_if_limb(T rhs, std::true_type) noexcept
    {
        multiply_limb_scalar(rhs, std::is_signed<T>{});
        return *this;
    }

    template <typename T>
    GINT_FORCE_INLINE integer multiply_scalar_if_limb(T rhs, std::false_type) const noexcept
    {
        return *this * integer(rhs);
    }

    template <typename T>
    GINT_FORCE_INLINE void multiply_limb_scalar(T rhs, std::false_type) noexcept
    {
#if GINT_DETAIL_AARCH64_CLANG_CARRY_BUILTINS
        if (limbs == 4 && sizeof(T) == sizeof(limb_type))
        {
            detail::mul_limbs4_by_u64_in_place(data_, rhs);
            return;
        }
#endif
        detail::mul_limb<limbs>(data_, rhs);
    }

    GINT_FORCE_INLINE void multiply_limb_scalar(int64_t rhs, std::true_type) noexcept
    {
        if (limbs == 1)
            data_[0] *= static_cast<limb_type>(rhs);
        else if (limbs == 2)
            detail::store_u128(data_, detail::load_u128(data_) * static_cast<unsigned __int128>(rhs));
        else
        {
            if (limbs > 4 && GINT_UNLIKELY(detail::limbs_zero_above<limbs>(data_, 1)))
            {
                // An unsigned limb times a signed 64-bit scalar fits in 128 bits.
                const auto product = static_cast<unsigned __int128>(data_[0]) * static_cast<unsigned __int128>(rhs);
                detail::store_u128(data_, product);
                if (rhs < 0)
                {
                    const limb_type fill = limb_type(0) - static_cast<limb_type>(product >> 127);
                    for (size_t i = 2; i < limbs; ++i)
                        data_[i] = fill;
                }
                return;
            }
            const limb_type bits = static_cast<limb_type>(rhs);
            // Taking the magnitude in the unsigned domain also handles INT64_MIN.
            const limb_type magnitude = rhs < 0 ? limb_type(0) - bits : bits;
            multiply_limb_scalar(magnitude, std::false_type{});
            if (rhs < 0)
                negate_in_place(*this);
        }
    }

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

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer add_copy(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
        detail::add_limbs_copy<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer sub_copy(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
        detail::sub_limbs_copy<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
    }

    enum class float_order
    {
        less = -1,
        equal,
        greater,
        unordered
    };

    template <typename T>
    static float_order compare_with_float(const integer & lhs, T rhs) noexcept
    {
        if (std::isnan(rhs))
            return float_order::unordered;
        if (std::isinf(rhs))
            return rhs > T(0) ? float_order::less : float_order::greater;

        const bool lhs_neg = std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63);
        if (rhs == T(0))
            return lhs_neg ? float_order::less : lhs.is_zero() ? float_order::equal : float_order::greater;

        const bool rhs_neg = std::signbit(rhs);
        if (lhs_neg != rhs_neg)
            return lhs_neg ? float_order::less : float_order::greater;

        const integer lhs_abs = lhs_neg ? -lhs : lhs;
        const T rhs_abs = rhs_neg ? -rhs : rhs;
        const int cmp = compare_with_float_abs(lhs_abs, rhs_abs);
        // The magnitude comparison returns -1, 0 or 1. Two negatives reverse the order.
        return static_cast<float_order>(lhs_neg ? -cmp : cmp);
    }

    template <typename T>
    void assign_float(T v) noexcept
    {
        if (std::isinf(v))
        {
            *this = v > 0 ? numeric_max() : numeric_min();
            return;
        }
        for (size_t i = 0; i < limbs; ++i)
            data_[i] = 0;
        if (v == 0 || std::isnan(v))
            return;
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
            if (hb < 64)
            {
                const Float res = static_cast<Float>(mag.data_[0]);
                return neg ? -res : res;
            }

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

    static size_t used_limbs(const integer & v) noexcept
    {
        size_t n = limbs;
        while (n > 0 && v.data_[n - 1] == 0)
            --n;
        return n;
    }

    template <typename T>
    static constexpr bool fits_signed_limb(T value) noexcept
    {
        return sizeof(T) <= sizeof(limb_type)
            && (!detail::is_unsigned<T>::value || value <= static_cast<T>(std::numeric_limits<signed_limb_type>::max()));
    }

    template <typename T>
    static constexpr bool uses_unsigned_limb_divisor() noexcept
    {
        return GINT_ARCH_X86_64 && limbs > 2 && std::is_same<Signed, unsigned>::value && detail::is_unsigned<T>::value
            && sizeof(T) <= sizeof(limb_type);
    }

    template <bool Remainder, typename T>
    static GINT_FORCE_INLINE bool infinite_divmod_operand(T value)
    {
        if (std::isnan(value))
            GINT_THROW(std::domain_error(Remainder ? "modulo by NaN" : "division by NaN"));
        return std::isinf(value);
    }

    template <typename T>
    static constexpr bool outside_uint64_operand_range(T value) noexcept
    {
        return Bits == 64 && std::is_same<Signed, unsigned>::value && sizeof(T) > sizeof(limb_type)
            && value != static_cast<T>(static_cast<limb_type>(value));
    }

    template <typename T>
    static constexpr bool is_int64_with_int128() noexcept
    {
        return Bits == 64 && std::is_same<Signed, signed>::value && std::is_same<T, __int128>::value;
    }

    static constexpr size_t promoted_signed_bits = Bits < 256 ? 256 : Bits;
    using promoted_signed_type = integer<promoted_signed_bits, signed>;

    template <typename T>
    static constexpr bool needs_unsigned_signed_promotion(T value) noexcept
    {
        return std::is_same<Signed, signed>::value && detail::is_unsigned<T>::value && Bits < 256
            && static_cast<unsigned __int128>(value) >= detail::signed_promotion_limit<Bits>::value();
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

    alignas((GINT_ARCH_AARCH64 || Bits == 64) ? alignof(limb_type) : 16) limb_type data_[limbs];
};

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint
