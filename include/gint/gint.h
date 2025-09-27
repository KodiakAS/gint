#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#ifdef GINT_ENABLE_FMT
#    include <fmt/format.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define GINT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#    define GINT_LIKELY(x) __builtin_expect(!!(x), 1)
#    define GINT_FORCE_INLINE inline __attribute__((always_inline))
#else
#    define GINT_UNLIKELY(x) (x)
#    define GINT_LIKELY(x) (x)
#    define GINT_FORCE_INLINE inline
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

#if __cplusplus >= 201402L
#    define GINT_CONSTEXPR14 constexpr
#else
#    define GINT_CONSTEXPR14
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define GINT_RESTRICT __restrict
#else
#    define GINT_RESTRICT
#endif

// Fast-path policy for multiplication
// Rationale:
// - GCC (both x86_64 and AArch64) benefits from small-number fast paths
//   (e.g., 64x64, operands <=128-bit, single-limb), often yielding fewer
//   µops and shorter dependency chains.
// - AppleClang on AArch64 tends to generate excellent straight-line code for
//   the general path, and the extra branching/ICache footprint of fast paths
//   can outweigh their savings. Default: disable under Clang.
// Allow users to override via -DGINT_ENABLE_MUL_FASTPATH=0/1.
#ifndef GINT_ENABLE_MUL_FASTPATH
#    if defined(__clang__)
#        define GINT_ENABLE_MUL_FASTPATH 0
#    else
#        define GINT_ENABLE_MUL_FASTPATH 1
#    endif
#endif

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
    static_assert(Bits > 0, "Bits must be > 0");
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
// Compute the high 128 bits of a 128x128->256 multiplication using
// 64x64 partial products. Inlines well on GCC/Clang and maps to umulh
// on AArch64 and efficient MUL+SHRD sequences on x86_64.
inline unsigned __int128 mulhi_u128(unsigned __int128 a, unsigned __int128 b) noexcept
{
    const unsigned __int128 a0 = static_cast<uint64_t>(a);
    const unsigned __int128 a1 = a >> 64;
    const unsigned __int128 b0 = static_cast<uint64_t>(b);
    const unsigned __int128 b1 = b >> 64;
    const unsigned __int128 t0 = a0 * b0;
    const unsigned __int128 t1 = a0 * b1;
    const unsigned __int128 t2 = a1 * b0;
    const unsigned __int128 t3 = a1 * b1;
    const unsigned __int128 s = (t0 >> 64) + t1 + t2;
    return t3 + (s >> 64);
}
// Add two 64-bit unsigned values and accumulate carry count (0 or 1) into c.
// Returns the 64-bit sum; c is incremented if overflow occurs.
inline uint64_t addc64(uint64_t a, uint64_t b, uint64_t & c) noexcept
{
    uint64_t s = a + b;
    c += (s < a);
    return s;
}


template <size_t L>
GINT_CONSTEXPR14 inline void add_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
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
GINT_CONSTEXPR14 inline void add_limbs<4>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 sum = 0;
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
GINT_CONSTEXPR14 inline void sub_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
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
GINT_CONSTEXPR14 inline void sub_limbs<4>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    uint64_t r0 = lhs[0] - rhs[0];
    bool b0 = lhs[0] < rhs[0];

    uint64_t r1 = lhs[1] - rhs[1];
    bool b1 = lhs[1] < rhs[1];

    uint64_t r2 = lhs[2] - rhs[2];
    bool b2 = lhs[2] < rhs[2];

    uint64_t r3 = lhs[3] - rhs[3];

    if (b0)
    {
        if (r1 == 0)
        {
            b1 = true;
            r1 = UINT64_MAX;
        }
        else
            --r1;
    }

    if (b1)
    {
        if (r2 == 0)
        {
            b2 = true;
            r2 = UINT64_MAX;
        }
        else
            --r2;
    }

    if (b2)
        --r3;

    lhs[0] = r0;
    lhs[1] = r1;
    lhs[2] = r2;
    lhs[3] = r3;
}

// Perform 128-bit multiplication using a straightforward schoolbook
// method. The operands are split into low/high 64-bit limbs and cross
// multiplied with 128-bit intermediates to produce a 256-bit result.
template <size_t L>
GINT_FORCE_INLINE void mul_limbs(uint64_t * res, const uint64_t * lhs, const uint64_t * rhs) noexcept
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

// Fast path for 128-bit (2-limb) multiplication: leverage the dedicated
// 128x128->256 routine and keep the low 128 bits (fixed-width semantics).
template <>
GINT_FORCE_INLINE void mul_limbs<2>(uint64_t * res, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    // 128-bit specialized schoolbook: decompose into 64-bit limbs and
    // accumulate cross products with 128-bit carries. Keep low 128 bits.
    using u128 = unsigned __int128;
    const u128 a0 = lhs[0];
    const u128 a1 = lhs[1];
    const u128 b0 = rhs[0];
    const u128 b1 = rhs[1];

    const u128 p00 = a0 * b0; // contributes to res[0] (low) and carry to res[1]
    const u128 p01 = a0 * b1; // contributes to res[1]
    const u128 p10 = a1 * b0; // contributes to res[1]

    uint64_t lo = static_cast<uint64_t>(p00);
    u128 carry = (p00 >> 64);
    u128 sum1 = carry + p01 + p10;

    res[0] = lo;
    res[1] = static_cast<uint64_t>(sum1);
}

template <>
GINT_FORCE_INLINE void
mul_limbs<4>(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    using u128 = unsigned __int128;
#if GINT_ENABLE_MUL_FASTPATH
    // Fast path: if both operands fit within 128 bits we can reduce to the
    // specialized 128-bit multiplication or even a single 64-bit multiply.
    if ((lhs[2] | lhs[3] | rhs[2] | rhs[3]) == 0)
    {
        if ((lhs[1] | rhs[1]) == 0)
        {
            u128 p = u128(lhs[0]) * rhs[0];
            res[0] = static_cast<uint64_t>(p);
            res[1] = static_cast<uint64_t>(p >> 64);
            res[2] = 0;
            res[3] = 0;
        }
        else
        {
            const u128 a0 = lhs[0];
            const u128 a1 = lhs[1];
            const u128 b0 = rhs[0];
            const u128 b1 = rhs[1];

            u128 p00 = a0 * b0;
            u128 p01 = a0 * b1;
            u128 p10 = a1 * b0;
            u128 p11 = a1 * b1;

            res[0] = static_cast<uint64_t>(p00);
            u128 sum = (p00 >> 64) + static_cast<uint64_t>(p01) + static_cast<uint64_t>(p10);
            res[1] = static_cast<uint64_t>(sum);
            u128 carry = sum >> 64;

            u128 high = p11;
            uint64_t high_carry = 0;
            u128 prev = high;
            high += (p01 >> 64);
            if (high < prev)
                ++high_carry;
            prev = high;
            high += (p10 >> 64);
            if (high < prev)
                ++high_carry;
            prev = high;
            high += carry;
            if (high < prev)
                ++high_carry;

            res[2] = static_cast<uint64_t>(high);
            res[3] = static_cast<uint64_t>(high >> 64) + high_carry;
        }
        return;
    }

    // One-operand single-limb fast paths
    if ((rhs[1] | rhs[2] | rhs[3]) == 0)
    {
        // res = lhs * rhs[0]
        u128 carry = 0;
        uint64_t k = rhs[0];
        for (size_t i = 0; i < 4; ++i)
        {
            u128 cur = u128(lhs[i]) * k + carry;
            res[i] = static_cast<uint64_t>(cur);
            carry = cur >> 64;
        }
        return;
    }
    if ((lhs[1] | lhs[2] | lhs[3]) == 0)
    {
        // res = rhs * lhs[0]
        u128 carry = 0;
        uint64_t k = lhs[0];
        for (size_t i = 0; i < 4; ++i)
        {
            u128 cur = u128(rhs[i]) * k + carry;
            res[i] = static_cast<uint64_t>(cur);
            carry = cur >> 64;
        }
        return;
    }
#endif
    // General 4-limb path: Batched diagonal accumulation with 128-bit
    // intermediates for compact instruction sequences on GCC/Clang.
    const uint64_t a0 = lhs[0], a1 = lhs[1], a2 = lhs[2], a3 = lhs[3];
    const uint64_t b0 = rhs[0], b1 = rhs[1], b2 = rhs[2], b3 = rhs[3];

    u128 carry = 0; // high 64 carries, low 64 is the starting low word

    // k = 0
    {
        u128 p00 = u128(a0) * b0;
        u128 lo_acc = u128(static_cast<uint64_t>(carry)) + static_cast<uint64_t>(p00);
        uint64_t lo = static_cast<uint64_t>(lo_acc);
        u128 hi_acc = (carry >> 64) + (p00 >> 64) + (lo_acc >> 64);
        res[0] = lo;
        carry = hi_acc;
    }

    // k = 1: a0*b1 + a1*b0 (hybrid low add chain)
    {
        u128 p01 = u128(a0) * b1;
        u128 p10 = u128(a1) * b0;
        uint64_t c = 0;
        uint64_t lo = detail::addc64(static_cast<uint64_t>(carry), static_cast<uint64_t>(p01), c);
        lo = detail::addc64(lo, static_cast<uint64_t>(p10), c);
        u128 hi_acc = (carry >> 64) + (p01 >> 64) + (p10 >> 64) + c;
        res[1] = lo;
        carry = hi_acc;
    }

    // k = 2: a0*b2 + a1*b1 + a2*b0 (hybrid low add chain)
    {
        u128 p02 = u128(a0) * b2;
        u128 p11 = u128(a1) * b1;
        u128 p20 = u128(a2) * b0;
        uint64_t c = 0;
        uint64_t lo = detail::addc64(static_cast<uint64_t>(carry), static_cast<uint64_t>(p02), c);
        lo = detail::addc64(lo, static_cast<uint64_t>(p11), c);
        lo = detail::addc64(lo, static_cast<uint64_t>(p20), c);
        u128 hi_acc = (carry >> 64) + (p02 >> 64) + (p11 >> 64) + (p20 >> 64) + c;
        res[2] = lo;
        carry = hi_acc;
    }

    // k = 3: a0*b3 + a1*b2 + a2*b1 + a3*b0
    {
        u128 p03 = u128(a0) * b3;
        u128 p12 = u128(a1) * b2;
        u128 p21 = u128(a2) * b1;
        u128 p30 = u128(a3) * b0;
        u128 lo_add = u128(static_cast<uint64_t>(p03)) + static_cast<uint64_t>(p12);
        lo_add += static_cast<uint64_t>(p21);
        lo_add += static_cast<uint64_t>(p30);
        u128 lo_acc = u128(static_cast<uint64_t>(carry)) + lo_add;
        res[3] = static_cast<uint64_t>(lo_acc);
    }
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

// Prefer the generic mul_limb implementation also for 4 limbs.
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
    // Constrain Signed to be exactly 'signed' or 'unsigned' tag types.
    static_assert(std::is_same<Signed, signed>::value || std::is_same<Signed, unsigned>::value, "Signed must be 'signed' or 'unsigned'.");
    template <size_t>
    friend struct detail::limbs_equal;
    friend class std::numeric_limits<integer<Bits, Signed>>;

    // Constructors
    constexpr integer() noexcept = default;
    constexpr integer(const integer &) noexcept = default;
    constexpr integer(integer &&) noexcept = default;

    // Assignment operators
    GINT_CONSTEXPR14 integer & operator=(const integer &) noexcept = default;
    GINT_CONSTEXPR14 integer & operator=(integer &&) noexcept = default;

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
    integer & operator/=(const integer & rhs)
    {
        *this = *this / rhs;
        return *this;
    }

    // Note: %= is not constexpr because of optional zero-checks and complexity
    integer & operator%=(const integer & rhs)
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
    GINT_CONSTEXPR14 integer & operator<<=(int n) noexcept
    {
        if (n <= 0)
            return *this; // negative and zero shifts are no-ops by design
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

    GINT_CONSTEXPR14 integer & operator>>=(int n) noexcept
    {
        if (n <= 0)
            return *this; // negative and zero shifts are no-ops by design
        const bool is_signed_t = std::is_same<Signed, signed>::value;
        const bool neg = is_signed_t && (data_[limbs - 1] >> 63);
        size_t total_bits = limbs * 64;
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
        int bit_shift = shift % 64;
        if (limb_shift)
        {
            for (size_t i = 0; i < limbs - limb_shift; ++i)
                data_[i] = data_[i + limb_shift];
            for (size_t i = limbs - limb_shift; i < limbs; ++i)
                data_[i] = neg ? ~limb_type(0) : limb_type(0);
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
            // Sign-extend vacated high bits for negative signed values.
            if (neg)
            {
                data_[limbs - 1] |= (~limb_type(0)) << (64 - bit_shift);
            }
        }
        return *this;
    }

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
    GINT_CONSTEXPR14 friend integer operator+(integer lhs, const integer & rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator+(integer lhs, T rhs) noexcept
    {
        lhs += integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator+(T lhs, integer rhs) noexcept
    {
        rhs += integer(lhs);
        return rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(integer lhs, T rhs) noexcept
    {
        lhs += integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator+(T lhs, integer rhs) noexcept
    {
        rhs += integer(lhs);
        return rhs;
    }

    GINT_CONSTEXPR14 friend integer operator-(integer lhs, const integer & rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator-(integer lhs, T rhs) noexcept
    {
        lhs -= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 integer operator-(T lhs, integer rhs) noexcept
    {
        return integer(lhs) - rhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(integer lhs, T rhs) noexcept
    {
        lhs -= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator-(T lhs, integer rhs) noexcept
    {
        return integer(lhs) - rhs;
    }

    GINT_CONSTEXPR14 friend integer operator&(integer lhs, const integer & rhs) noexcept
    {
        lhs &= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator&(integer lhs, T rhs) noexcept
    {
        lhs &= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator&(T lhs, integer rhs) noexcept
    {
        rhs &= integer(lhs);
        return rhs;
    }

    GINT_CONSTEXPR14 friend integer operator|(integer lhs, const integer & rhs) noexcept
    {
        lhs |= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator|(integer lhs, T rhs) noexcept
    {
        lhs |= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator|(T lhs, integer rhs) noexcept
    {
        rhs |= integer(lhs);
        return rhs;
    }

    GINT_CONSTEXPR14 friend integer operator^(integer lhs, const integer & rhs) noexcept
    {
        lhs ^= rhs;
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator^(integer lhs, T rhs) noexcept
    {
        lhs ^= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator^(T lhs, integer rhs) noexcept
    {
        rhs ^= integer(lhs);
        return rhs;
    }

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

    // Multiplication is left non-constexpr due to helper internals
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
        lhs *= integer(rhs);
        return lhs;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator*(T lhs, integer rhs) noexcept
    {
        rhs *= integer(lhs);
        return rhs;
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
            else
            {
                // Multi-limb divisor: use Knuth's Algorithm D (div_large)
                result = div_large(lhs, divisor, divisor_limbs);
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
        if (std::isnan(rhs))
            throw std::domain_error("division by NaN");
        if (std::isinf(rhs))
            return integer(); // finite / ±inf -> 0
        integer div(rhs);
        GINT_DIVZERO_CHECK(div.is_zero());
        return lhs / div;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator/(T lhs, integer rhs)
    {
        if (std::isnan(lhs))
            throw std::domain_error("division by NaN");
        if (std::isinf(lhs))
            throw std::domain_error("infinite dividend");
        GINT_DIVZERO_CHECK(rhs.is_zero());
        return integer(lhs) / rhs;
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
        if (std::isnan(rhs))
            throw std::domain_error("modulo by NaN");
        if (std::isinf(rhs))
            return lhs; // finite % ±inf -> lhs
        integer div(rhs);
        GINT_MODZERO_CHECK(div.is_zero());
        return lhs % div;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    friend integer operator%(T lhs, integer rhs)
    {
        if (std::isnan(lhs))
            throw std::domain_error("modulo by NaN");
        if (std::isinf(lhs))
            throw std::domain_error("infinite dividend in modulo");
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
        if (shift > 0)
            scaled >>= shift;
        else if (shift < 0)
            scaled <<= -shift;
        unsigned __int128 sigA = 0;
        if (limbs >= 2)
            sigA = (static_cast<unsigned __int128>(scaled.data_[1]) << 64) | scaled.data_[0];
        else
            sigA = scaled.data_[0];
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
    friend GINT_CONSTEXPR14 bool operator<(const integer & lhs, T rhs) noexcept
    {
        return lhs < integer(rhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<(T lhs, const integer & rhs) noexcept
    {
        return integer(lhs) < rhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>(const integer & lhs, T rhs) noexcept
    {
        return integer(rhs) < lhs;
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>(T lhs, const integer & rhs) noexcept
    {
        return rhs < integer(lhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<=(const integer & lhs, T rhs) noexcept
    {
        return !(integer(rhs) < lhs);
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator<=(T lhs, const integer & rhs) noexcept
    {
        return !(rhs < integer(lhs));
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>=(const integer & lhs, T rhs) noexcept
    {
        return !(lhs < integer(rhs));
    }

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend GINT_CONSTEXPR14 bool operator>=(T lhs, const integer & rhs) noexcept
    {
        return !(integer(lhs) < rhs);
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
        // Single correction (branch); empirically最稳定的实现。
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
                    u128 q = detail::mulhi_u128(num, inv);
                    u128 rem = num - q * div;
                    corr(q, rem);
                    quotient.data_[0] = static_cast<limb_type>(q);
                    return static_cast<limb_type>(rem);
                }
                case 2: {
                    u128 num = (static_cast<u128>(data_[1]) << 64) | data_[0];
                    u128 q = detail::mulhi_u128(num, inv);
                    u128 rem = num - q * div;
                    corr(q, rem);
                    quotient.data_[0] = static_cast<limb_type>(q);
                    quotient.data_[1] = static_cast<limb_type>(q >> 64);
                    return static_cast<limb_type>(rem);
                }
                case 3: {
                    u128 rem = 0;
                    u128 num = (rem << 64) | data_[2];
                    u128 q2 = detail::mulhi_u128(num, inv);
                    rem = num - q2 * div;
                    corr(q2, rem);
                    quotient.data_[2] = static_cast<limb_type>(q2);
                    num = (rem << 64) | data_[1];
                    u128 q1 = detail::mulhi_u128(num, inv);
                    rem = num - q1 * div;
                    corr(q1, rem);
                    quotient.data_[1] = static_cast<limb_type>(q1);
                    num = (rem << 64) | data_[0];
                    u128 q0 = detail::mulhi_u128(num, inv);
                    rem = num - q0 * div;
                    corr(q0, rem);
                    quotient.data_[0] = static_cast<limb_type>(q0);
                    return static_cast<limb_type>(rem);
                }
                case 4:
                default: {
                    u128 rem = 0;
                    u128 num = (rem << 64) | data_[3];
                    u128 q3 = detail::mulhi_u128(num, inv);
                    rem = num - q3 * div;
                    corr(q3, rem);
                    quotient.data_[3] = static_cast<limb_type>(q3);
                    num = (rem << 64) | data_[2];
                    u128 q2 = detail::mulhi_u128(num, inv);
                    rem = num - q2 * div;
                    corr(q2, rem);
                    quotient.data_[2] = static_cast<limb_type>(q2);
                    num = (rem << 64) | data_[1];
                    u128 q1 = detail::mulhi_u128(num, inv);
                    rem = num - q1 * div;
                    corr(q1, rem);
                    quotient.data_[1] = static_cast<limb_type>(q1);
                    num = (rem << 64) | data_[0];
                    u128 q0 = detail::mulhi_u128(num, inv);
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
            u128 q = detail::mulhi_u128(num, inv);
            rem = num - q * div;
            corr(q, rem);
            quotient.data_[i] = static_cast<limb_type>(q);
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
        limb_type abs_div = static_cast<limb_type>(div);
        if (div_neg)
            abs_div = limb_type(0) - abs_div;
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
        limb_type carry = lshift_limbs_to(lhs.data_, n, u.data(), shift);
        u[n] = carry;

        carry = lshift_limbs_to(divisor.data_, div_limbs, v.data(), shift);

        for (int j = static_cast<int>(n - div_limbs); j >= 0; --j)
        {
            unsigned __int128 numerator = (static_cast<unsigned __int128>(u[j + div_limbs]) << 64) | u[j + div_limbs - 1];
            // Single 128/64 division: compute quotient, derive remainder by multiply-back
            unsigned __int128 qhat = numerator / v[div_limbs - 1];
            unsigned __int128 rhat = numerator - qhat * v[div_limbs - 1];

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

    // Optimized specialization: two-limb divisor (divisor_limbs == 2)
    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), integer>::type div_large_2(integer lhs, const integer & divisor) noexcept
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

        if (n == 4)
        {
            auto step = [&](int j)
            {
                limb_type & uj0 = u[j + 0];
                limb_type & uj1 = u[j + 1];
                limb_type & uj2 = u[j + 2];
                u128 numerator = (static_cast<u128>(uj2) << 64) | uj1;
                // 1) Initial estimate via reciprocal multiply
                u128 qhat = detail::mulhi_u128(numerator, inv128);
                const u128 QMAX = (static_cast<u128>(1) << 64) - 1;
                if (qhat > QMAX)
                    qhat = QMAX;
                u128 qhat_v1 = qhat * v1;
                // Downward correction to ensure qhat <= floor(numerator / v1)
                if (qhat_v1 > numerator)
                {
                    --qhat;
                    qhat_v1 -= v1;
                }
                // Upward correction to ensure qhat >= floor(numerator / v1)
                else if ((numerator - qhat_v1) >= v1)
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
                limb_type borrow_hi = static_cast<limb_type>(static_cast<unsigned __int128>(borrow) >> 64);
                if (uj2 < borrow_hi)
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
                    uj2 = static_cast<limb_type>(static_cast<unsigned __int128>(uj2) - borrow_hi);
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
                u128 qhat = detail::mulhi_u128(numerator, inv128);
                const u128 QMAX = (static_cast<u128>(1) << 64) - 1;
                if (qhat > QMAX)
                    qhat = QMAX;
                u128 qhat_v1 = qhat * v1;
                // Downward correction
                if (qhat_v1 > numerator)
                {
                    --qhat;
                    qhat_v1 -= v1;
                }
                // Upward correction
                else if ((numerator - qhat_v1) >= v1)
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
                limb_type borrow_hi = static_cast<limb_type>(static_cast<unsigned __int128>(borrow) >> 64);
                if (uj2 < borrow_hi)
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
                    uj2 = static_cast<limb_type>(static_cast<unsigned __int128>(uj2) - borrow_hi);
                }
                quotient.data_[j] = static_cast<limb_type>(qhat);
            }
        }
        return quotient;
    }

    // Stub for small-limb types to avoid -Warray-bounds during template instantiation
    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), integer>::type div_large_2(integer lhs, const integer & divisor) noexcept
    {
        // Not reachable for L < 2 because divisor_limbs cannot be 2. Keep a safe fallback signature.
        return div_large(lhs, divisor, 2);
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

            if (u[j + 3] < static_cast<limb_type>(borrow))
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

    // Stub for small-limb types to avoid -Warray-bounds during template instantiation
    template <size_t L = limbs>
    static typename std::enable_if<(L < 3), integer>::type div_large_3(integer lhs, const integer & divisor) noexcept
    {
        // Not reachable for L < 3 because divisor_limbs cannot be 3. Keep a safe fallback signature.
        return div_large(lhs, divisor, 3);
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
    static const bool has_signaling_NaN = false;
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

    using Int = integer<Bits, Signed>;
    const typename Int::limb_type base = 10000000000000000000ULL; // 1e19
    const unsigned chunk_digits = 19;

    std::vector<typename Int::limb_type> chunks;
    chunks.reserve((Bits + 62) / 63);
    while (!tmp.is_zero())
    {
        Int q;
        typename Int::limb_type rem = tmp.div_mod_small(base, q);
        chunks.push_back(rem);
        tmp = q;
    }

    std::string out;
    out.reserve(chunks.size() * chunk_digits + 1);
    // most significant chunk
    auto it = chunks.rbegin();
    {
        char buf[32];
        unsigned idx = 32;
        typename Int::limb_type x = *it;
        while (x)
        {
            buf[--idx] = static_cast<char>('0' + (x % 10));
            x /= 10;
        }
        out.append(buf + idx, 32 - idx);
        ++it;
    }
    // zero-padded remaining chunks
    for (; it != chunks.rend(); ++it)
    {
        char buf[32];
        unsigned idx = 32;
        typename Int::limb_type x = *it;
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

//=== Macro cleanup =============================================================

#undef GINT_UNLIKELY
#undef GINT_LIKELY
#undef GINT_ZERO_CHECK
#undef GINT_DIVZERO_CHECK
#undef GINT_MODZERO_CHECK
#undef GINT_CONSTEXPR14
#undef GINT_FORCE_INLINE
#undef GINT_RESTRICT
#undef GINT_ENABLE_MUL_FASTPATH
