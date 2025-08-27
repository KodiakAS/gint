#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef GINT_ENABLE_FMT
#    include <fmt/format.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define GINT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#    define GINT_UNLIKELY(x) (x)
#endif

#if defined(GINT_ENABLE_DIVZERO_CHECKS)
#    define GINT_ZERO_CHECK(cond, msg) \
        do \
        { \
            if (GINT_UNLIKELY(cond)) \
                throw std::domain_error(msg); \
        } while (false)
#else
#    define GINT_ZERO_CHECK(cond, msg) ((void)0)
#endif
#define GINT_DIVZERO_CHECK(cond) GINT_ZERO_CHECK(cond, "division by zero")
#define GINT_MODZERO_CHECK(cond) GINT_ZERO_CHECK(cond, "modulo by zero")

namespace gint
{

//=== Forward declarations & type aliases ====================================
template <size_t Bits, typename Signed>
class integer;

using Int128 = integer<128, signed>;
using UInt128 = integer<128, unsigned>;
using Int256 = integer<256, signed>;
using UInt256 = integer<256, unsigned>;

//=== Internal helper utilities ==============================================
namespace detail
{
template <size_t Bits>
struct storage_count
{
    static_assert(Bits % 64 == 0, "Bits must be multiple of 64");
    static constexpr size_t value = Bits / 64;
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

template <size_t I>
struct limbs_equal
{
    template <typename Int>
    static constexpr bool eval(const Int & lhs, const Int & rhs) noexcept
    {
        return lhs.data_[I] == rhs.data_[I] && limbs_equal<I - 1>::eval(lhs, rhs);
    }
};

template <>
struct limbs_equal<0>
{
    template <typename Int>
    static constexpr bool eval(const Int & lhs, const Int & rhs) noexcept
    {
        return lhs.data_[0] == rhs.data_[0];
    }
};

template <size_t L>
inline void add_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 sum = static_cast<unsigned __int128>(lhs[i]) + rhs[i] + carry;
        lhs[i] = static_cast<uint64_t>(sum);
        carry = sum >> 64;
    }
}

template <>
inline void add_limbs<4>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 sum;
    sum = static_cast<unsigned __int128>(lhs[0]) + rhs[0];
    lhs[0] = static_cast<uint64_t>(sum);
    sum = static_cast<unsigned __int128>(lhs[1]) + rhs[1] + (sum >> 64);
    lhs[1] = static_cast<uint64_t>(sum);
    sum = static_cast<unsigned __int128>(lhs[2]) + rhs[2] + (sum >> 64);
    lhs[2] = static_cast<uint64_t>(sum);
    sum = static_cast<unsigned __int128>(lhs[3]) + rhs[3] + (sum >> 64);
    lhs[3] = static_cast<uint64_t>(sum);
}

template <size_t L>
inline void sub_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 lhs_i = lhs[i];
        unsigned __int128 subtrahend = static_cast<unsigned __int128>(rhs[i]) + borrow;
        lhs[i] = static_cast<uint64_t>(lhs_i - subtrahend);
        borrow = lhs_i < subtrahend;
    }
}

template <>
inline void sub_limbs<4>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 borrow = 0;
    unsigned __int128 lhs0 = lhs[0];
    unsigned __int128 rhs0 = rhs[0];
    lhs[0] = static_cast<uint64_t>(lhs0 - rhs0);
    borrow = lhs0 < rhs0;

    unsigned __int128 lhs1 = lhs[1];
    unsigned __int128 rhs1 = static_cast<unsigned __int128>(rhs[1]) + borrow;
    lhs[1] = static_cast<uint64_t>(lhs1 - rhs1);
    borrow = lhs1 < rhs1;

    unsigned __int128 lhs2 = lhs[2];
    unsigned __int128 rhs2 = static_cast<unsigned __int128>(rhs[2]) + borrow;
    lhs[2] = static_cast<uint64_t>(lhs2 - rhs2);
    borrow = lhs2 < rhs2;

    unsigned __int128 lhs3 = lhs[3];
    unsigned __int128 rhs3 = static_cast<unsigned __int128>(rhs[3]) + borrow;
    lhs[3] = static_cast<uint64_t>(lhs3 - rhs3);
}

// Perform 128-bit multiplication using a straightforward schoolbook
// method. The operands are split into low/high 64-bit limbs and cross
// multiplied with 128-bit intermediates to produce a 256-bit result.
inline void mul_128(uint64_t * res, const uint64_t * a, const uint64_t * b) noexcept
{
    unsigned __int128 a_lo = a[0];
    unsigned __int128 a_hi = a[1];
    unsigned __int128 b_lo = b[0];
    unsigned __int128 b_hi = b[1];

    unsigned __int128 lo = a_lo * b_lo;
    unsigned __int128 mid = a_lo * b_hi + a_hi * b_lo + (lo >> 64);
    unsigned __int128 hi = a_hi * b_hi + (mid >> 64);

    res[0] = static_cast<uint64_t>(lo);
    res[1] = static_cast<uint64_t>(mid);
    res[2] = static_cast<uint64_t>(hi);
    res[3] = static_cast<uint64_t>(hi >> 64);
}

template <size_t L>
inline void mul_limbs(uint64_t * res, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    // Generic O(n^2) schoolbook multiplication.
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 carry = 0;
        for (size_t j = 0; j + i < L; ++j)
        {
            unsigned __int128 cur = static_cast<unsigned __int128>(res[i + j]) + static_cast<unsigned __int128>(lhs[i]) * rhs[j] + carry;
            res[i + j] = static_cast<uint64_t>(cur);
            carry = cur >> 64;
        }
    }
}

template <>
inline void mul_limbs<4>(uint64_t * res, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    // Fast path: if the high limbs are zero, operands fit in 128 bits and we
    // can reuse the simpler 128-bit routine.
    if ((lhs[2] | lhs[3] | rhs[2] | rhs[3]) == 0)
    {
        mul_128(res, lhs, rhs);
        return;
    }

    // Otherwise use a Comba-style method: pack each pair of 64-bit limbs into a
    // 128-bit value and compute cross products to form the 256-bit result.
    using Half = unsigned __int128;
    // Pack limb pairs from lhs and rhs.
    Half a01 = (Half(lhs[1]) << 64) | lhs[0];
    Half a23 = (Half(lhs[3]) << 64) | lhs[2];
    Half b01 = (Half(rhs[1]) << 64) | rhs[0];
    Half b23 = (Half(rhs[3]) << 64) | rhs[2];

    // Compute cross products and assemble final limbs.
    Half r23 = a23 * b01 + a01 * b23 + Half(lhs[1]) * rhs[1];
    Half r01 = Half(lhs[0]) * rhs[0];
    Half r12 = (r01 >> 64) + (r23 << 64);
    Half cross = Half(lhs[1]) * rhs[0];

    res[0] = static_cast<uint64_t>(r01);
    res[3] = static_cast<uint64_t>(r23 >> 64);

    Half cross2 = Half(lhs[0]) * rhs[1];
    cross += cross2;
    if (cross < cross2)
        ++res[3];

    r12 += cross;
    if (r12 < cross)
        ++res[3];

    res[1] = static_cast<uint64_t>(r12);
    res[2] = static_cast<uint64_t>(r12 >> 64);
}

template <size_t L>
inline void mul_limb(uint64_t * lhs, uint64_t rhs) noexcept
{
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 cur = static_cast<unsigned __int128>(lhs[i]) * rhs + carry;
        lhs[i] = static_cast<uint64_t>(cur);
        carry = cur >> 64;
    }
}

template <>
inline void mul_limb<4>(uint64_t * lhs, uint64_t rhs) noexcept
{
    using Half = unsigned __int128;
    Half a01 = (Half(lhs[1]) << 64) | lhs[0];
    Half a23 = (Half(lhs[3]) << 64) | lhs[2];
    Half r23 = a23 * rhs;
    Half r01 = Half(lhs[0]) * rhs;
    Half r12 = (r01 >> 64) + (r23 << 64);
    Half cross = Half(lhs[1]) * rhs;

    lhs[0] = static_cast<uint64_t>(r01);
    lhs[3] = static_cast<uint64_t>(r23 >> 64);

    r12 += cross;
    if (r12 < cross)
        ++lhs[3];

    lhs[1] = static_cast<uint64_t>(r12);
    lhs[2] = static_cast<uint64_t>(r12 >> 64);
}
} // namespace detail

//=== String and stream declarations =========================================
template <size_t Bits, typename Signed>
std::string to_string(const integer<Bits, Signed> & value);

template <size_t Bits, typename Signed>
std::ostream & operator<<(std::ostream & out, const integer<Bits, Signed> & value);

//=== Core integer type ======================================================
template <size_t Bits, typename Signed>
class integer
{
public:
    static constexpr size_t limbs = detail::storage_count<Bits>::value;
    using limb_type = uint64_t;
    using signed_limb_type = int64_t;
    template <size_t>
    friend struct detail::limbs_equal;
    friend class std::numeric_limits<integer<Bits, Signed>>;

    // Constructors
    constexpr integer() noexcept = default;
    constexpr integer(const integer &) noexcept = default;
    constexpr integer(integer &&) noexcept = default;

    // Assignment operators
    constexpr integer & operator=(const integer &) noexcept = default;
    constexpr integer & operator=(integer &&) noexcept = default;

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

private:
    template <typename T, size_t... I>
    constexpr integer(T v, detail::index_sequence<I...>) noexcept
        : data_{limb_from<T, I>(v)...}
    {
    }

    template <typename T, size_t I>
    static constexpr limb_type limb_from(T v) noexcept
    {
        using wide = typename std::conditional<detail::is_signed<T>::value, __int128, unsigned __int128>::type;
        return I < (sizeof(T) * 8 + 63) / 64 ? static_cast<limb_type>(static_cast<unsigned __int128>(static_cast<wide>(v) >> (I * 64)))
                                             : (detail::is_signed<T>::value && v < 0 ? ~0ULL : 0ULL);
    }

public:
    // Assignment operators
    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    integer & operator=(T v) noexcept
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

    // Conversion operators
    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    explicit operator T() const noexcept
    {
        unsigned __int128 value = 0;
        for (size_t i = 0; i < limbs && i < (sizeof(T) + sizeof(limb_type) - 1) / sizeof(limb_type); ++i)
            value |= static_cast<unsigned __int128>(data_[i]) << (i * 64);

        if (detail::is_signed<T>::value)
            return static_cast<T>(static_cast<__int128>(value));
        else
            return static_cast<T>(value);
    }

    explicit operator long double() const noexcept
    {
        if (is_zero())
            return 0;
        bool neg = std::is_same<Signed, signed>::value && (data_[limbs - 1] >> 63);
        integer tmp = neg ? -*this : *this;
        long double res = 0;
        for (size_t i = limbs; i-- > 0;)
        {
            res *= std::ldexp(1.0L, 64);
            res += static_cast<long double>(tmp.data_[i]);
        }
        return neg ? -res : res;
    }

    explicit operator double() const noexcept { return static_cast<double>(static_cast<long double>(*this)); }

    explicit operator float() const noexcept { return static_cast<float>(static_cast<long double>(*this)); }

    explicit operator bool() const noexcept { return !is_zero(); }

    // Arithmetic assignment operators
    integer & operator+=(const integer & rhs) noexcept
    {
        detail::add_limbs<limbs>(data_, rhs.data_);
        return *this;
    }

    integer & operator-=(const integer & rhs) noexcept
    {
        detail::sub_limbs<limbs>(data_, rhs.data_);
        return *this;
    }

    integer & operator*=(const integer & rhs) noexcept
    {
        *this = *this * rhs;
        return *this;
    }

    integer & operator/=(const integer & rhs)
    {
        *this = *this / rhs;
        return *this;
    }

    integer & operator%=(const integer & rhs)
    {
        *this = *this % rhs;
        return *this;
    }

    // Bitwise assignment operators
    integer & operator&=(const integer & rhs) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] &= rhs.data_[i];
        return *this;
    }

    integer & operator|=(const integer & rhs) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] |= rhs.data_[i];
        return *this;
    }

    integer & operator^=(const integer & rhs) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] ^= rhs.data_[i];
        return *this;
    }

    // Shift operators
    integer & operator<<=(int n) noexcept
    {
        if (n <= 0)
            return *this;
        size_t total_bits = limbs * 64;
        size_t shift = static_cast<size_t>(n);
        if (shift >= total_bits)
        {
            for (size_t i = 0; i < limbs; ++i)
                data_[i] = 0;
            return *this;
        }
        size_t limb_shift = shift / 64;
        int bit_shift = shift % 64;
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

    integer & operator>>=(int n) noexcept
    {
        if (n <= 0)
            return *this;
        size_t total_bits = limbs * 64;
        size_t shift = static_cast<size_t>(n);
        if (shift >= total_bits)
        {
            for (size_t i = 0; i < limbs; ++i)
                data_[i] = 0;
            return *this;
        }
        size_t limb_shift = shift / 64;
        int bit_shift = shift % 64;
        if (limb_shift)
        {
            for (size_t i = 0; i < limbs - limb_shift; ++i)
                data_[i] = data_[i + limb_shift];
            for (size_t i = limbs - limb_shift; i < limbs; ++i)
                data_[i] = 0;
        }
        if (bit_shift)
        {
            for (size_t i = 0; i < limbs; ++i)
            {
                unsigned __int128 part = static_cast<unsigned __int128>(data_[i]) >> bit_shift;
                if (i + 1 < limbs)
                    part |= static_cast<unsigned __int128>(data_[i + 1]) << (64 - bit_shift);
                data_[i] = static_cast<limb_type>(part);
            }
        }
        return *this;
    }

    // Increment and decrement
    integer & operator++() noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
        {
            if (++data_[i])
                break;
        }
        return *this;
    }

    integer operator++(int) noexcept
    {
        integer tmp = *this;
        ++(*this);
        return tmp;
    }

    integer & operator--() noexcept
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

    integer operator--(int) noexcept
    {
        integer tmp = *this;
        --(*this);
        return tmp;
    }

    // Friend operators
    friend integer operator+(integer lhs, const integer & rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator+(integer lhs, T rhs) noexcept
    {
        lhs += integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator+(T lhs, integer rhs) noexcept
    {
        rhs += integer(lhs);
        return rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(integer lhs, T rhs) noexcept
    {
        long double res = static_cast<long double>(lhs);
        res += static_cast<long double>(rhs);
        return integer(res);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(T lhs, integer rhs) noexcept
    {
        long double res = static_cast<long double>(lhs);
        res += static_cast<long double>(rhs);
        return integer(res);
    }

    friend integer operator-(integer lhs, const integer & rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator-(integer lhs, T rhs) noexcept
    {
        lhs -= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator-(T lhs, integer rhs) noexcept
    {
        return integer(lhs) - rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(integer lhs, T rhs) noexcept
    {
        long double res = static_cast<long double>(lhs);
        res -= static_cast<long double>(rhs);
        return integer(res);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(T lhs, integer rhs) noexcept
    {
        long double res = static_cast<long double>(lhs);
        res -= static_cast<long double>(rhs);
        return integer(res);
    }

    friend integer operator&(integer lhs, const integer & rhs) noexcept
    {
        lhs &= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator&(integer lhs, T rhs) noexcept
    {
        lhs &= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator&(T lhs, integer rhs) noexcept
    {
        rhs &= integer(lhs);
        return rhs;
    }

    friend integer operator|(integer lhs, const integer & rhs) noexcept
    {
        lhs |= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator|(integer lhs, T rhs) noexcept
    {
        lhs |= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator|(T lhs, integer rhs) noexcept
    {
        rhs |= integer(lhs);
        return rhs;
    }

    friend integer operator^(integer lhs, const integer & rhs) noexcept
    {
        lhs ^= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator^(integer lhs, T rhs) noexcept
    {
        lhs ^= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator^(T lhs, integer rhs) noexcept
    {
        rhs ^= integer(lhs);
        return rhs;
    }

    friend integer operator<<(integer lhs, int n) noexcept
    {
        lhs <<= n;
        return lhs;
    }

    friend integer operator>>(integer lhs, int n) noexcept
    {
        lhs >>= n;
        return lhs;
    }

    friend integer operator*(const integer & lhs, const integer & rhs) noexcept
    {
        // Dispatch to the limb-wise multiplication routine which selects the
        // appropriate algorithm based on operand size.
        integer result{};
        detail::mul_limbs<limbs>(result.data_, lhs.data_, rhs.data_);
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
        long double res = static_cast<long double>(lhs);
        res *= static_cast<long double>(rhs);
        return integer(res);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator*(T lhs, integer rhs) noexcept
    {
        long double res = static_cast<long double>(lhs);
        res *= static_cast<long double>(rhs);
        return integer(res);
    }

    friend integer operator/(integer lhs, const integer & rhs)
    {
        bool lhs_neg = false;
        bool rhs_neg = false;
        integer divisor = rhs;
        if (std::is_same<Signed, signed>::value)
        {
            lhs_neg = lhs.data_[limbs - 1] >> 63;
            rhs_neg = divisor.data_[limbs - 1] >> 63;
            if (lhs_neg)
                lhs = -lhs;
            if (rhs_neg)
                divisor = -divisor;
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
            else if (divisor_limbs <= 2)
            {
                size_t lhs_limbs = limbs;
                while (lhs_limbs > 0 && lhs.data_[lhs_limbs - 1] == 0)
                    --lhs_limbs;
                if (lhs_limbs <= 2)
                {
                    // operands fit into two limbs; reuse the 128-bit path
                    result = div_128(lhs, divisor);
                }
                else if (limbs <= 4)
                {
                    // small operand size: fall back to Knuth division
                    result = div_large(lhs, divisor, divisor_limbs);
                }
                else
                {
                    // generic shift-subtract algorithm for very large numbers
                    result = div_shift_subtract(lhs, divisor);
                }
            }
            else if (limbs <= 4)
            {
                // up to 256-bit operands: Knuth division
                result = div_large(lhs, divisor, divisor_limbs);
            }
            else
            {
                // generic shift-subtract algorithm
                result = div_shift_subtract(lhs, divisor);
            }
        }
        if (std::is_same<Signed, signed>::value && lhs_neg != rhs_neg)
            result = -result;
        return result;
    }

    friend integer operator/(integer lhs, limb_type rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        if (rhs <= static_cast<limb_type>(std::numeric_limits<signed_limb_type>::max()))
            return lhs / static_cast<signed_limb_type>(rhs);
        return lhs / integer(rhs);
    }

    friend integer operator/(integer lhs, signed_limb_type rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        integer q;
        lhs.div_mod_small(rhs, q);
        return q;
    }

    friend integer operator/(limb_type lhs, integer rhs) { return integer(lhs) / rhs; }

    friend integer operator%(integer lhs, const integer & rhs)
    {
        GINT_MODZERO_CHECK(rhs.is_zero());
        integer q = lhs / rhs;
        q *= rhs;
        lhs -= q;
        return lhs;
    }

    friend integer operator%(integer lhs, limb_type rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        if (rhs <= static_cast<limb_type>(std::numeric_limits<signed_limb_type>::max()))
            return lhs % static_cast<signed_limb_type>(rhs);
        return lhs % integer(rhs);
    }

    friend integer operator%(integer lhs, signed_limb_type rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        integer q;
        signed_limb_type r = lhs.div_mod_small(rhs, q);
        return integer(r);
    }

    friend integer operator%(limb_type lhs, integer rhs) { return integer(lhs) % rhs; }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator/(integer lhs, T rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        if (sizeof(T) <= sizeof(limb_type)
            && (!detail::is_unsigned<T>::value || rhs <= static_cast<T>(std::numeric_limits<signed_limb_type>::max())))
            return lhs / static_cast<signed_limb_type>(rhs);
        return lhs / integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator/(T lhs, integer rhs)
    {
        return integer(lhs) / rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator/(integer lhs, T rhs)
    {
        GINT_DIVZERO_CHECK(rhs == 0);
        long double res = static_cast<long double>(lhs);
        res /= static_cast<long double>(rhs);
        return integer(res);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator/(T lhs, integer rhs)
    {
        GINT_DIVZERO_CHECK(rhs.is_zero());
        long double res = static_cast<long double>(lhs);
        res /= static_cast<long double>(rhs);
        return integer(res);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator%(integer lhs, T rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
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
    friend integer operator%(T lhs, integer rhs)
    {
        return integer(lhs) % rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator%(integer lhs, T rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        return lhs % integer(rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator%(T lhs, integer rhs)
    {
        return integer(lhs) % rhs;
    }

    friend constexpr bool operator==(const integer & lhs, const integer & rhs) noexcept
    {
        return detail::limbs_equal<limbs - 1>::eval(lhs, rhs);
    }

    friend constexpr bool operator!=(const integer & lhs, const integer & rhs) noexcept { return !(lhs == rhs); }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend constexpr bool operator==(const integer & lhs, T rhs) noexcept
    {
        return lhs == integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend constexpr bool operator==(T lhs, const integer & rhs) noexcept
    {
        return integer(lhs) == rhs;
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

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator==(const integer & lhs, T rhs) noexcept
    {
        return static_cast<long double>(lhs) == static_cast<long double>(rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator==(T lhs, const integer & rhs) noexcept
    {
        return static_cast<long double>(lhs) == static_cast<long double>(rhs);
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

    friend bool operator<(const integer & lhs, const integer & rhs) noexcept
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

    friend bool operator>(const integer & lhs, const integer & rhs) noexcept { return rhs < lhs; }

    friend bool operator<=(const integer & lhs, const integer & rhs) noexcept { return !(rhs < lhs); }

    friend bool operator>=(const integer & lhs, const integer & rhs) noexcept { return !(lhs < rhs); }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator<(const integer & lhs, T rhs) noexcept
    {
        return lhs < integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator<(T lhs, const integer & rhs) noexcept
    {
        return integer(lhs) < rhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator>(const integer & lhs, T rhs) noexcept
    {
        return integer(rhs) < lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator>(T lhs, const integer & rhs) noexcept
    {
        return rhs < integer(lhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator<=(const integer & lhs, T rhs) noexcept
    {
        return !(integer(rhs) < lhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator<=(T lhs, const integer & rhs) noexcept
    {
        return !(rhs < integer(lhs));
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator>=(const integer & lhs, T rhs) noexcept
    {
        return !(lhs < integer(rhs));
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend bool operator>=(T lhs, const integer & rhs) noexcept
    {
        return !(integer(lhs) < rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<(const integer & lhs, T rhs) noexcept
    {
        return static_cast<long double>(lhs) < static_cast<long double>(rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<(T lhs, const integer & rhs) noexcept
    {
        return static_cast<long double>(lhs) < static_cast<long double>(rhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>(const integer & lhs, T rhs) noexcept
    {
        return static_cast<long double>(rhs) < static_cast<long double>(lhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>(T lhs, const integer & rhs) noexcept
    {
        return static_cast<long double>(rhs) < static_cast<long double>(lhs);
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<=(const integer & lhs, T rhs) noexcept
    {
        return !(static_cast<long double>(rhs) < static_cast<long double>(lhs));
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator<=(T lhs, const integer & rhs) noexcept
    {
        return !(static_cast<long double>(rhs) < static_cast<long double>(lhs));
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>=(const integer & lhs, T rhs) noexcept
    {
        return !(static_cast<long double>(lhs) < static_cast<long double>(rhs));
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend bool operator>=(T lhs, const integer & rhs) noexcept
    {
        return !(static_cast<long double>(lhs) < static_cast<long double>(rhs));
    }

    friend integer operator~(integer v) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            v.data_[i] = ~v.data_[i];
        return v;
    }

    friend integer operator-(const integer & v) noexcept
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

    friend integer operator+(const integer & v) noexcept { return v; }

    friend std::string to_string<>(const integer & v);

private:
    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    void assign(T v) noexcept
    {
        using wide = typename std::conditional<detail::is_signed<T>::value, __int128, unsigned __int128>::type;
        wide val = static_cast<wide>(v);
        for (size_t i = 0; i < limbs; ++i)
        {
            data_[i] = static_cast<limb_type>(static_cast<unsigned __int128>(val));
            val >>= 64;
        }
    }

    template <typename T>
    void assign_float(T v) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            data_[i] = 0;
        if (v == 0)
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

    bool is_zero() const noexcept
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
    typename std::enable_if<(L > 1), limb_type>::type div_mod_small(limb_type div, integer & quotient) const noexcept
    {
        // This overload is only instantiated for multi-limb integers, preventing
        // compilers from inspecting out-of-bounds accesses in single-limb cases.
        quotient = integer();
        size_t n = limbs;
        while (n > 0 && data_[n - 1] == 0)
            --n;
        if (n == 0)
            return 0;
        if (n == 1)
        {
            quotient.data_[0] = static_cast<limb_type>(data_[0] / div);
            return static_cast<limb_type>(data_[0] % div);
        }
        if (n == 2)
        {
            unsigned __int128 num = (static_cast<unsigned __int128>(data_[1]) << 64) | data_[0];
            unsigned __int128 q = num / div;
            quotient.data_[0] = static_cast<limb_type>(q);
            quotient.data_[1] = static_cast<limb_type>(q >> 64);
            return static_cast<limb_type>(num % div);
        }
        unsigned __int128 rem = 0;
        for (size_t i = n; i-- > 0;)
        {
            rem = (rem << 64) | data_[i];
            quotient.data_[i] = static_cast<limb_type>(rem / div);
            rem %= div;
        }
        return static_cast<limb_type>(rem);
    }

    signed_limb_type div_mod_small(signed_limb_type div, integer & quotient) const noexcept
    {
        integer tmp = *this;
        bool lhs_neg = false;
        if (std::is_same<Signed, signed>::value && (tmp.data_[limbs - 1] >> 63))
        {
            tmp = -tmp;
            lhs_neg = true;
        }
        bool div_neg = div < 0;
        limb_type abs_div = div_neg ? static_cast<limb_type>(-div) : static_cast<limb_type>(div);
        signed_limb_type rem = static_cast<signed_limb_type>(tmp.div_mod_small(abs_div, quotient));
        if (lhs_neg)
            rem = -rem;
        if (lhs_neg != div_neg)
            quotient = -quotient;
        return rem;
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

    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), integer>::type div_128(const integer & lhs, const integer & rhs) noexcept
    {
        unsigned __int128 a = (static_cast<unsigned __int128>(lhs.data_[1]) << 64) | lhs.data_[0];
        unsigned __int128 b = (static_cast<unsigned __int128>(rhs.data_[1]) << 64) | rhs.data_[0];
        unsigned __int128 q = a / b;
        integer result;
        result.data_[0] = static_cast<limb_type>(q);
        result.data_[1] = static_cast<limb_type>(q >> 64);
        return result;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), integer>::type div_128(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
        result.data_[0] = lhs.data_[0] / rhs.data_[0];
        return result;
    }

    static integer div_shift_subtract(integer lhs, integer divisor) noexcept
    {
        integer result;
        int shift = lhs.highest_bit() - divisor.highest_bit();
        integer current(1);
        if (shift > 0)
        {
            divisor <<= shift;
            current <<= shift;
        }
        for (; shift >= 0; --shift)
        {
            if (!(lhs < divisor))
            {
                lhs -= divisor;
                result |= current;
            }
            divisor >>= 1;
            current >>= 1;
        }
        return result;
    }

    static integer div_large(integer lhs, const integer & divisor, size_t div_limbs) noexcept
    {
        integer quotient;
        size_t n = limbs;
        while (n > 0 && lhs.data_[n - 1] == 0)
            --n;
        if (n < div_limbs)
            return quotient;

        std::array<limb_type, limbs + 1> u = {};
        std::array<limb_type, limbs> v = {};

        int shift = __builtin_clzll(divisor.data_[div_limbs - 1]);
        limb_type carry = 0;
        for (size_t i = 0; i < n; ++i)
        {
            limb_type cur = lhs.data_[i];
            u[i] = (cur << shift) | carry;
            carry = shift ? static_cast<limb_type>(cur >> (64 - shift)) : 0;
        }
        u[n] = carry;

        carry = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            limb_type cur = divisor.data_[i];
            v[i] = (cur << shift) | carry;
            carry = shift ? static_cast<limb_type>(cur >> (64 - shift)) : 0;
        }

        for (int j = static_cast<int>(n - div_limbs); j >= 0; --j)
        {
            unsigned __int128 numerator = (static_cast<unsigned __int128>(u[j + div_limbs]) << 64) | u[j + div_limbs - 1];
            unsigned __int128 qhat = numerator / v[div_limbs - 1];
            unsigned __int128 rhat = numerator % v[div_limbs - 1];

            if (div_limbs > 1)
            {
                while (qhat == (static_cast<unsigned __int128>(1) << 64) || qhat * v[div_limbs - 2] > ((rhat << 64) | u[j + div_limbs - 2]))
                {
                    --qhat;
                    rhat += v[div_limbs - 1];
                    if (rhat >= (static_cast<unsigned __int128>(1) << 64))
                        break;
                }
            }

            unsigned __int128 borrow = 0;
            for (size_t i = 0; i < div_limbs; ++i)
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
            if (u[j + div_limbs] < static_cast<limb_type>(borrow))
            {
                unsigned __int128 carry2 = 0;
                for (size_t i = 0; i < div_limbs; ++i)
                {
                    unsigned __int128 t2 = static_cast<unsigned __int128>(u[j + i]) + v[i] + carry2;
                    u[j + i] = static_cast<limb_type>(t2);
                    carry2 = t2 >> 64;
                }
                u[j + div_limbs] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + div_limbs]) + carry2);
                --qhat;
            }
            else
            {
                u[j + div_limbs] = static_cast<limb_type>(static_cast<unsigned __int128>(u[j + div_limbs]) - borrow);
            }
            quotient.data_[j] = static_cast<limb_type>(qhat);
        }
        return quotient;
    }

    limb_type data_[limbs] = {};
};

} // namespace gint

namespace std
{
template <size_t Bits, typename Signed>
class numeric_limits<gint::integer<Bits, Signed>>
{
public:
    static const bool is_specialized = true;
    static const bool is_signed = std::is_same<Signed, signed>::value;
    static const bool is_integer = true;
    static const bool is_exact = true;
    static const bool has_infinity = false;
    static const bool has_quiet_NaN = false;
    static const bool has_signaling_NaN = true;
    static const bool has_denorm_loss = false;
    static const std::float_round_style round_style = std::round_toward_zero;
    static const bool is_iec559 = false;
    static const bool is_bounded = true;
    static const bool is_modulo = true;
    static const int digits = Bits - (is_signed ? 1 : 0);
    static const int digits10 = digits * 30103 / 100000;
    static const int max_digits10 = 0;
    static const int radix = 2;
    static const int min_exponent = 0;
    static const int min_exponent10 = 0;
    static const int max_exponent = 0;
    static const int max_exponent10 = 0;
    static const bool traps = true;
    static const bool tinyness_before = false;

    static gint::integer<Bits, Signed> min() noexcept
    {
        if (is_signed)
        {
            typedef gint::integer<Bits, Signed> T;
            T res;
            res.data_[T::limbs - 1] = static_cast<typename T::limb_type>(1ULL << 63);
            return res;
        }
        return gint::integer<Bits, Signed>(0);
    }

    static gint::integer<Bits, Signed> max() noexcept
    {
        if (is_signed)
            return ~min();
        typedef gint::integer<Bits, Signed> T;
        T res;
        for (size_t i = 0; i < T::limbs; ++i)
            res.data_[i] = std::numeric_limits<typename T::limb_type>::max();
        return res;
    }

    static gint::integer<Bits, Signed> lowest() noexcept { return min(); }
    static gint::integer<Bits, Signed> epsilon() noexcept { return gint::integer<Bits, Signed>(0); }
    static gint::integer<Bits, Signed> round_error() noexcept { return gint::integer<Bits, Signed>(0); }
    static gint::integer<Bits, Signed> infinity() noexcept { return gint::integer<Bits, Signed>(0); }
    static gint::integer<Bits, Signed> quiet_NaN() noexcept { return gint::integer<Bits, Signed>(0); }
    static gint::integer<Bits, Signed> signaling_NaN() noexcept { return gint::integer<Bits, Signed>(0); }
    static gint::integer<Bits, Signed> denorm_min() noexcept { return gint::integer<Bits, Signed>(0); }
};
} // namespace std

namespace gint
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
    std::string res;
    while (!tmp.is_zero())
    {
        integer<Bits, Signed> q;
        typename integer<Bits, Signed>::limb_type rem = tmp.div_mod_small(static_cast<typename integer<Bits, Signed>::limb_type>(10), q);
        res.insert(res.begin(), static_cast<char>('0' + rem));
        tmp = q;
    }
    if (neg)
        res.insert(res.begin(), '-');
    return res;
}

template <size_t Bits, typename Signed>
inline std::ostream & operator<<(std::ostream & out, const integer<Bits, Signed> & value)
{
    return out << to_string(value);
}

} // namespace gint

#ifdef GINT_ENABLE_FMT
namespace fmt
{
template <size_t Bits, typename Signed>
struct formatter<gint::integer<Bits, Signed>>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext & ctx) -> typename ParseContext::iterator
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const gint::integer<Bits, Signed> & value, FormatContext & ctx) const -> typename FormatContext::iterator
    {
        return fmt::format_to(ctx.out(), "{}", gint::to_string(value));
    }
};
} // namespace fmt
#endif
