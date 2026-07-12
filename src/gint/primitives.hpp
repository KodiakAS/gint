#pragma once

#include "configuration.hpp"

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED

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

//=== Internal helper utilities ==============================================
namespace detail
{
#    ifdef GINT_TEST_ACCESS
template <size_t Bits, typename Signed>
struct integer_test_access;
#    endif

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

GINT_FORCE_INLINE bool limbs_equal_runtime_1024(const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    if (lhs[15] != rhs[15])
        return false;

    uint64_t difference = 0;
    for (size_t i = 0; i < 15; ++i)
        difference |= lhs[i] ^ rhs[i];
    return difference == 0;
}
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
    const unsigned __int128 mid = (t0 >> 64) + static_cast<uint64_t>(t1) + static_cast<uint64_t>(t2);
    return t3 + (t1 >> 64) + (t2 >> 64) + (mid >> 64);
}

// Fast high-half multiply for division reciprocal paths where the middle
// accumulation is proven not to wrap modulo 2^128. Use mulhi_u128 for the
// general 128x128->256 high-half contract.
inline unsigned __int128 mulhi_u128_no_middle_wrap(unsigned __int128 a, unsigned __int128 b) noexcept
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
GINT_CONSTEXPR14 inline void add_limbs_copy_scalar(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <size_t L>
GINT_FORCE_INLINE void add_limbs_copy_runtime(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <>
GINT_FORCE_INLINE void add_limbs_copy_runtime<1>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <>
GINT_FORCE_INLINE void add_limbs_copy_runtime<2>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <>
GINT_FORCE_INLINE void add_limbs_copy_runtime<4>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <size_t L>
GINT_CONSTEXPR14 inline void sub_limbs_copy_scalar(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <size_t L>
GINT_FORCE_INLINE void sub_limbs_copy_runtime(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <>
GINT_FORCE_INLINE void sub_limbs_copy_runtime<2>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <>
GINT_FORCE_INLINE void sub_limbs_copy_runtime<4>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept;

template <size_t L>
GINT_CONSTEXPR14 inline void add_limbs_copy_scalar(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 sum = static_cast<unsigned __int128>(lhs[i]) + rhs[i] + carry;
        dst[i] = static_cast<uint64_t>(sum);
        carry = sum >> 64;
    }
}

template <size_t L>
GINT_FORCE_INLINE void add_limbs_copy_runtime(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
#    if GINT_DETAIL_X86_64_GCC || GINT_DETAIL_X86_64_CLANG
    unsigned char carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        carry = _addcarry_u64(carry, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        dst[i] = static_cast<uint64_t>(r);
    }
    return;
#    elif GINT_DETAIL_AARCH64_CLANG && GINT_DETAIL_HAS_BUILTIN(__builtin_addcll) && GINT_DETAIL_HAS_BUILTIN(__builtin_subcll)
    unsigned long long carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long carry_out;
        const unsigned long long r
            = __builtin_addcll(static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), carry, &carry_out);
        dst[i] = static_cast<uint64_t>(r);
        carry = carry_out;
    }
    return;
#    endif
    add_limbs_copy_scalar<L>(dst, lhs, rhs);
}

template <>
GINT_FORCE_INLINE void add_limbs_copy_runtime<1>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    dst[0] = lhs[0] + rhs[0];
}

template <>
GINT_FORCE_INLINE void add_limbs_copy_runtime<2>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    add_limbs_copy_scalar<2>(dst, lhs, rhs);
}

template <>
GINT_FORCE_INLINE void add_limbs_copy_runtime<4>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
#    if GINT_DETAIL_X86_64_CLANG
    unsigned long long r0;
    unsigned long long r1;
    unsigned long long r2;
    unsigned long long r3;
    unsigned char c = _addcarry_u64(0, static_cast<unsigned long long>(lhs[0]), static_cast<unsigned long long>(rhs[0]), &r0);
    c = _addcarry_u64(c, static_cast<unsigned long long>(lhs[1]), static_cast<unsigned long long>(rhs[1]), &r1);
    c = _addcarry_u64(c, static_cast<unsigned long long>(lhs[2]), static_cast<unsigned long long>(rhs[2]), &r2);
    _addcarry_u64(c, static_cast<unsigned long long>(lhs[3]), static_cast<unsigned long long>(rhs[3]), &r3);
    dst[0] = static_cast<uint64_t>(r0);
    dst[1] = static_cast<uint64_t>(r1);
    dst[2] = static_cast<uint64_t>(r2);
    dst[3] = static_cast<uint64_t>(r3);
    return;
#    elif GINT_ARCH_AARCH64 && GINT_ENABLE_AARCH64_LIMB_ASM
    asm volatile("ldp x8, x9, [%[lhs]]\n\t"
                 "ldp x10, x11, [%[lhs], #16]\n\t"
                 "ldp x12, x13, [%[rhs]]\n\t"
                 "ldp x14, x15, [%[rhs], #16]\n\t"
                 "adds x8, x8, x12\n\t"
                 "adcs x9, x9, x13\n\t"
                 "adcs x10, x10, x14\n\t"
                 "adc  x11, x11, x15\n\t"
                 "stp x8, x9, [%[dst]]\n\t"
                 "stp x10, x11, [%[dst], #16]"
                 :
                 : [dst] "r"(dst), [lhs] "r"(lhs), [rhs] "r"(rhs)
                 : "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "cc", "memory");
    return;
#    endif
    using u128 = unsigned __int128;
    const u128 lo_a = (u128(lhs[1]) << 64) | lhs[0];
    const u128 lo_b = (u128(rhs[1]) << 64) | rhs[0];
    const u128 hi_a = (u128(lhs[3]) << 64) | lhs[2];
    const u128 hi_b = (u128(rhs[3]) << 64) | rhs[2];

    const u128 lo_sum = lo_a + lo_b;
    const u128 carry = lo_sum < lo_a;
    const u128 hi_sum = hi_a + hi_b + carry;

    dst[0] = static_cast<uint64_t>(lo_sum);
    dst[1] = static_cast<uint64_t>(lo_sum >> 64);
    dst[2] = static_cast<uint64_t>(hi_sum);
    dst[3] = static_cast<uint64_t>(hi_sum >> 64);
}

template <size_t L>
GINT_CONSTEXPR14 inline void add_limbs_copy(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        add_limbs_copy_scalar<L>(dst, lhs, rhs);
        return;
    }
    add_limbs_copy_runtime<L>(dst, lhs, rhs);
#    elif __cplusplus >= 201402L
    add_limbs_copy_scalar<L>(dst, lhs, rhs);
#    else
    add_limbs_copy_runtime<L>(dst, lhs, rhs);
#    endif
}

template <size_t L>
GINT_CONSTEXPR14 inline void add_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    add_limbs_copy<L>(lhs, lhs, rhs);
}

template <size_t L>
GINT_CONSTEXPR14 inline void sub_limbs_copy_scalar(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 lhs_i = lhs[i];
        unsigned __int128 subtrahend = static_cast<unsigned __int128>(rhs[i]) + borrow;
        dst[i] = static_cast<uint64_t>(lhs_i - subtrahend);
        borrow = lhs_i < subtrahend;
    }
}

template <size_t L>
GINT_FORCE_INLINE void sub_limbs_copy_runtime(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
#    if GINT_DETAIL_X86_64_GCC || GINT_DETAIL_X86_64_CLANG
    unsigned char borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        borrow = _subborrow_u64(borrow, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        dst[i] = static_cast<uint64_t>(r);
    }
    return;
#    elif GINT_DETAIL_AARCH64_CLANG && GINT_DETAIL_HAS_BUILTIN(__builtin_addcll) && GINT_DETAIL_HAS_BUILTIN(__builtin_subcll)
    unsigned long long borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long borrow_out;
        const unsigned long long r
            = __builtin_subcll(static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), borrow, &borrow_out);
        dst[i] = static_cast<uint64_t>(r);
        borrow = borrow_out;
    }
    return;
#    endif
    sub_limbs_copy_scalar<L>(dst, lhs, rhs);
}

template <>
GINT_FORCE_INLINE void sub_limbs_copy_runtime<2>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    sub_limbs_copy_scalar<2>(dst, lhs, rhs);
}

template <>
GINT_FORCE_INLINE void sub_limbs_copy_runtime<4>(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
#    if GINT_DETAIL_X86_64_GCC || GINT_DETAIL_X86_64_CLANG
    unsigned long long r0;
    unsigned long long r1;
    unsigned long long r2;
    unsigned long long r3;
    unsigned char b = _subborrow_u64(0, static_cast<unsigned long long>(lhs[0]), static_cast<unsigned long long>(rhs[0]), &r0);
    b = _subborrow_u64(b, static_cast<unsigned long long>(lhs[1]), static_cast<unsigned long long>(rhs[1]), &r1);
    b = _subborrow_u64(b, static_cast<unsigned long long>(lhs[2]), static_cast<unsigned long long>(rhs[2]), &r2);
    _subborrow_u64(b, static_cast<unsigned long long>(lhs[3]), static_cast<unsigned long long>(rhs[3]), &r3);
    dst[0] = static_cast<uint64_t>(r0);
    dst[1] = static_cast<uint64_t>(r1);
    dst[2] = static_cast<uint64_t>(r2);
    dst[3] = static_cast<uint64_t>(r3);
    return;
#    elif GINT_ARCH_AARCH64 && GINT_ENABLE_AARCH64_LIMB_ASM
    asm volatile("ldp x8, x9, [%[lhs]]\n\t"
                 "ldp x10, x11, [%[lhs], #16]\n\t"
                 "ldp x12, x13, [%[rhs]]\n\t"
                 "ldp x14, x15, [%[rhs], #16]\n\t"
                 "subs x8, x8, x12\n\t"
                 "sbcs x9, x9, x13\n\t"
                 "sbcs x10, x10, x14\n\t"
                 "sbc  x11, x11, x15\n\t"
                 "stp x8, x9, [%[dst]]\n\t"
                 "stp x10, x11, [%[dst], #16]"
                 :
                 : [dst] "r"(dst), [lhs] "r"(lhs), [rhs] "r"(rhs)
                 : "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "cc", "memory");
    return;
#    endif
    using u128 = unsigned __int128;
    const u128 lo_a = (u128(lhs[1]) << 64) | lhs[0];
    const u128 lo_b = (u128(rhs[1]) << 64) | rhs[0];
    const u128 hi_a = (u128(lhs[3]) << 64) | lhs[2];
    const u128 hi_b = (u128(rhs[3]) << 64) | rhs[2];

    const u128 lo_diff = lo_a - lo_b;
    const u128 borrow = lo_a < lo_b;
    const u128 hi_diff = hi_a - hi_b - borrow;

    dst[0] = static_cast<uint64_t>(lo_diff);
    dst[1] = static_cast<uint64_t>(lo_diff >> 64);
    dst[2] = static_cast<uint64_t>(hi_diff);
    dst[3] = static_cast<uint64_t>(hi_diff >> 64);
}

template <size_t L>
GINT_CONSTEXPR14 inline void sub_limbs_copy(uint64_t * dst, const uint64_t * lhs, const uint64_t * rhs) noexcept
{
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        sub_limbs_copy_scalar<L>(dst, lhs, rhs);
        return;
    }
    sub_limbs_copy_runtime<L>(dst, lhs, rhs);
#    elif __cplusplus >= 201402L
    sub_limbs_copy_scalar<L>(dst, lhs, rhs);
#    else
    sub_limbs_copy_runtime<L>(dst, lhs, rhs);
#    endif
}

template <size_t L>
GINT_CONSTEXPR14 inline void sub_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    sub_limbs_copy<L>(lhs, lhs, rhs);
}

template <size_t L>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_and_limbs(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    for (size_t i = 0; i < L; ++i)
        dst[i] = lhs[i] & rhs[i];
}

template <>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_and_limbs<2>(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    dst[0] = lhs[0] & rhs[0];
    dst[1] = lhs[1] & rhs[1];
}

template <>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_and_limbs<4>(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    dst[0] = lhs[0] & rhs[0];
    dst[1] = lhs[1] & rhs[1];
    dst[2] = lhs[2] & rhs[2];
    dst[3] = lhs[3] & rhs[3];
}

template <size_t L>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_or_limbs(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    for (size_t i = 0; i < L; ++i)
        dst[i] = lhs[i] | rhs[i];
}

template <>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_or_limbs<4>(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    dst[0] = lhs[0] | rhs[0];
    dst[1] = lhs[1] | rhs[1];
    dst[2] = lhs[2] | rhs[2];
    dst[3] = lhs[3] | rhs[3];
}

template <size_t L>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_xor_limbs(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    for (size_t i = 0; i < L; ++i)
        dst[i] = lhs[i] ^ rhs[i];
}

template <>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_xor_limbs<2>(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
#    if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        dst[0] = lhs[0] ^ rhs[0];
        dst[1] = lhs[1] ^ rhs[1];
        return;
    }
#    endif
#    if GINT_ARCH_X86_64 && defined(__SSE2__) && GINT_DETAIL_X86_64_CONSTEXPR_INTRINSICS_SAFE
    const __m128i l = _mm_loadu_si128(reinterpret_cast<const __m128i *>(lhs));
    const __m128i r = _mm_loadu_si128(reinterpret_cast<const __m128i *>(rhs));
    _mm_storeu_si128(reinterpret_cast<__m128i *>(dst), _mm_xor_si128(l, r));
#    else
    dst[0] = lhs[0] ^ rhs[0];
    dst[1] = lhs[1] ^ rhs[1];
#    endif
}

template <>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_xor_limbs<4>(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    dst[0] = lhs[0] ^ rhs[0];
    dst[1] = lhs[1] ^ rhs[1];
    dst[2] = lhs[2] ^ rhs[2];
    dst[3] = lhs[3] ^ rhs[3];
}

#    if GINT_DETAIL_AARCH64_CLANG
template <>
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_xor_limbs<16>(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    dst[0] = lhs[0] ^ rhs[0];
    dst[1] = lhs[1] ^ rhs[1];
    dst[2] = lhs[2] ^ rhs[2];
    dst[3] = lhs[3] ^ rhs[3];
    dst[4] = lhs[4] ^ rhs[4];
    dst[5] = lhs[5] ^ rhs[5];
    dst[6] = lhs[6] ^ rhs[6];
    dst[7] = lhs[7] ^ rhs[7];
    dst[8] = lhs[8] ^ rhs[8];
    dst[9] = lhs[9] ^ rhs[9];
    dst[10] = lhs[10] ^ rhs[10];
    dst[11] = lhs[11] ^ rhs[11];
    dst[12] = lhs[12] ^ rhs[12];
    dst[13] = lhs[13] ^ rhs[13];
    dst[14] = lhs[14] ^ rhs[14];
    dst[15] = lhs[15] ^ rhs[15];
}
#    endif

GINT_FORCE_INLINE void mul_limbs4_by_limb(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, uint64_t rhs) noexcept
{
    using u128 = unsigned __int128;
#    if GINT_GCC_TUNED_PATHS && !GINT_ARCH_X86_64
    u128 cur = u128(lhs[0]) * rhs;
    res[0] = static_cast<uint64_t>(cur);
    cur = u128(lhs[1]) * rhs + (cur >> 64);
    res[1] = static_cast<uint64_t>(cur);
    cur = u128(lhs[2]) * rhs + (cur >> 64);
    res[2] = static_cast<uint64_t>(cur);
    cur = u128(lhs[3]) * rhs + (cur >> 64);
    res[3] = static_cast<uint64_t>(cur);
#    else
    u128 carry = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        u128 cur = u128(lhs[i]) * rhs + carry;
        res[i] = static_cast<uint64_t>(cur);
        carry = cur >> 64;
    }
#    endif
}

GINT_FORCE_INLINE void
mul_limbs4_by_2limb(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, uint64_t rhs0, uint64_t rhs1) noexcept
{
    using u128 = unsigned __int128;

    const u128 p00 = u128(lhs[0]) * rhs0;
    res[0] = static_cast<uint64_t>(p00);
    u128 carry = p00 >> 64;

    {
        const u128 p10 = u128(lhs[1]) * rhs0;
        const u128 p01 = u128(lhs[0]) * rhs1;
        uint64_t c = 0;
        uint64_t lo = addc64(static_cast<uint64_t>(carry), static_cast<uint64_t>(p10), c);
        lo = addc64(lo, static_cast<uint64_t>(p01), c);
        res[1] = lo;
        carry = (carry >> 64) + (p10 >> 64) + (p01 >> 64) + c;
    }

    {
        const u128 p20 = u128(lhs[2]) * rhs0;
        const u128 p11 = u128(lhs[1]) * rhs1;
        uint64_t c = 0;
        uint64_t lo = addc64(static_cast<uint64_t>(carry), static_cast<uint64_t>(p20), c);
        lo = addc64(lo, static_cast<uint64_t>(p11), c);
        res[2] = lo;
        carry = (carry >> 64) + (p20 >> 64) + (p11 >> 64) + c;
    }

    {
        const u128 p30 = u128(lhs[3]) * rhs0;
        const u128 p21 = u128(lhs[2]) * rhs1;
        uint64_t c = 0;
        uint64_t lo = addc64(static_cast<uint64_t>(carry), static_cast<uint64_t>(p30), c);
        lo = addc64(lo, static_cast<uint64_t>(p21), c);
        res[3] = lo;
    }
}

GINT_CONSTEXPR14 GINT_FORCE_INLINE void
sub_limbs4_by_limb(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, uint64_t rhs) noexcept
{
    const uint64_t r0 = lhs[0] - rhs;
    uint64_t borrow = lhs[0] < rhs;
    const uint64_t r1 = lhs[1] - borrow;
    borrow = borrow && lhs[1] == 0;
    const uint64_t r2 = lhs[2] - borrow;
    borrow = borrow && lhs[2] == 0;
    const uint64_t r3 = lhs[3] - borrow;
    dst[0] = r0;
    dst[1] = r1;
    dst[2] = r2;
    dst[3] = r3;
}

GINT_FORCE_INLINE
bool mul_limbs4_try_small_operand(
    uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
#    if GINT_GCC_TUNED_PATHS && !GINT_ARCH_X86_64
    using u128 = unsigned __int128;
    if ((rhs[1] | rhs[2] | rhs[3]) == 0)
    {
        if ((lhs[1] | lhs[2] | lhs[3]) == 0)
        {
            u128 p = u128(lhs[0]) * rhs[0];
            res[0] = static_cast<uint64_t>(p);
            res[1] = static_cast<uint64_t>(p >> 64);
            res[2] = 0;
            res[3] = 0;
        }
        else
        {
            mul_limbs4_by_limb(res, lhs, rhs[0]);
        }
        return true;
    }
    if ((lhs[1] | lhs[2] | lhs[3]) == 0)
    {
        mul_limbs4_by_limb(res, rhs, lhs[0]);
        return true;
    }
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
            high += (p01 >> 64);
            high += (p10 >> 64);
            high += carry;

            res[2] = static_cast<uint64_t>(high);
            res[3] = static_cast<uint64_t>(high >> 64);
        }
        return true;
    }
#    else
    (void)res;
    (void)lhs;
    (void)rhs;
#    endif
    return false;
}

#    if GINT_DETAIL_X86_64_GCC
inline GINT_NOINLINE GINT_COLD void mul_limbs4_u64(uint64_t * GINT_RESTRICT res, uint64_t lhs, uint64_t rhs) noexcept
{
    const unsigned __int128 p = static_cast<unsigned __int128>(lhs) * rhs;
    res[0] = static_cast<uint64_t>(p);
    res[1] = static_cast<uint64_t>(p >> 64);
    res[2] = 0;
    res[3] = 0;
}
#    endif

// Perform fixed-width multiplication using the generic schoolbook method.
// Only the low L limbs are retained.
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

// Fast path for 128-bit (2-limb) multiplication that directly computes the
// low 128 bits required by fixed-width semantics.
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

GINT_FORCE_INLINE void
mul_limbs4_general(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    using u128 = unsigned __int128;
#    if GINT_ARCH_X86_64
    const u128 a01 = (u128(lhs[1]) << 64) | lhs[0];
    const u128 a23 = (u128(lhs[3]) << 64) | lhs[2];
    const u128 a0 = lhs[0];
    const u128 a1 = lhs[1];

    const u128 b01 = (u128(rhs[1]) << 64) | rhs[0];
    const u128 b23 = (u128(rhs[3]) << 64) | rhs[2];
    const uint64_t b0 = rhs[0];
    const uint64_t b1 = rhs[1];

    u128 r23 = a23 * b01 + a01 * b23 + a1 * b1;
    const u128 r01 = a0 * b0;
    u128 r12 = (r01 >> 64) + (r23 << 64);
    u128 r12_x = a1 * b0;

    res[0] = static_cast<uint64_t>(r01);
    res[3] = static_cast<uint64_t>(r23 >> 64);

    const u128 r12_y = a0 * b1;
    r12_x += r12_y;
    if (r12_x < r12_y)
        ++res[3];

    r12 += r12_x;
    if (r12 < r12_x)
        ++res[3];

    res[1] = static_cast<uint64_t>(r12);
    res[2] = static_cast<uint64_t>(r12 >> 64);
#    else
    const uint64_t a0 = lhs[0], a1 = lhs[1], a2 = lhs[2], a3 = lhs[3];
    const uint64_t b0 = rhs[0], b1 = rhs[1], b2 = rhs[2], b3 = rhs[3];

    u128 carry = 0;

    {
        u128 p00 = u128(a0) * b0;
        u128 lo_acc = u128(static_cast<uint64_t>(carry)) + static_cast<uint64_t>(p00);
        uint64_t lo = static_cast<uint64_t>(lo_acc);
        u128 hi_acc = (carry >> 64) + (p00 >> 64) + (lo_acc >> 64);
        res[0] = lo;
        carry = hi_acc;
    }

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
#    endif
}

template <>
GINT_FORCE_INLINE void
mul_limbs<4>(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
#    if GINT_DETAIL_X86_64_GCC
    if (GINT_LIKELY((lhs[3] | rhs[3]) == 0 && (lhs[2] | rhs[2] | lhs[1] | rhs[1]) == 0))
    {
        mul_limbs4_u64(res, lhs[0], rhs[0]);
        return;
    }
#    endif
#    if GINT_GCC_TUNED_PATHS && !GINT_ARCH_X86_64
    const bool lhs_above_128 = (lhs[2] | lhs[3]) != 0;
    const bool rhs_above_128 = (rhs[2] | rhs[3]) != 0;
    if (GINT_LIKELY(lhs_above_128 && rhs_above_128))
    {
        mul_limbs4_general(res, lhs, rhs);
        return;
    }
    if (mul_limbs4_try_small_operand(res, lhs, rhs))
        return;
#    endif
    mul_limbs4_general(res, lhs, rhs);
}

template <size_t L>
GINT_NOINLINE bool
mul_try_single_limb_operand(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept;

template <size_t L>
GINT_FORCE_INLINE void
mul_limbs_schoolbook_result(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    for (size_t i = 0; i < L; ++i)
        res[i] = 0;
    mul_limbs<L>(res, lhs, rhs);
}

template <size_t L>
GINT_FORCE_INLINE void
mul_limbs_result(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
#    if !GINT_DETAIL_X86_64_GCC
    if (L > 4 && GINT_UNLIKELY(lhs[L - 1] == 0 || rhs[L - 1] == 0) && mul_try_single_limb_operand<L>(res, lhs, rhs))
        return;
#    endif
    mul_limbs_schoolbook_result<L>(res, lhs, rhs);
}

template <>
GINT_FORCE_INLINE void
mul_limbs_result<2>(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    mul_limbs<2>(res, lhs, rhs);
}

template <>
GINT_FORCE_INLINE void
mul_limbs_result<4>(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    mul_limbs<4>(res, lhs, rhs);
}

template <size_t L>
GINT_FORCE_INLINE bool limbs_zero_above(const uint64_t * data, size_t first) noexcept
{
    if (data[L - 1] != 0)
        return false;
    for (size_t i = first; i < L; ++i)
    {
        if (data[i] != 0)
            return false;
    }
    return true;
}

template <size_t L>
GINT_FORCE_INLINE void mul_single_limb_product(uint64_t * res, uint64_t lhs, uint64_t rhs) noexcept
{
    const unsigned __int128 product = static_cast<unsigned __int128>(lhs) * rhs;
    res[0] = static_cast<uint64_t>(product);
    if (L > 1)
        res[1] = static_cast<uint64_t>(product >> 64);
    for (size_t i = 2; i < L; ++i)
        res[i] = 0;
}

template <size_t L>
GINT_FORCE_INLINE void mul_limbs_by_limb_result(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, uint64_t rhs) noexcept
{
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 cur = static_cast<unsigned __int128>(lhs[i]) * rhs + carry;
        res[i] = static_cast<uint64_t>(cur);
        carry = cur >> 64;
    }
}

template <size_t L>
GINT_NOINLINE bool
mul_try_single_limb_operand(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    if (GINT_UNLIKELY((lhs[L - 1] | rhs[L - 1]) == 0))
    {
        uint64_t high_or = 0;
        for (size_t i = 1; i < L; ++i)
            high_or |= lhs[i] | rhs[i];
        if (high_or == 0)
        {
            mul_single_limb_product<L>(res, lhs[0], rhs[0]);
            return true;
        }
    }
    if (limbs_zero_above<L>(rhs, 1))
    {
        if (limbs_zero_above<L>(lhs, 1))
            mul_single_limb_product<L>(res, lhs[0], rhs[0]);
        else
            mul_limbs_by_limb_result<L>(res, lhs, rhs[0]);
        return true;
    }
    if (limbs_zero_above<L>(lhs, 1))
    {
        mul_limbs_by_limb_result<L>(res, rhs, lhs[0]);
        return true;
    }
    return false;
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
GINT_FORCE_INLINE void mul_limb<4>(uint64_t * lhs, uint64_t rhs) noexcept
{
    using u128 = unsigned __int128;
    u128 cur = u128(lhs[0]) * rhs;
    lhs[0] = static_cast<uint64_t>(cur);
    cur = u128(lhs[1]) * rhs + (cur >> 64);
    lhs[1] = static_cast<uint64_t>(cur);
    cur = u128(lhs[2]) * rhs + (cur >> 64);
    lhs[2] = static_cast<uint64_t>(cur);
    cur = u128(lhs[3]) * rhs + (cur >> 64);
    lhs[3] = static_cast<uint64_t>(cur);
}

} // namespace detail

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint

#endif // GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
