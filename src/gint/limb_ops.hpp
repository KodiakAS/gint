#pragma once

#include "configuration.hpp"

// Fixed-width arithmetic on native words and limb arrays, including target fast paths.
// No integer objects, private-storage access, or signed-object policies belong here.

namespace gint
{
inline namespace GINT_DETAIL_CONFIG_NAMESPACE
{

namespace detail
{

// Array references preserve C++11 constant evaluation on GCC 4.8.
template <size_t I>
struct limbs_equal
{
    template <size_t L>
    static constexpr bool eval(const uint64_t (&lhs)[L], const uint64_t (&rhs)[L]) noexcept
    {
        return lhs[I] == rhs[I] && limbs_equal<I - 1>::eval(lhs, rhs);
    }
};

template <>
struct limbs_equal<0>
{
    template <size_t L>
    static constexpr bool eval(const uint64_t (&lhs)[L], const uint64_t (&rhs)[L]) noexcept
    {
        return lhs[0] == rhs[0];
    }
};

//=== Equality on raw limbs ===================================================
GINT_FORCE_INLINE bool limbs_equal_runtime_1024(const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    if (lhs[15] != rhs[15])
        return false;

    uint64_t difference = 0;
    for (size_t i = 0; i < 15; ++i)
        difference |= lhs[i] ^ rhs[i];
    return difference == 0;
}

template <size_t Limbs>
constexpr bool equal_limbs(const uint64_t (&lhs)[Limbs], const uint64_t (&rhs)[Limbs]) noexcept
{
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L && !GINT_GCC_TUNED_PATHS
    if (!__builtin_is_constant_evaluated() && Limbs == 16)
        return limbs_equal_runtime_1024(lhs, rhs);
#endif
    return limbs_equal<Limbs - 1>::eval(lhs, rhs);
}

//=== Native-word arithmetic ==================================================
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

//=== Limb addition and subtraction ===========================================
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
#if GINT_DETAIL_X86_64_CARRY_INTRINSICS
    unsigned char carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        carry = _addcarry_u64(carry, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        dst[i] = static_cast<uint64_t>(r);
    }
    return;
#elif GINT_DETAIL_AARCH64_CLANG && GINT_DETAIL_HAS_BUILTIN(__builtin_addcll) && GINT_DETAIL_HAS_BUILTIN(__builtin_subcll)
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
#endif
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
#if GINT_DETAIL_X86_64_CLANG && GINT_DETAIL_X86_64_CARRY_INTRINSICS
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
#elif GINT_ARCH_AARCH64 && GINT_ENABLE_AARCH64_LIMB_ASM
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
#endif
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
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        add_limbs_copy_scalar<L>(dst, lhs, rhs);
        return;
    }
    add_limbs_copy_runtime<L>(dst, lhs, rhs);
#elif __cplusplus >= 201402L
    add_limbs_copy_scalar<L>(dst, lhs, rhs);
#else
    add_limbs_copy_runtime<L>(dst, lhs, rhs);
#endif
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
#if GINT_DETAIL_X86_64_CARRY_INTRINSICS
    unsigned char borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        borrow = _subborrow_u64(borrow, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        dst[i] = static_cast<uint64_t>(r);
    }
    return;
#elif GINT_DETAIL_AARCH64_CLANG && GINT_DETAIL_HAS_BUILTIN(__builtin_addcll) && GINT_DETAIL_HAS_BUILTIN(__builtin_subcll)
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
#endif
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
#if GINT_DETAIL_X86_64_CARRY_INTRINSICS
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
#elif GINT_ARCH_AARCH64 && GINT_ENABLE_AARCH64_LIMB_ASM
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
#endif
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
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        sub_limbs_copy_scalar<L>(dst, lhs, rhs);
        return;
    }
    sub_limbs_copy_runtime<L>(dst, lhs, rhs);
#elif __cplusplus >= 201402L
    sub_limbs_copy_scalar<L>(dst, lhs, rhs);
#else
    sub_limbs_copy_runtime<L>(dst, lhs, rhs);
#endif
}

template <size_t L>
GINT_CONSTEXPR14 inline void sub_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    sub_limbs_copy<L>(lhs, lhs, rhs);
}

//=== Limb bitwise operations =================================================
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
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        dst[0] = lhs[0] ^ rhs[0];
        dst[1] = lhs[1] ^ rhs[1];
        return;
    }
#endif
#if GINT_ARCH_X86_64 && defined(__SSE2__) && GINT_DETAIL_X86_64_CONSTEXPR_INTRINSICS_SAFE
    const __m128i l = _mm_loadu_si128(reinterpret_cast<const __m128i *>(lhs));
    const __m128i r = _mm_loadu_si128(reinterpret_cast<const __m128i *>(rhs));
    _mm_storeu_si128(reinterpret_cast<__m128i *>(dst), _mm_xor_si128(l, r));
#else
    dst[0] = lhs[0] ^ rhs[0];
    dst[1] = lhs[1] ^ rhs[1];
#endif
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

#if GINT_DETAIL_AARCH64_CLANG
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
#endif

//=== Small-operand helpers and limb multiplication ===========================
GINT_FORCE_INLINE void mul_limbs4_by_limb(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, uint64_t rhs) noexcept
{
    using u128 = unsigned __int128;
#if GINT_GCC_TUNED_PATHS && !GINT_ARCH_X86_64
    u128 cur = u128(lhs[0]) * rhs;
    res[0] = static_cast<uint64_t>(cur);
    cur = u128(lhs[1]) * rhs + (cur >> 64);
    res[1] = static_cast<uint64_t>(cur);
    cur = u128(lhs[2]) * rhs + (cur >> 64);
    res[2] = static_cast<uint64_t>(cur);
    cur = u128(lhs[3]) * rhs + (cur >> 64);
    res[3] = static_cast<uint64_t>(cur);
#else
    u128 carry = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        u128 cur = u128(lhs[i]) * rhs + carry;
        res[i] = static_cast<uint64_t>(cur);
        carry = cur >> 64;
    }
#endif
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
#if GINT_GCC_TUNED_PATHS && !GINT_ARCH_X86_64
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
#else
    (void)res;
    (void)lhs;
    (void)rhs;
#endif
    return false;
}

#if GINT_DETAIL_X86_64_GCC
inline GINT_NOINLINE GINT_COLD void mul_limbs4_u64(uint64_t * GINT_RESTRICT res, uint64_t lhs, uint64_t rhs) noexcept
{
    const unsigned __int128 p = static_cast<unsigned __int128>(lhs) * rhs;
    res[0] = static_cast<uint64_t>(p);
    res[1] = static_cast<uint64_t>(p >> 64);
    res[2] = 0;
    res[3] = 0;
}
#endif

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
#if GINT_ARCH_X86_64
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
#else
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
#endif
}

template <>
GINT_FORCE_INLINE void
mul_limbs<4>(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
#if GINT_DETAIL_X86_64_GCC
    if (GINT_LIKELY((lhs[3] | rhs[3]) == 0 && (lhs[2] | rhs[2] | lhs[1] | rhs[1]) == 0))
    {
        mul_limbs4_u64(res, lhs[0], rhs[0]);
        return;
    }
#endif
#if GINT_GCC_TUNED_PATHS && !GINT_ARCH_X86_64
    const bool lhs_above_128 = (lhs[2] | lhs[3]) != 0;
    const bool rhs_above_128 = (rhs[2] | rhs[3]) != 0;
    if (GINT_LIKELY(lhs_above_128 && rhs_above_128))
    {
        mul_limbs4_general(res, lhs, rhs);
        return;
    }
    if (mul_limbs4_try_small_operand(res, lhs, rhs))
        return;
#endif
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
#if !GINT_DETAIL_X86_64_GCC
    if (L > 4 && GINT_UNLIKELY(lhs[L - 1] == 0 || rhs[L - 1] == 0) && mul_try_single_limb_operand<L>(res, lhs, rhs))
        return;
#endif
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

//=== Elementary limb operations ===========================================
template <size_t limbs>
GINT_CONSTEXPR14 inline void bit_and_assign(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
        lhs[i] &= rhs[i];
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void bit_or_assign(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
        lhs[i] |= rhs[i];
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void bit_xor_assign(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
        lhs[i] ^= rhs[i];
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void increment_limbs(uint64_t * data) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
    {
        if (++data[i])
            break;
    }
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void decrement_limbs(uint64_t * data) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
    {
        uint64_t old = data[i];
        --data[i];
        if (old)
            break;
    }
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void add_limb(uint64_t * lhs, uint64_t rhs) noexcept
{
    const uint64_t old = lhs[0];
    lhs[0] += rhs;
    uint64_t carry = lhs[0] < old;
    for (size_t i = 1; i < limbs && carry; ++i)
        carry = ++lhs[i] == 0;
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void sub_limb(uint64_t * lhs, uint64_t rhs) noexcept
{
    const uint64_t old = lhs[0];
    lhs[0] -= rhs;
    uint64_t borrow = old < rhs;
    for (size_t i = 1; i < limbs && borrow; ++i)
    {
        const uint64_t current = lhs[i];
        --lhs[i];
        borrow = current == 0;
    }
}

template <size_t limbs>
GINT_CONSTEXPR14 inline bool less_limbs(const uint64_t * lhs, const uint64_t * rhs) noexcept
{
    for (size_t i = limbs; i-- > 0;)
    {
        if (lhs[i] != rhs[i])
            return lhs[i] < rhs[i];
    }
    return false;
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void complement_limbs(uint64_t * value) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
        value[i] = ~value[i];
}

template <size_t limbs>
GINT_CONSTEXPR14 inline void negate_limbs_copy(uint64_t * GINT_RESTRICT res, const uint64_t * value) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
        res[i] = ~value[i];
    uint64_t carry = 1;
    for (size_t i = 0; i < limbs; ++i)
    {
        unsigned __int128 sum = static_cast<unsigned __int128>(res[i]) + carry;
        res[i] = static_cast<uint64_t>(sum);
        carry = sum >> 64;
        if (!carry)
            break;
    }
}

template <size_t limbs>
GINT_FORCE_INLINE void mul_add_limb(uint64_t * data, uint64_t multiplier, uint64_t addend) noexcept
{
    unsigned __int128 carry = addend;
    for (size_t i = 0; i < limbs; ++i)
    {
        const unsigned __int128 product = static_cast<unsigned __int128>(data[i]) * multiplier + carry;
        data[i] = static_cast<uint64_t>(product);
        carry = product >> 64;
    }
}

template <size_t limbs>
GINT_CONSTEXPR14 inline bool is_zero_limbs(const uint64_t * data) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
        if (data[i] != 0)
            return false;
    return true;
}

template <size_t limbs>
inline int highest_bit_limbs(const uint64_t * data) noexcept
{
    for (int i = static_cast<int>(limbs) - 1; i >= 0; --i)
    {
        if (data[i])
            return i * 64 + 63 - __builtin_clzll(data[i]);
    }
    return -1;
}

template <size_t limbs>
inline size_t used_limbs(const uint64_t * value) noexcept
{
    size_t n = limbs;
    while (n > 0 && value[n - 1] == 0)
        --n;
    return n;
}

template <size_t limbs>
GINT_FORCE_INLINE void negate_limbs(uint64_t * value) noexcept
{
    uint64_t carry = 1;
    for (size_t i = 0; i < limbs; ++i)
    {
        const uint64_t inv = ~value[i];
        const uint64_t sum = inv + carry;
        value[i] = sum;
        carry = carry && (sum == 0);
    }
}

template <size_t limbs>
inline bool power_of_two_limbs(const uint64_t * value, int & bit_index) noexcept
{
    bit_index = -1;
    bool found = false;
    for (size_t i = 0; i < limbs; ++i)
    {
        uint64_t limb = value[i];
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

template <size_t limbs>
GINT_CONSTEXPR14 inline void fill_limbs(uint64_t * data, uint64_t fill) noexcept
{
    for (size_t i = 0; i < limbs; ++i)
        data[i] = fill;
}

//=== Native-word fast paths and magnitude copying ===========================
GINT_CONSTEXPR14 GINT_FORCE_INLINE void shift_right_arithmetic_128(uint64_t * GINT_RESTRICT result, const uint64_t * lhs, size_t n) noexcept
{
    using s128 = __int128;
    using u128 = unsigned __int128;
    const u128 raw = (static_cast<u128>(lhs[1]) << 64) | lhs[0];
    const s128 shifted = static_cast<s128>(raw) >> n;
    const u128 shifted_raw = static_cast<u128>(shifted);
    result[0] = static_cast<uint64_t>(shifted_raw);
    result[1] = static_cast<uint64_t>(shifted_raw >> 64);
}

GINT_CONSTEXPR14 GINT_FORCE_INLINE void shift_left_128(uint64_t * GINT_RESTRICT result, const uint64_t * lhs, unsigned n) noexcept
{
    using u128 = unsigned __int128;
    const u128 raw = (static_cast<u128>(lhs[1]) << 64) | lhs[0];
    const u128 shifted = raw << n;
    result[0] = static_cast<uint64_t>(shifted);
    result[1] = static_cast<uint64_t>(shifted >> 64);
}

GINT_FORCE_INLINE void
div_unsigned_int128_by_positive_limb(uint64_t * GINT_RESTRICT result, const uint64_t * lhs, uint64_t divisor) noexcept
{
    using u128 = unsigned __int128;
    const u128 lhs_raw = (static_cast<u128>(lhs[1]) << 64) | lhs[0];
    const u128 quotient = lhs_raw / divisor;
    result[0] = static_cast<uint64_t>(quotient);
    result[1] = static_cast<uint64_t>(quotient >> 64);
}

GINT_FORCE_INLINE void div_signed_int128_by_positive_limb(uint64_t * GINT_RESTRICT result, const uint64_t * lhs, uint64_t divisor) noexcept
{
    using u128 = unsigned __int128;
    using s128 = __int128;
    const u128 lhs_raw = (static_cast<u128>(lhs[1]) << 64) | lhs[0];
    const s128 quotient = static_cast<s128>(lhs_raw) / static_cast<s128>(divisor);
    const u128 quotient_raw = static_cast<u128>(quotient);
    result[0] = static_cast<uint64_t>(quotient_raw);
    result[1] = static_cast<uint64_t>(quotient_raw >> 64);
}

template <size_t limbs>
inline void copy_magnitude_limbs(uint64_t * dst, const uint64_t * src, bool neg) noexcept
{
    if (!neg)
    {
        for (size_t i = 0; i < limbs; ++i)
            dst[i] = src[i];
        return;
    }

    uint64_t carry = 1;
    for (size_t i = 0; i < limbs; ++i)
    {
        const uint64_t inv = ~src[i];
        const uint64_t sum = inv + carry;
        dst[i] = sum;
        carry = carry && (sum == 0);
    }
}

//=== Limb shifts ============================================================
// In-place shifts require a nonzero total shift within the array width.
// Decoded right-shift offsets require limb_shift < limbs and bit_shift < 64.
// Out-of-place outputs must not overlap inputs. The integer layer chooses zero or sign-extension fill.
template <size_t Limbs>
struct limb_shift
{
    using limb_type = uint64_t;
    static constexpr size_t limbs = Limbs;

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE void shift_left_assign(limb_type (&data)[limbs], size_t shift) noexcept
    {
        size_t limb_shift = shift / 64;
        int bit_shift = shift % 64;
        if (limbs == 4)
        {
            const uint64_t src0 = static_cast<uint64_t>(data[0]);
            const uint64_t src1 = static_cast<uint64_t>(data[1]);
            const uint64_t src2 = static_cast<uint64_t>(data[2]);
            const uint64_t src3 = static_cast<uint64_t>(data[3]);
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
            data[0] = static_cast<limb_type>(out0);
            data[1] = static_cast<limb_type>(out1);
            data[2] = static_cast<limb_type>(out2);
            data[3] = static_cast<limb_type>(out3);
            return;
        }
        if (limbs < 4)
        {
            if (limb_shift)
            {
                for (size_t i = limbs; i-- > limb_shift;)
                    data[i] = data[i - limb_shift];
                for (size_t i = 0; i < limb_shift; ++i)
                    data[i] = 0;
            }
            if (bit_shift)
            {
                for (size_t i = limbs; i-- > 0;)
                {
                    unsigned __int128 part = static_cast<unsigned __int128>(data[i]) << bit_shift;
                    if (i)
                        part |= data[i - 1] >> (64 - bit_shift);
                    data[i] = static_cast<limb_type>(part);
                }
            }
            return;
        }
        return shift_left_assign_wide(data, limb_shift, static_cast<unsigned>(bit_shift));
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE void
    shift_right_assign(limb_type (&data)[limbs], size_t limb_shift, unsigned bit_shift, limb_type fill) noexcept
    {
        // Keep two-limb temporaries scalar across the object-to-array boundary.
        if (limbs == 2)
        {
            const limb_type low = limb_shift ? data[1] : data[0];
            const limb_type high = limb_shift ? fill : data[1];
            if (bit_shift)
            {
                data[0] = (low >> bit_shift) | (high << (64 - bit_shift));
                data[1] = (high >> bit_shift) | (fill << (64 - bit_shift));
            }
            else
            {
                data[0] = low;
                data[1] = high;
            }
            return;
        }
        if (limbs == 4)
        {
            const uint64_t src0 = static_cast<uint64_t>(data[0]);
            const uint64_t src1 = static_cast<uint64_t>(data[1]);
            const uint64_t src2 = static_cast<uint64_t>(data[2]);
            const uint64_t src3 = static_cast<uint64_t>(data[3]);
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
            data[0] = static_cast<limb_type>(out0);
            data[1] = static_cast<limb_type>(out1);
            data[2] = static_cast<limb_type>(out2);
            data[3] = static_cast<limb_type>(out3);
            return;
        }
        if (limbs < 4)
        {
            if (limb_shift)
            {
                for (size_t i = 0; i < limbs - limb_shift; ++i)
                    data[i] = data[i + limb_shift];
                for (size_t i = limbs - limb_shift; i < limbs; ++i)
                    data[i] = fill;
            }
            if (bit_shift)
            {
                const unsigned inv_shift = 64U - bit_shift;
                const limb_type top = data[limbs - 1];
                limb_type prev = top;
                for (size_t i = limbs - 1; i > 0; --i)
                {
                    limb_type cur = data[i - 1];
                    data[i - 1] = (cur >> bit_shift) | (prev << inv_shift);
                    prev = cur;
                }
                data[limbs - 1] = (top >> bit_shift) | (fill << inv_shift);
            }
            return;
        }
        return shift_right_assign_wide(data, limb_shift, bit_shift, fill);
    }

    static GINT_CONSTEXPR14 GINT_WIDE_SHIFT_INLINE void
    shift_left_assign_wide(limb_type (&data)[limbs], size_t limb_shift, unsigned bit_shift) noexcept
    {
        if (bit_shift)
        {
            const unsigned shift_bits = static_cast<unsigned>(bit_shift);
            const unsigned inv_shift = 64U - shift_bits;
            for (size_t i = limbs; i-- > 0;)
            {
                if (i < limb_shift)
                {
                    data[i] = 0;
                    continue;
                }

                const size_t src = i - limb_shift;
                limb_type part = data[src] << shift_bits;
                if (src)
                    part |= data[src - 1] >> inv_shift;
                data[i] = part;
            }
        }
        else if (limb_shift)
        {
            for (size_t i = limbs; i-- > limb_shift;)
                data[i] = data[i - limb_shift];
            for (size_t i = 0; i < limb_shift; ++i)
                data[i] = 0;
        }
        return;
    }

    static GINT_CONSTEXPR14 GINT_WIDE_SHIFT_INLINE void
    shift_right_assign_wide(limb_type (&data)[limbs], size_t limb_shift, unsigned bit_shift, limb_type fill) noexcept
    {
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            const size_t count = limbs - limb_shift;
            for (size_t i = 0; i < count; ++i)
            {
                const size_t src = i + limb_shift;
                limb_type part = data[src] >> bit_shift;
                if (src + 1 < limbs)
                    part |= data[src + 1] << inv_shift;
                else
                    part |= fill << inv_shift;
                data[i] = part;
            }
            for (size_t i = count; i < limbs; ++i)
                data[i] = fill;
        }
        else if (limb_shift)
        {
            const size_t count = limbs - limb_shift;
            for (size_t i = 0; i < count; ++i)
                data[i] = data[i + limb_shift];
            for (size_t i = count; i < limbs; ++i)
                data[i] = fill;
        }
        return;
    }

#if !GINT_GCC_TUNED_PATHS || GINT_DETAIL_AARCH64_GCC
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE void
    shift_left_into(limb_type * GINT_RESTRICT result, const limb_type * value, size_t shift) noexcept
    {
        const size_t limb_shift = shift / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            for (size_t i = 0; i < limb_shift; ++i)
                result[i] = 0;

            const size_t count = limbs - limb_shift;
            limb_type carry = 0;
            for (size_t i = 0; i < count; ++i)
            {
                const limb_type cur = value[i];
                result[i + limb_shift] = (cur << bit_shift) | carry;
                carry = cur >> inv_shift;
            }
        }
        else
        {
            for (size_t i = 0; i < limb_shift; ++i)
                result[i] = 0;
            for (size_t i = limb_shift; i < limbs; ++i)
                result[i] = value[i - limb_shift];
        }
        return;
    }

#endif

#if !GINT_GCC_TUNED_PATHS
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE void
    shift_right_into(limb_type * GINT_RESTRICT result, const limb_type * value, size_t shift, limb_type fill) noexcept
    {
        if (shift >= limbs * 64)
        {
            for (size_t i = 0; i < limbs; ++i)
                result[i] = fill;
            return;
        }

        const size_t limb_shift = shift / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        const size_t count = limbs - limb_shift;
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            result[0] = value[limb_shift] >> bit_shift;
            for (size_t i = 1; i < count; ++i)
            {
                const limb_type cur = value[limb_shift + i];
                result[i - 1] |= cur << inv_shift;
                result[i] = cur >> bit_shift;
            }
            result[count - 1] |= fill << inv_shift;
        }
        else
        {
            for (size_t i = 0; i < count; ++i)
                result[i] = value[i + limb_shift];
        }
        for (size_t i = count; i < limbs; ++i)
            result[i] = fill;
    }

#endif
};

//=== Floating-point magnitude kernels =======================================
// Conversion policies stay in integer; kernels receive raw magnitudes and the
// sign/fill selected by that layer. Rounding retains guard/sticky-bit logic.
template <size_t Limbs>
struct limb_float
{
    using limb_type = uint64_t;
    static constexpr size_t limbs = Limbs;

    static bool test_bit(const limb_type * value, int bit) noexcept
    {
        return bit >= 0 && bit < static_cast<int>(limbs * 64) && ((value[static_cast<size_t>(bit) / 64] >> (bit % 64)) & 1);
    }

    static bool has_any_bit_below(const limb_type * value, int bit) noexcept
    {
        if (bit <= 0)
            return false;
        size_t full_limbs = static_cast<size_t>(bit) / 64;
        const unsigned rem = static_cast<unsigned>(bit % 64);
        if (full_limbs > limbs)
            full_limbs = limbs;
        for (size_t i = 0; i < full_limbs; ++i)
            if (value[i])
                return true;
        if (full_limbs < limbs && rem != 0)
        {
            const limb_type mask = (limb_type(1) << rem) - 1;
            return (value[full_limbs] & mask) != 0;
        }
        return false;
    }

    static limb_type low_limb_after_logical_right_shift(const limb_type * value, unsigned shift) noexcept
    {
        const size_t limb_shift = static_cast<size_t>(shift) / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        if (limb_shift >= limbs)
            return 0;

        limb_type result = value[limb_shift] >> bit_shift;
        if (bit_shift != 0 && limb_shift + 1 < limbs)
            result |= value[limb_shift + 1] << (64 - bit_shift);
        return result;
    }

    static unsigned __int128 low_u128_after_logical_right_shift(const limb_type * value, unsigned shift) noexcept
    {
        using u128 = unsigned __int128;
        const size_t limb_shift = static_cast<size_t>(shift) / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        if (limb_shift >= limbs)
            return 0;

        u128 result = static_cast<u128>(value[limb_shift]) >> bit_shift;
        if (limb_shift + 1 < limbs)
            result |= static_cast<u128>(value[limb_shift + 1]) << (64 - bit_shift);
        if (bit_shift != 0 && limb_shift + 2 < limbs)
            result |= static_cast<u128>(value[limb_shift + 2]) << (128 - bit_shift);
        return result;
    }

    template <typename Float>
    static typename std::enable_if<(std::numeric_limits<Float>::digits < 64), limb_type>::type
    binary_float_significand(const limb_type * value, unsigned shift) noexcept
    {
        return low_limb_after_logical_right_shift(value, shift);
    }

    template <typename Float>
    static typename std::enable_if<(std::numeric_limits<Float>::digits >= 64), unsigned __int128>::type
    binary_float_significand(const limb_type * value, unsigned shift) noexcept
    {
        return low_u128_after_logical_right_shift(value, shift);
    }

    template <typename Float>
    static GINT_FORCE_INLINE Float to_binary_float(const limb_type * mag, bool neg) noexcept
    {
        const int hb = highest_bit_limbs<limbs>(mag);
        const int digits = std::numeric_limits<Float>::digits;
        if (hb < digits)
        {
            Float res = 0;
            for (size_t i = limbs; i-- > 0;)
            {
                res = std::ldexp(res, 64);
                res += static_cast<Float>(mag[i]);
            }
            return neg ? -res : res;
        }

        int scale = hb - (digits - 1);
        typedef typename std::conditional<(std::numeric_limits<Float>::digits < 64), limb_type, unsigned __int128>::type significand_type;
        significand_type significand = binary_float_significand<Float>(mag, static_cast<unsigned>(scale));
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

    // The caller selects high-bit extension at compile time, avoiding a live
    // fill argument across the significand comparison and its library calls.
    template <bool ExtendHighBit, typename T>
    static int compare_with_float_abs(const limb_type (&lhs_abs)[limbs], T rhs_abs) noexcept
    {
        // Both are non-negative.
        if (is_zero_limbs<limbs>(lhs_abs))
            return rhs_abs == T(0) ? 0 : -1;
        int e = 0;
        T m = std::frexp(rhs_abs, &e); // rhs_abs = m * 2^e, 0.5<=m<1
        if (m == T(0))
            return 1; // lhs_abs > 0 > rhs_abs
        int hb = highest_bit_limbs<limbs>(lhs_abs);
        int k = e - 1; // index of highest set bit of rhs_abs
        if (hb != k)
            return hb < k ? -1 : 1;
        const int p = std::numeric_limits<T>::digits;
        int shift = hb - (p - 1);
        // Preserve aggregate-copy temporaries without depending on integer.
        struct magnitude_buffer
        {
            limb_type words[limbs];
        };
        magnitude_buffer scaled;
        for (size_t i = 0; i < limbs; ++i)
            scaled.words[i] = lhs_abs[i];
        unsigned __int128 sigA = 0;
        if (limbs >= 2)
            sigA = (static_cast<unsigned __int128>(lhs_abs[1]) << 64) | lhs_abs[0];
        else
            sigA = lhs_abs[0];
        if (shift > 0)
        {
            const limb_type fill = ExtendHighBit && (lhs_abs[limbs - 1] >> 63) ? ~limb_type(0) : 0;
            limb_shift<limbs>::shift_right_assign(scaled.words, static_cast<size_t>(shift) / 64, static_cast<unsigned>(shift % 64), fill);
            if (limbs >= 2)
                sigA = (static_cast<unsigned __int128>(scaled.words[1]) << 64) | scaled.words[0];
            else
                sigA = scaled.words[0];
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
            magnitude_buffer rec = scaled;
            limb_shift<limbs>::shift_left_assign(rec.words, static_cast<size_t>(shift));
            return equal_limbs(rec.words, lhs_abs) ? 0 : 1; // lhs has extra low bits -> larger
        }
    }

    // The caller clears data and supplies a finite, non-negative magnitude.
    static void assign_float_digits(limb_type * data, long double val) noexcept
    {
        long double intpart;
        std::modf(val, &intpart);
        val = intpart;
        long double base = std::ldexp(1.0L, 64);
        for (size_t i = 0; i < limbs && val > 0; ++i)
        {
            long double rem = std::fmod(val, base);
            data[i] = static_cast<limb_type>(rem);
            val = std::floor(val / base);
        }
    }
};

//=== Unsigned magnitude division ============================================
// Kernels accept little-endian arrays of exactly Limbs words. Outputs must not
// overlap inputs. The caller supplies magnitudes and handles signs/zero policy.
// Every quotient/remainder kernel initializes its complete output array.
template <size_t Limbs>
struct limb_division
{
    using limb_type = uint64_t;
    static constexpr size_t limbs = Limbs;

    static GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR bool greater_limbs_128(const uint64_t * lhs, const uint64_t * rhs) noexcept
    {
#if GINT_ARCH_AARCH64
        unsigned result;
        __asm__("cmp %[lhs_hi], %[rhs_hi]\n"
                "ccmp %[lhs_lo], %[rhs_lo], #0, eq\n"
                "cset %w[result], hi"
                : [result] "=r"(result)
                : [lhs_hi] "r"(lhs[1]), [rhs_hi] "r"(rhs[1]), [lhs_lo] "r"(lhs[0]), [rhs_lo] "r"(rhs[0])
                : "cc");
        return result != 0;
#else
        return lhs[1] > rhs[1] || (lhs[1] == rhs[1] && lhs[0] > rhs[0]);
#endif
    }

    static GINT_FORCE_INLINE void clear(limb_type * value) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            value[i] = 0;
    }

    static GINT_FORCE_INLINE void copy(limb_type * dst, const limb_type * src) noexcept
    {
        for (size_t i = 0; i < limbs; ++i)
            dst[i] = src[i];
    }

    template <size_t L = limbs>
    static GINT_SMALL_DIV_INLINE typename std::enable_if<(L == 1), limb_type>::type
    div_mod_small(const limb_type (&data)[limbs], limb_type div, limb_type (&quotient)[limbs]) noexcept
    {
        // SFINAE provides a dedicated implementation for single-limb integers,
        // avoiding multi-limb code that would trigger -Warray-bounds warnings.
        clear(quotient);
        if (data[0] == 0)
            return 0;
        quotient[0] = static_cast<limb_type>(data[0] / div);
        return static_cast<limb_type>(data[0] % div);
    }

    template <size_t L = limbs>
    static GINT_SMALL_DIV_INLINE typename std::enable_if<(L == 1), limb_type>::type
    mod_small(const limb_type * data, limb_type div) noexcept
    {
        return static_cast<limb_type>(data[0] % div);
    }

    template <size_t L = limbs>
    static GINT_SMALL_DIV_INLINE typename std::enable_if<(L > 1), limb_type>::type
    div_mod_small(const limb_type (&data)[limbs], limb_type div, limb_type (&quotient)[limbs]) noexcept
    {
        using u128 = unsigned __int128;
        // This overload is only instantiated for multi-limb integers, preventing
        // compilers from inspecting out-of-bounds accesses in single-limb cases.
        size_t n = limbs;
        // Initialize only the unused high limbs; every active limb is written
        // by the division path below. Keep this in the scan so an unused
        // quotient does not retain a separate bulk clear after inlining.
        while (n > 0 && data[n - 1] == 0)
            quotient[--n] = 0;
        if (n == 0)
            return 0;
        // Power-of-two divisor becomes a simple shift/modulo by mask.
        if ((div & (div - 1)) == 0)
        {
            const unsigned s = static_cast<unsigned>(__builtin_ctzll(div));
            if (s == 0)
            {
                copy(quotient, data);
            }
            else
            {
                const limb_type mask = (limb_type(1) << s) - 1;
                limb_type carry = 0;
                for (size_t i = limbs; i-- > 0;)
                {
                    const limb_type cur = data[i];
                    quotient[i] = (cur >> s) | (carry << (64 - s));
                    carry = cur & mask;
                }
            }
            return static_cast<limb_type>(data[0] & (div - 1));
        }
#if GINT_DETAIL_AARCH64_CLANG
        if (limbs == 2 && div > 0xFFFFFFFFULL)
        {
            const u128 num = (static_cast<u128>(data[1]) << 64) | data[0];
            const u128 q = num / div;
            quotient[0] = static_cast<limb_type>(q);
            quotient[1] = static_cast<limb_type>(q >> 64);
            return static_cast<limb_type>(num % div);
        }
#endif
#if GINT_ARCH_X86_64
#    if GINT_CLANG_TUNED_PATHS
        if (div != 10000000000000000000ULL)
#    endif
        {
            u128 rem = 0;
            for (size_t i = n; i-- > 0;)
            {
                u128 num = (rem << 64) | data[i];
                quotient[i] = static_cast<limb_type>(num / div);
                rem = num % div;
            }
            return static_cast<limb_type>(rem);
        }
#endif
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
                const uint64_t cur = data[i];
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
                quotient[i] = (static_cast<uint64_t>(qhi) << 32) | static_cast<uint32_t>(qlo);
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
                    u128 num = data[0];
                    u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
                    u128 rem = num - q * div;
                    corr(q, rem);
                    quotient[0] = static_cast<limb_type>(q);
                    return static_cast<limb_type>(rem);
                }
                case 2: {
                    u128 num = (static_cast<u128>(data[1]) << 64) | data[0];
                    u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
                    u128 rem = num - q * div;
                    corr(q, rem);
                    quotient[0] = static_cast<limb_type>(q);
                    quotient[1] = static_cast<limb_type>(q >> 64);
                    return static_cast<limb_type>(rem);
                }
                case 3: {
                    u128 rem = 0;
                    u128 num = (rem << 64) | data[2];
                    u128 q2 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q2 * div;
                    corr(q2, rem);
                    quotient[2] = static_cast<limb_type>(q2);
                    num = (rem << 64) | data[1];
                    u128 q1 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q1 * div;
                    corr(q1, rem);
                    quotient[1] = static_cast<limb_type>(q1);
                    num = (rem << 64) | data[0];
                    u128 q0 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q0 * div;
                    corr(q0, rem);
                    quotient[0] = static_cast<limb_type>(q0);
                    return static_cast<limb_type>(rem);
                }
                case 4:
                default: {
                    u128 rem = 0;
                    u128 num = (rem << 64) | data[3];
                    u128 q3 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q3 * div;
                    corr(q3, rem);
                    quotient[3] = static_cast<limb_type>(q3);
                    num = (rem << 64) | data[2];
                    u128 q2 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q2 * div;
                    corr(q2, rem);
                    quotient[2] = static_cast<limb_type>(q2);
                    num = (rem << 64) | data[1];
                    u128 q1 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q1 * div;
                    corr(q1, rem);
                    quotient[1] = static_cast<limb_type>(q1);
                    num = (rem << 64) | data[0];
                    u128 q0 = detail::mulhi_u128_no_middle_wrap(num, inv);
                    rem = num - q0 * div;
                    corr(q0, rem);
                    quotient[0] = static_cast<limb_type>(q0);
                    return static_cast<limb_type>(rem);
                }
            }
        }
        // Generic path for other limb counts
        u128 rem = 0;
        for (size_t i = n; i-- > 0;)
        {
            u128 num = (rem << 64) | data[i];
            u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
            rem = num - q * div;
            corr(q, rem);
            quotient[i] = static_cast<limb_type>(q);
        }
        return static_cast<limb_type>(rem);
    }

    template <size_t L = limbs>
    static GINT_SMALL_DIV_INLINE typename std::enable_if<(L > 1), limb_type>::type mod_small(const limb_type * data, limb_type div) noexcept
    {
        using u128 = unsigned __int128;
        size_t n = limbs;
        while (n > 0 && data[n - 1] == 0)
            --n;
        if (n == 0)
            return 0;

        if ((div & (div - 1)) == 0)
            return static_cast<limb_type>(data[0] & (div - 1));

#if GINT_DETAIL_AARCH64_CLANG || GINT_DETAIL_AARCH64_GCC
        if (limbs == 2 && div > 0xFFFFFFFFULL)
        {
            const u128 num = (static_cast<u128>(data[1]) << 64) | data[0];
            return static_cast<limb_type>(num % div);
        }
#endif

#if GINT_ARCH_X86_64
        {
            u128 rem = 0;
            for (size_t i = n; i-- > 0;)
                rem = ((rem << 64) | data[i]) % div;
            return static_cast<limb_type>(rem);
        }
#endif

        if (div <= 0xFFFFFFFFULL)
        {
            using u128x = unsigned __int128;
            const uint32_t d32 = static_cast<uint32_t>(div);
            const uint64_t rinv = ~uint64_t(0) / static_cast<uint64_t>(d32);
            uint64_t rem = 0;
            for (size_t i = n; i-- > 0;)
            {
                const uint64_t cur = data[i];
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
            u128 num = (rem << 64) | data[i];
            u128 q = detail::mulhi_u128_no_middle_wrap(num, inv);
            rem = num - q * div;
            corr(q, rem);
        }
        return static_cast<limb_type>(rem);
    }


    //=== Multi-limb division and remainder kernels ==========================
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
    static typename std::enable_if<(L >= 2), void>::type
    div_128(const limb_type * lhs, const limb_type * rhs, limb_type * GINT_RESTRICT result) noexcept
    {
#if GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG
        clear(result);
        if (GINT_UNLIKELY((rhs[1] | rhs[0]) == 0))
            return;
        if (rhs[1] >= (limb_type(1) << 62))
        {
            limb_type rem_hi = lhs[1];
            limb_type rem_lo = lhs[0];
            limb_type q = 0;
            for (limb_type i = 0; i < 3; ++i)
            {
                if (rem_hi < rhs[1] || (rem_hi == rhs[1] && rem_lo < rhs[0]))
                    break;
                limb_type next_lo = rem_lo - rhs[0];
                limb_type borrow = rem_lo < rhs[0];
                rem_hi = rem_hi - rhs[1] - borrow;
                rem_lo = next_lo;
                ++q;
            }
            result[0] = q;
            result[1] = 0;
            return;
        }
#endif
        return div_128_native(lhs, rhs, result);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), void>::type
    div_128_native(const limb_type * lhs, const limb_type * rhs, limb_type * GINT_RESTRICT result) noexcept
    {
        unsigned __int128 a = (static_cast<unsigned __int128>(lhs[1]) << 64) | lhs[0];
        unsigned __int128 b = (static_cast<unsigned __int128>(rhs[1]) << 64) | rhs[0];
        clear(result);
        if (GINT_UNLIKELY(b == 0))
            return;
        unsigned __int128 q = a / b;
        result[0] = static_cast<limb_type>(q);
        result[1] = static_cast<limb_type>(q >> 64);
        return;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), void>::type
    div_128(const limb_type * lhs, const limb_type * rhs, limb_type * GINT_RESTRICT result) noexcept
    {
        return div_128_native(lhs, rhs, result);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), void>::type
    div_128_native(const limb_type * lhs, const limb_type * rhs, limb_type * GINT_RESTRICT result) noexcept
    {
        clear(result);
        if (GINT_UNLIKELY(rhs[0] == 0))
            return;
        result[0] = lhs[0] / rhs[0];
        return;
    }

#if GINT_GCC_TUNED_PATHS
    GINT_FORCE_INLINE static limb_type left_shifted_limb_at(const limb_type * src, size_t i, int shift) noexcept
    {
        limb_type cur = src[i];
        if (shift == 0)
            return cur;
        limb_type prev = i == 0 ? 0 : src[i - 1];
        return static_cast<limb_type>((cur << shift) | (prev >> (64 - shift)));
    }

    static bool mul_by_limb_greater_than(const limb_type * lhs, const limb_type * divisor, size_t div_limbs, limb_type q) noexcept
    {
        if (q == 0)
            return false;

        std::array<limb_type, limbs + 1> product;
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor[i]) * q + carry;
            product[i] = static_cast<limb_type>(p);
            carry = p >> 64;
        }
        if (carry != 0)
            return true;

        for (size_t i = div_limbs; i-- > 0;)
        {
            if (product[i] != lhs[i])
                return product[i] > lhs[i];
        }
        return false;
    }

    static void div_large_single_limb_quotient(
        const limb_type * lhs, const limb_type * divisor, size_t div_limbs, limb_type * GINT_RESTRICT quotient) noexcept
    {
        clear(quotient);
        if (div_limbs < 2)
            return;

        using u128 = unsigned __int128;
        const int shift = __builtin_clzll(divisor[div_limbs - 1]);
        const limb_type vtop = left_shifted_limb_at(divisor, div_limbs - 1, shift);
        const limb_type vnext = left_shifted_limb_at(divisor, div_limbs - 2, shift);
        const limb_type utop = shift ? static_cast<limb_type>(lhs[div_limbs - 1] >> (64 - shift)) : 0;
        const limb_type unext = left_shifted_limb_at(lhs, div_limbs - 1, shift);
        const limb_type uthird = left_shifted_limb_at(lhs, div_limbs - 2, shift);

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
        quotient[0] = q;
        return;
    }
#endif

    template <bool WantRemainder>
    static GINT_NOINLINE void div_or_rem_large_core(
        const limb_type * lhs, const limb_type * divisor, size_t v_limbs, size_t u_limbs, limb_type * GINT_RESTRICT result) noexcept
    {
        clear(result);
        if (GINT_UNLIKELY(v_limbs == 0) || u_limbs < v_limbs)
        {
            if (WantRemainder)
                copy(result, lhs);
            return;
        }

        std::array<limb_type, limbs + 1> u;
        std::array<limb_type, limbs + 1> v;

        int shift = __builtin_clzll(divisor[v_limbs - 1]);
        limb_type carry = lshift_limbs_to(lhs, u_limbs, u.data(), shift);
        u[u_limbs] = carry;

        carry = lshift_limbs_to(divisor, v_limbs, v.data(), shift);

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
                result[j] = static_cast<limb_type>(qhat);
        }

        if (WantRemainder)
        {
            if (shift == 0)
            {
                for (size_t i = 0; i < v_limbs; ++i)
                    result[i] = u[i];
            }
            else
            {
                const int inv_shift = 64 - shift;
                for (size_t i = 0; i < v_limbs; ++i)
                {
                    const limb_type next = (i + 1 < v_limbs) ? u[i + 1] : 0;
                    result[i] = (u[i] >> shift) | (next << inv_shift);
                }
            }
        }
        return;
    }

    static void div_large(const limb_type * lhs, const limb_type * divisor, size_t div_limbs, limb_type * GINT_RESTRICT result) noexcept
    {
        const size_t dividend_limbs = detail::used_limbs<limbs>(lhs);
#if GINT_GCC_TUNED_PATHS
        if (dividend_limbs == div_limbs && div_limbs >= 2)
            return div_large_single_limb_quotient(lhs, divisor, div_limbs, result);
#endif
        return div_or_rem_large_core<false>(lhs, divisor, div_limbs, dividend_limbs, result);
    }

#if GINT_DETAIL_AARCH64_GCC
    static limb_type rem_estimate_single_limb_quotient(const limb_type * lhs, const limb_type * divisor, size_t div_limbs) noexcept
    {
        using u128 = unsigned __int128;
        const int shift = __builtin_clzll(divisor[div_limbs - 1]);
        const limb_type vtop = left_shifted_limb_at(divisor, div_limbs - 1, shift);
        const limb_type vnext = left_shifted_limb_at(divisor, div_limbs - 2, shift);
        const limb_type utop = shift ? static_cast<limb_type>(lhs[div_limbs - 1] >> (64 - shift)) : 0;
        const limb_type unext = left_shifted_limb_at(lhs, div_limbs - 1, shift);
        const limb_type uthird = left_shifted_limb_at(lhs, div_limbs - 2, shift);

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

    static bool rem_sub_mul_limb(limb_type * lhs, const limb_type * divisor, size_t div_limbs, limb_type q) noexcept
    {
        unsigned __int128 carry = 0;
        limb_type borrow = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor[i]) * q + carry;
            carry = p >> 64;

            unsigned __int128 subtrahend = static_cast<unsigned __int128>(static_cast<limb_type>(p)) + borrow;
            limb_type next_borrow = static_cast<unsigned __int128>(lhs[i]) < subtrahend;
            lhs[i] = static_cast<limb_type>(static_cast<unsigned __int128>(lhs[i]) - subtrahend);
            borrow = next_borrow;
        }
        return carry != 0 || borrow != 0;
    }

    static bool rem_sub_mul_limb_full_width(limb_type * lhs, const limb_type * divisor, limb_type q) noexcept
    {
        unsigned __int128 carry = 0;
        limb_type borrow = 0;
        for (size_t i = 0; i < limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor[i]) * q + carry;
            carry = p >> 64;

            unsigned __int128 subtrahend = static_cast<unsigned __int128>(static_cast<limb_type>(p)) + borrow;
            limb_type next_borrow = static_cast<unsigned __int128>(lhs[i]) < subtrahend;
            lhs[i] = static_cast<limb_type>(static_cast<unsigned __int128>(lhs[i]) - subtrahend);
            borrow = next_borrow;
        }
        return carry != 0 || borrow != 0;
    }

    static void rem_add_divisor(limb_type * lhs, const limb_type * divisor, size_t div_limbs) noexcept
    {
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 sum = static_cast<unsigned __int128>(lhs[i]) + divisor[i] + carry;
            lhs[i] = static_cast<limb_type>(sum);
            carry = sum >> 64;
        }
    }

    static void rem_add_divisor_full_width(limb_type * lhs, const limb_type * divisor) noexcept
    {
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < limbs; ++i)
        {
            unsigned __int128 sum = static_cast<unsigned __int128>(lhs[i]) + divisor[i] + carry;
            lhs[i] = static_cast<limb_type>(sum);
            carry = sum >> 64;
        }
    }

    static void rem_large_single_limb_quotient(
        const limb_type * lhs, const limb_type * divisor, size_t div_limbs, limb_type * GINT_RESTRICT result) noexcept
    {
        copy(result, lhs);
        if (div_limbs < 2)
            return;

        const limb_type q = rem_estimate_single_limb_quotient(result, divisor, div_limbs);
        if (div_limbs == limbs)
        {
            if (rem_sub_mul_limb_full_width(result, divisor, q))
                rem_add_divisor_full_width(result, divisor);
        }
        else if (rem_sub_mul_limb(result, divisor, div_limbs, q))
        {
            rem_add_divisor(result, divisor, div_limbs);
        }
        return;
    }
#endif

    static void rem_large(const limb_type * lhs, const limb_type * divisor, size_t div_limbs, limb_type * GINT_RESTRICT result) noexcept
    {
        const size_t dividend_limbs = detail::used_limbs<limbs>(lhs);
#if GINT_DETAIL_AARCH64_GCC
        if (dividend_limbs == div_limbs && div_limbs >= 2)
            return rem_large_single_limb_quotient(lhs, divisor, div_limbs, result);
#endif
        return div_or_rem_large_core<true>(lhs, divisor, div_limbs, dividend_limbs, result);
    }

    // Optimized specialization: full-width 256-bit divisor (divisor_limbs == 4)
    template <size_t L = limbs>
    static GINT_NOINLINE typename std::enable_if<(L == 4), void>::type
    div_large_4(const limb_type * lhs, const limb_type * divisor, limb_type * GINT_RESTRICT quotient) noexcept
    {
        clear(quotient);
        if (lhs[3] == 0)
            return;

        using u128 = unsigned __int128;

        const int shift = __builtin_clzll(divisor[3]);
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
            u0 = lhs[0];
            u1 = lhs[1];
            u2 = lhs[2];
            u3 = lhs[3];
            u4 = 0;
            v0 = divisor[0];
            v1 = divisor[1];
            v2 = divisor[2];
            v3 = divisor[3];
        }
        else
        {
            const int inv_shift = 64 - shift;
            u0 = lhs[0] << shift;
            u1 = (lhs[1] << shift) | (lhs[0] >> inv_shift);
            u2 = (lhs[2] << shift) | (lhs[1] >> inv_shift);
            u3 = (lhs[3] << shift) | (lhs[2] >> inv_shift);
            u4 = lhs[3] >> inv_shift;
            v0 = divisor[0] << shift;
            v1 = (divisor[1] << shift) | (divisor[0] >> inv_shift);
            v2 = (divisor[2] << shift) | (divisor[1] >> inv_shift);
            v3 = (divisor[3] << shift) | (divisor[2] >> inv_shift);
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

        quotient[0] = static_cast<limb_type>(qhat);
        return;
    }

    static GINT_NOINLINE void rem_large_4_impl(const limb_type * lhs, const limb_type * divisor, limb_type * GINT_RESTRICT result) noexcept
    {
        clear(result);
        if (lhs[3] == 0)
            return copy(result, lhs);

        using u128 = unsigned __int128;

        const int shift = __builtin_clzll(divisor[3]);
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
            u0 = lhs[0];
            u1 = lhs[1];
            u2 = lhs[2];
            u3 = lhs[3];
            u4 = 0;
            v0 = divisor[0];
            v1 = divisor[1];
            v2 = divisor[2];
            v3 = divisor[3];
        }
        else
        {
            const int inv_shift = 64 - shift;
            u0 = lhs[0] << shift;
            u1 = (lhs[1] << shift) | (lhs[0] >> inv_shift);
            u2 = (lhs[2] << shift) | (lhs[1] >> inv_shift);
            u3 = (lhs[3] << shift) | (lhs[2] >> inv_shift);
            u4 = lhs[3] >> inv_shift;
            v0 = divisor[0] << shift;
            v1 = (divisor[1] << shift) | (divisor[0] >> inv_shift);
            v2 = (divisor[2] << shift) | (divisor[1] >> inv_shift);
            v3 = (divisor[3] << shift) | (divisor[2] >> inv_shift);
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
            result[0] = u0;
            result[1] = u1;
            result[2] = u2;
            result[3] = u3;
        }
        else
        {
            const int inv_shift = 64 - shift;
            result[0] = (u0 >> shift) | (u1 << inv_shift);
            result[1] = (u1 >> shift) | (u2 << inv_shift);
            result[2] = (u2 >> shift) | (u3 << inv_shift);
            result[3] = (u3 >> shift) | (u4 << inv_shift);
        }
        return;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L == 4), void>::type
    rem_large_4(const limb_type * lhs, const limb_type * divisor, limb_type * GINT_RESTRICT result) noexcept
    {
        return rem_large_4_impl(lhs, divisor, result);
    }

    // Stub for non-256-bit instantiations to keep dependent calls well-formed.
    template <size_t L = limbs>
    static typename std::enable_if<(L != 4), void>::type
    div_large_4(const limb_type * lhs, const limb_type * divisor, limb_type * GINT_RESTRICT result) noexcept
    {
        return div_large(lhs, divisor, 4, result);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L != 4), void>::type
    rem_large_4(const limb_type * lhs, const limb_type *, limb_type * GINT_RESTRICT result) noexcept
    {
        return copy(result, lhs);
    }

    // Optimized specialization: two-limb divisor (divisor_limbs == 2)
    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), void>::type
    div_large_2(const limb_type * lhs, const limb_type * divisor, limb_type * GINT_RESTRICT quotient) noexcept GINT_CLANG_NOINLINE
    {
        clear(quotient);
        size_t n = limbs;
        while (n > 0 && lhs[n - 1] == 0)
            --n;
        if (n < 2)
            return;

        std::array<limb_type, limbs + 1> u = {{}};

        // Normalize divisor so that the top limb has its MSB set.
        const limb_type d0 = divisor[0];
        const limb_type d1 = divisor[1];
        int shift = __builtin_clzll(d1);

        limb_type carry = lshift_limbs_to(lhs, n, u.data(), shift);
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
                quotient[j] = static_cast<limb_type>(qhat);
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
                quotient[j] = static_cast<limb_type>(qhat);
            }
        }
        return;
    }


    // Optimized specialization: three-limb divisor (divisor_limbs == 3)
    template <size_t L = limbs>
    static typename std::enable_if<(L >= 3), void>::type
    div_large_3(const limb_type * lhs, const limb_type * divisor, limb_type * GINT_RESTRICT quotient) noexcept
    {
        clear(quotient);
        size_t n = limbs;
        while (n > 0 && lhs[n - 1] == 0)
            --n;
        if (n < 3)
            return;

        std::array<limb_type, limbs + 1> u = {{}};
        std::array<limb_type, 3> v = {{}};

        // Normalize divisor: ensure MSB of v[2] is set
        int shift = __builtin_clzll(divisor[2]);
        limb_type carry = 0;
        for (size_t i = 0; i < n; ++i)
        {
            limb_type cur = lhs[i];
            u[i] = (cur << shift) | carry;
            carry = shift ? static_cast<limb_type>(cur >> (64 - shift)) : 0;
        }
        u[n] = carry;

        carry = lshift_limbs_to(divisor, 3, v.data(), shift);

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
            quotient[j] = static_cast<limb_type>(qhat);
        }
        return;
    }
};

} // namespace detail

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint
