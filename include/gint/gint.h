#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#ifdef GINT_ENABLE_FMT
#    include <fmt/format.h>
#endif

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#    include <x86intrin.h>
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

#if defined(__clang__)
#    define GINT_CLANG_NOINLINE __attribute__((noinline))
#else
#    define GINT_CLANG_NOINLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define GINT_NOINLINE __attribute__((noinline))
#    define GINT_COLD __attribute__((cold))
#else
#    define GINT_NOINLINE
#    define GINT_COLD
#endif

#define GINT_ZERO_CHECK(cond, msg) \
    do \
    { \
        if (GINT_UNLIKELY(cond) && ::gint::detail::divzero_checks_enabled()) \
            throw std::domain_error(msg); \
    } while (false)
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

#ifndef __has_builtin
#    define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_is_constant_evaluated)
#    define GINT_HAS_IS_CONSTANT_EVALUATED 1
#else
#    define GINT_HAS_IS_CONSTANT_EVALUATED 0
#endif

#if defined(__aarch64__)
#    define GINT_ARCH_AARCH64 1
#else
#    define GINT_ARCH_AARCH64 0
#endif

#if defined(__x86_64__)
#    define GINT_ARCH_X86_64 1
#else
#    define GINT_ARCH_X86_64 0
#endif

#ifndef GINT_ENABLE_AARCH64_LIMB_ASM
#    if GINT_ARCH_AARCH64
#        define GINT_ENABLE_AARCH64_LIMB_ASM 1
#    else
#        define GINT_ENABLE_AARCH64_LIMB_ASM 0
#    endif
#endif

// Compiler-tuned path policy:
// - GCC benefits from selected small-number/division/modulo fast paths.
// - Clang benefits from a narrower modulo fast path and otherwise prefers
//   several generic paths that optimize better under AppleClang on AArch64.
#ifndef GINT_GCC_TUNED_PATHS
#    if defined(__clang__)
#        define GINT_GCC_TUNED_PATHS 0
#    elif defined(__GNUC__)
#        define GINT_GCC_TUNED_PATHS 1
#    else
#        define GINT_GCC_TUNED_PATHS 0
#    endif
#endif

#ifndef GINT_CLANG_TUNED_PATHS
#    if defined(__clang__)
#        define GINT_CLANG_TUNED_PATHS 1
#    else
#        define GINT_CLANG_TUNED_PATHS 0
#    endif
#endif

#if GINT_GCC_TUNED_PATHS && GINT_CLANG_TUNED_PATHS
#    error "GINT_GCC_TUNED_PATHS and GINT_CLANG_TUNED_PATHS are mutually exclusive"
#endif

#define GINT_X86_64_GCC_TUNED_PATHS (GINT_ARCH_X86_64 && GINT_GCC_TUNED_PATHS)
#define GINT_NON_X86_GCC_TUNED_PATHS (GINT_GCC_TUNED_PATHS && !GINT_ARCH_X86_64)

#ifndef GINT_ENABLE_X86_64_ADD_INTRINSICS
#    define GINT_ENABLE_X86_64_ADD_INTRINSICS (GINT_ARCH_X86_64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_X86_64_WIDE_ADD_INTRINSICS
#    define GINT_ENABLE_X86_64_WIDE_ADD_INTRINSICS (GINT_ARCH_X86_64 && (GINT_GCC_TUNED_PATHS || GINT_CLANG_TUNED_PATHS))
#endif

#ifndef GINT_ENABLE_X86_64_SUB_INTRINSICS
#    define GINT_ENABLE_X86_64_SUB_INTRINSICS (GINT_ARCH_X86_64 && (GINT_GCC_TUNED_PATHS || GINT_CLANG_TUNED_PATHS))
#endif

#ifndef GINT_ENABLE_AARCH64_WIDE_ADDSUB_BUILTINS
#    define GINT_ENABLE_AARCH64_WIDE_ADDSUB_BUILTINS \
        (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS && __has_builtin(__builtin_addcll) && __has_builtin(__builtin_subcll))
#endif

#ifndef GINT_ENABLE_AARCH64_UINT128_SMALL_DIVMOD
#    define GINT_ENABLE_AARCH64_UINT128_SMALL_DIVMOD (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_AARCH64_UINT128_SMALL_MOD
#    define GINT_ENABLE_AARCH64_UINT128_SMALL_MOD (GINT_ARCH_AARCH64 && GINT_GCC_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_MUL4_LOW128_FASTPATH
#    define GINT_ENABLE_MUL4_LOW128_FASTPATH GINT_NON_X86_GCC_TUNED_PATHS
#endif

#ifndef GINT_ENABLE_MUL4_RHS_SINGLE_LIMB_FASTPATH
#    define GINT_ENABLE_MUL4_RHS_SINGLE_LIMB_FASTPATH GINT_NON_X86_GCC_TUNED_PATHS
#endif

#ifndef GINT_ENABLE_MUL4_LHS_SINGLE_LIMB_FASTPATH
#    define GINT_ENABLE_MUL4_LHS_SINGLE_LIMB_FASTPATH GINT_NON_X86_GCC_TUNED_PATHS
#endif

#ifndef GINT_ENABLE_X86_64_GCC_MUL4_U64_FASTPATH
#    define GINT_ENABLE_X86_64_GCC_MUL4_U64_FASTPATH (GINT_ARCH_X86_64 && GINT_GCC_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_WIDE_SINGLE_LIMB_MUL_FASTPATH
#    define GINT_ENABLE_WIDE_SINGLE_LIMB_MUL_FASTPATH 1
#endif

#ifndef GINT_ENABLE_REM_QUOTIENT_MUL_FASTPATH
#    define GINT_ENABLE_REM_QUOTIENT_MUL_FASTPATH GINT_GCC_TUNED_PATHS
#endif

#ifndef GINT_ENABLE_POSITIVE_LIMB_DIV_FASTPATH
#    define GINT_ENABLE_POSITIVE_LIMB_DIV_FASTPATH GINT_GCC_TUNED_PATHS
#endif

#ifndef GINT_ENABLE_AARCH64_LIMB2_POSITIVE_LIMB_DIV_FASTPATH
#    define GINT_ENABLE_AARCH64_LIMB2_POSITIVE_LIMB_DIV_FASTPATH (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_POSITIVE_LIMB_REM_FASTPATH
#    define GINT_ENABLE_POSITIVE_LIMB_REM_FASTPATH (GINT_CLANG_TUNED_PATHS || GINT_ARCH_X86_64)
#endif

#ifndef GINT_ENABLE_X86_64_HW_SMALL_DIVMOD
#    define GINT_ENABLE_X86_64_HW_SMALL_DIVMOD GINT_ARCH_X86_64
#endif

#ifndef GINT_ENABLE_AARCH64_TWO_LIMB_LARGE_DIVISOR_FASTPATH
#    define GINT_ENABLE_AARCH64_TWO_LIMB_LARGE_DIVISOR_FASTPATH (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_AARCH64_INT128_NEGATIVE_ZERO_DIV_FASTPATH
#    define GINT_ENABLE_AARCH64_INT128_NEGATIVE_ZERO_DIV_FASTPATH (GINT_ARCH_AARCH64 && (GINT_CLANG_TUNED_PATHS || GINT_GCC_TUNED_PATHS))
#endif

#ifndef GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR
#    if GINT_GCC_TUNED_PATHS
#        define GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR GINT_NOINLINE
#    else
#        define GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR GINT_FORCE_INLINE
#    endif
#endif

#ifndef GINT_ENABLE_AARCH64_TWO_LIMB_POSITIVE_POW2_DIV_FASTPATH
#    define GINT_ENABLE_AARCH64_TWO_LIMB_POSITIVE_POW2_DIV_FASTPATH (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_AARCH64_INT128_SIGNED_LIMB_DIV_FASTPATH
#    define GINT_ENABLE_AARCH64_INT128_SIGNED_LIMB_DIV_FASTPATH (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_AARCH64_XOR16_UNROLL_FASTPATH
#    define GINT_ENABLE_AARCH64_XOR16_UNROLL_FASTPATH (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_AARCH64_INT128_UNSIGNED_RIGHT_SHIFT_FASTPATH
#    define GINT_ENABLE_AARCH64_INT128_UNSIGNED_RIGHT_SHIFT_FASTPATH (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_AARCH64_INT128_LEFT_SHIFT_FASTPATH
#    define GINT_ENABLE_AARCH64_INT128_LEFT_SHIFT_FASTPATH (GINT_ARCH_AARCH64 && GINT_GCC_TUNED_PATHS)
#endif

#ifndef GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH
#    ifdef GINT_ENABLE_AARCH64_GCC_WIDE_LEFT_SHIFT_UNSIGNED_FASTPATH
#        define GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH GINT_ENABLE_AARCH64_GCC_WIDE_LEFT_SHIFT_UNSIGNED_FASTPATH
#    elif defined(GINT_ENABLE_AARCH64_GCC_WIDE_RIGHT_SHIFT_UNSIGNED_FASTPATH)
#        define GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH GINT_ENABLE_AARCH64_GCC_WIDE_RIGHT_SHIFT_UNSIGNED_FASTPATH
#    else
#        define GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH (GINT_ARCH_AARCH64 && GINT_GCC_TUNED_PATHS)
#    endif
#endif

#if GINT_X86_64_GCC_TUNED_PATHS
#    define GINT_WIDE_SHIFT_INLINE inline GINT_NOINLINE GINT_COLD
#else
#    define GINT_WIDE_SHIFT_INLINE GINT_FORCE_INLINE
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
GINT_FORCE_INLINE bool & divzero_checks_flag() noexcept
{
    static bool enabled = false;
    return enabled;
}

GINT_FORCE_INLINE bool divzero_checks_enabled() noexcept
{
    return divzero_checks_flag();
}

#if defined(GINT_ENABLE_DIVZERO_CHECKS)
struct divzero_checks_registrar
{
    divzero_checks_registrar() noexcept { divzero_checks_flag() = true; }
};

namespace
{
static const divzero_checks_registrar k_divzero_checks_registrar;
}
#endif

#ifdef GINT_TEST_ACCESS
template <size_t Bits, typename Signed>
struct integer_test_access;
#endif

template <size_t Bits>
struct storage_count
{
    static_assert(Bits >= 64, "Bits must be at least 64");
    static_assert(Bits <= 1024, "Bits must be at most 1024");
    static_assert((Bits & (Bits - 1)) == 0, "Bits must be a power of two");
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
GINT_CONSTEXPR14 inline void add_limbs_scalar(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned __int128 sum = static_cast<unsigned __int128>(lhs[i]) + rhs[i] + carry;
        lhs[i] = static_cast<uint64_t>(sum);
        carry = sum >> 64;
    }
}

template <size_t L>
GINT_FORCE_INLINE void add_limbs_runtime(uint64_t * lhs, const uint64_t * rhs) noexcept
{
#if GINT_ENABLE_X86_64_WIDE_ADD_INTRINSICS
    unsigned char carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        carry = _addcarry_u64(carry, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        lhs[i] = static_cast<uint64_t>(r);
    }
    return;
#elif GINT_ENABLE_AARCH64_WIDE_ADDSUB_BUILTINS
    unsigned long long carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long carry_out;
        const unsigned long long r
            = __builtin_addcll(static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), carry, &carry_out);
        lhs[i] = static_cast<uint64_t>(r);
        carry = carry_out;
    }
    return;
#endif
    add_limbs_scalar<L>(lhs, rhs);
}

template <>
GINT_FORCE_INLINE void add_limbs_runtime<1>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    lhs[0] += rhs[0];
}

template <>
GINT_FORCE_INLINE void add_limbs_runtime<2>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    add_limbs_scalar<2>(lhs, rhs);
}

template <>
GINT_FORCE_INLINE void add_limbs_runtime<4>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
#if GINT_ENABLE_X86_64_ADD_INTRINSICS
    unsigned long long r0;
    unsigned long long r1;
    unsigned long long r2;
    unsigned long long r3;
    unsigned char c = _addcarry_u64(0, static_cast<unsigned long long>(lhs[0]), static_cast<unsigned long long>(rhs[0]), &r0);
    c = _addcarry_u64(c, static_cast<unsigned long long>(lhs[1]), static_cast<unsigned long long>(rhs[1]), &r1);
    c = _addcarry_u64(c, static_cast<unsigned long long>(lhs[2]), static_cast<unsigned long long>(rhs[2]), &r2);
    _addcarry_u64(c, static_cast<unsigned long long>(lhs[3]), static_cast<unsigned long long>(rhs[3]), &r3);
    lhs[0] = static_cast<uint64_t>(r0);
    lhs[1] = static_cast<uint64_t>(r1);
    lhs[2] = static_cast<uint64_t>(r2);
    lhs[3] = static_cast<uint64_t>(r3);
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
                 "stp x8, x9, [%[lhs]]\n\t"
                 "stp x10, x11, [%[lhs], #16]"
                 :
                 : [lhs] "r"(lhs), [rhs] "r"(rhs)
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

    lhs[0] = static_cast<uint64_t>(lo_sum);
    lhs[1] = static_cast<uint64_t>(lo_sum >> 64);
    lhs[2] = static_cast<uint64_t>(hi_sum);
    lhs[3] = static_cast<uint64_t>(hi_sum >> 64);
}

template <size_t L>
GINT_CONSTEXPR14 inline void add_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
{
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        add_limbs_scalar<L>(lhs, rhs);
        return;
    }
    add_limbs_runtime<L>(lhs, rhs);
#elif __cplusplus >= 201402L
    add_limbs_scalar<L>(lhs, rhs);
#else
    add_limbs_runtime<L>(lhs, rhs);
#endif
}

template <size_t L>
GINT_CONSTEXPR14 inline void sub_limbs_scalar(uint64_t * lhs, const uint64_t * rhs) noexcept
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

template <size_t L>
GINT_FORCE_INLINE void sub_limbs_runtime(uint64_t * lhs, const uint64_t * rhs) noexcept
{
#if GINT_ENABLE_X86_64_SUB_INTRINSICS
    unsigned char borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        borrow = _subborrow_u64(borrow, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        lhs[i] = static_cast<uint64_t>(r);
    }
    return;
#elif GINT_ENABLE_AARCH64_WIDE_ADDSUB_BUILTINS
    unsigned long long borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long borrow_out;
        const unsigned long long r
            = __builtin_subcll(static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), borrow, &borrow_out);
        lhs[i] = static_cast<uint64_t>(r);
        borrow = borrow_out;
    }
    return;
#endif
    sub_limbs_scalar<L>(lhs, rhs);
}

template <>
GINT_FORCE_INLINE void sub_limbs_runtime<2>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
    sub_limbs_scalar<2>(lhs, rhs);
}

template <>
GINT_FORCE_INLINE void sub_limbs_runtime<4>(uint64_t * lhs, const uint64_t * rhs) noexcept
{
#if GINT_ENABLE_X86_64_SUB_INTRINSICS
    unsigned long long r0;
    unsigned long long r1;
    unsigned long long r2;
    unsigned long long r3;
    unsigned char b = _subborrow_u64(0, static_cast<unsigned long long>(lhs[0]), static_cast<unsigned long long>(rhs[0]), &r0);
    b = _subborrow_u64(b, static_cast<unsigned long long>(lhs[1]), static_cast<unsigned long long>(rhs[1]), &r1);
    b = _subborrow_u64(b, static_cast<unsigned long long>(lhs[2]), static_cast<unsigned long long>(rhs[2]), &r2);
    _subborrow_u64(b, static_cast<unsigned long long>(lhs[3]), static_cast<unsigned long long>(rhs[3]), &r3);
    lhs[0] = static_cast<uint64_t>(r0);
    lhs[1] = static_cast<uint64_t>(r1);
    lhs[2] = static_cast<uint64_t>(r2);
    lhs[3] = static_cast<uint64_t>(r3);
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
                 "stp x8, x9, [%[lhs]]\n\t"
                 "stp x10, x11, [%[lhs], #16]"
                 :
                 : [lhs] "r"(lhs), [rhs] "r"(rhs)
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

    lhs[0] = static_cast<uint64_t>(lo_diff);
    lhs[1] = static_cast<uint64_t>(lo_diff >> 64);
    lhs[2] = static_cast<uint64_t>(hi_diff);
    lhs[3] = static_cast<uint64_t>(hi_diff >> 64);
}

template <size_t L>
GINT_CONSTEXPR14 inline void sub_limbs(uint64_t * lhs, const uint64_t * rhs) noexcept
{
#if GINT_HAS_IS_CONSTANT_EVALUATED && __cplusplus >= 201402L
    if (__builtin_is_constant_evaluated())
    {
        sub_limbs_scalar<L>(lhs, rhs);
        return;
    }
    sub_limbs_runtime<L>(lhs, rhs);
#elif __cplusplus >= 201402L
    sub_limbs_scalar<L>(lhs, rhs);
#else
    sub_limbs_runtime<L>(lhs, rhs);
#endif
}

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
#if GINT_ENABLE_X86_64_WIDE_ADD_INTRINSICS
    unsigned char carry = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        carry = _addcarry_u64(carry, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        dst[i] = static_cast<uint64_t>(r);
    }
    return;
#elif GINT_ENABLE_AARCH64_WIDE_ADDSUB_BUILTINS
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
#if GINT_ENABLE_X86_64_ADD_INTRINSICS
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
#if GINT_ENABLE_X86_64_SUB_INTRINSICS
    unsigned char borrow = 0;
    for (size_t i = 0; i < L; ++i)
    {
        unsigned long long r;
        borrow = _subborrow_u64(borrow, static_cast<unsigned long long>(lhs[i]), static_cast<unsigned long long>(rhs[i]), &r);
        dst[i] = static_cast<uint64_t>(r);
    }
    return;
#elif GINT_ENABLE_AARCH64_WIDE_ADDSUB_BUILTINS
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
#if GINT_ENABLE_X86_64_SUB_INTRINSICS
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
GINT_CONSTEXPR14 GINT_FORCE_INLINE void
bit_and_limbs(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    for (size_t i = 0; i < L; ++i)
        dst[i] = lhs[i] & rhs[i];
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
bit_xor_limbs<4>(uint64_t * GINT_RESTRICT dst, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
    dst[0] = lhs[0] ^ rhs[0];
    dst[1] = lhs[1] ^ rhs[1];
    dst[2] = lhs[2] ^ rhs[2];
    dst[3] = lhs[3] ^ rhs[3];
}

#if GINT_ENABLE_AARCH64_XOR16_UNROLL_FASTPATH
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

GINT_FORCE_INLINE void mul_limbs4_by_limb(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, uint64_t rhs) noexcept
{
    using u128 = unsigned __int128;
#if GINT_NON_X86_GCC_TUNED_PATHS
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
#if GINT_ENABLE_MUL4_RHS_SINGLE_LIMB_FASTPATH || GINT_ENABLE_MUL4_LHS_SINGLE_LIMB_FASTPATH || GINT_ENABLE_MUL4_LOW128_FASTPATH
    using u128 = unsigned __int128;
#else
    (void)res;
    (void)lhs;
    (void)rhs;
#endif
#if GINT_ENABLE_MUL4_RHS_SINGLE_LIMB_FASTPATH
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
#endif
#if GINT_ENABLE_MUL4_LHS_SINGLE_LIMB_FASTPATH
    if ((lhs[1] | lhs[2] | lhs[3]) == 0)
    {
        mul_limbs4_by_limb(res, rhs, lhs[0]);
        return true;
    }
#endif
#if GINT_ENABLE_MUL4_LOW128_FASTPATH
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
#endif
    return false;
}

#if GINT_ENABLE_X86_64_GCC_MUL4_U64_FASTPATH
inline GINT_NOINLINE GINT_COLD void mul_limbs4_u64(uint64_t * GINT_RESTRICT res, uint64_t lhs, uint64_t rhs) noexcept
{
    const unsigned __int128 p = static_cast<unsigned __int128>(lhs) * rhs;
    res[0] = static_cast<uint64_t>(p);
    res[1] = static_cast<uint64_t>(p >> 64);
    res[2] = 0;
    res[3] = 0;
}
#endif

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
#if GINT_ENABLE_X86_64_GCC_MUL4_U64_FASTPATH
    if (GINT_LIKELY((lhs[3] | rhs[3]) == 0 && (lhs[2] | rhs[2] | lhs[1] | rhs[1]) == 0))
    {
        mul_limbs4_u64(res, lhs[0], rhs[0]);
        return;
    }
#endif
#if GINT_ENABLE_MUL4_RHS_SINGLE_LIMB_FASTPATH || GINT_ENABLE_MUL4_LHS_SINGLE_LIMB_FASTPATH || GINT_ENABLE_MUL4_LOW128_FASTPATH
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
mul_limbs_result(uint64_t * GINT_RESTRICT res, const uint64_t * GINT_RESTRICT lhs, const uint64_t * GINT_RESTRICT rhs) noexcept
{
#if GINT_ENABLE_WIDE_SINGLE_LIMB_MUL_FASTPATH
    if (L > 4 && GINT_UNLIKELY(lhs[L - 1] == 0 || rhs[L - 1] == 0) && mul_try_single_limb_operand<L>(res, lhs, rhs))
        return;
#endif
    for (size_t i = 0; i < L; ++i)
        res[i] = 0;
    mul_limbs<L>(res, lhs, rhs);
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
#ifdef GINT_TEST_ACCESS
    friend struct detail::integer_test_access<Bits, Signed>;
#endif

private:
    struct uninitialized_tag
    {
    };
    explicit integer(uninitialized_tag) noexcept { }

public:
    // Constructors
    constexpr integer() noexcept
        : data_{}
    {
    }
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
    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator<<=(int n) noexcept
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

    GINT_CONSTEXPR14 GINT_FORCE_INLINE integer & operator>>=(int n) noexcept
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

private:
    template <size_t L = limbs>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE typename std::enable_if<(L == 2 && std::is_same<Signed, signed>::value), integer>::type
    shift_right_int128_unsigned_value(const integer & lhs, unsigned n) noexcept
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

        const bool neg = (lhs.data_[1] >> 63) != 0;
        const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
        integer result(uninitialized_tag{});
        result.data_[0] = fill;
        result.data_[1] = fill;
        return result;
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

#if !GINT_GCC_TUNED_PATHS || GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_value_by_size(const integer & value, size_t shift) noexcept
    {
        const size_t total_bits = limbs * 64;
#    if __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
        if (shift >= total_bits)
        {
            for (size_t i = 0; i < limbs; ++i)
                result.data_[i] = 0;
            return result;
        }

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

#if GINT_ENABLE_AARCH64_INT128_LEFT_SHIFT_FASTPATH
    template <size_t L = limbs, typename std::enable_if<(L == 2), int>::type = 0>
    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_left_int128_unsigned_value(const integer & value, unsigned n) noexcept
    {
        if (n == 0)
            return value;
        if (n >= 128)
            return integer();

        using u128 = unsigned __int128;
        const u128 raw = (static_cast<u128>(value.data_[1]) << 64) | value.data_[0];
        const u128 shifted = raw << n;
        integer result;
        result.data_[0] = static_cast<limb_type>(shifted);
        result.data_[1] = static_cast<limb_type>(shifted >> 64);
        return result;
    }
#endif

#if GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH
    template <size_t LimbShift>
    static GINT_CONSTEXPR14
        GINT_FORCE_INLINE integer shift_right_value_16_by_limb_shift(const integer & value, unsigned bit_shift, limb_type fill) noexcept
    {
#    if __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
        const size_t count = limbs - LimbShift;
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            limb_type prev = value.data_[LimbShift];
            for (size_t i = 0; i + 1 < count; ++i)
            {
                const limb_type cur = value.data_[LimbShift + i + 1];
                result.data_[i] = (prev >> bit_shift) | (cur << inv_shift);
                prev = cur;
            }
            result.data_[count - 1] = (prev >> bit_shift) | (fill << inv_shift);
        }
        else
        {
            for (size_t i = 0; i < count; ++i)
                result.data_[i] = value.data_[i + LimbShift];
        }
        for (size_t i = count; i < limbs; ++i)
            result.data_[i] = fill;
        return result;
    }

    template <size_t L = limbs>
    static GINT_CONSTEXPR14 typename std::enable_if<(L == 16), integer>::type
    shift_right_value_by_size_fixed16(const integer & value, size_t shift) noexcept
    {
        const bool is_signed_t = std::is_same<Signed, signed>::value;
        const bool neg = is_signed_t && (value.data_[limbs - 1] >> 63);
        const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
        const size_t total_bits = limbs * 64;
        if (shift >= total_bits)
        {
#    if __cplusplus >= 201402L
            integer result;
#    else
            integer result(uninitialized_tag{});
#    endif
            for (size_t i = 0; i < limbs; ++i)
                result.data_[i] = fill;
            return result;
        }

        const size_t limb_shift = shift / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        switch (limb_shift)
        {
            case 0:
                return shift_right_value_16_by_limb_shift<0>(value, bit_shift, fill);
            case 1:
                return shift_right_value_16_by_limb_shift<1>(value, bit_shift, fill);
            case 2:
                return shift_right_value_16_by_limb_shift<2>(value, bit_shift, fill);
            case 3:
                return shift_right_value_16_by_limb_shift<3>(value, bit_shift, fill);
            case 4:
                return shift_right_value_16_by_limb_shift<4>(value, bit_shift, fill);
            case 5:
                return shift_right_value_16_by_limb_shift<5>(value, bit_shift, fill);
            case 6:
                return shift_right_value_16_by_limb_shift<6>(value, bit_shift, fill);
            case 7:
                return shift_right_value_16_by_limb_shift<7>(value, bit_shift, fill);
            case 8:
                return shift_right_value_16_by_limb_shift<8>(value, bit_shift, fill);
            case 9:
                return shift_right_value_16_by_limb_shift<9>(value, bit_shift, fill);
            case 10:
                return shift_right_value_16_by_limb_shift<10>(value, bit_shift, fill);
            case 11:
                return shift_right_value_16_by_limb_shift<11>(value, bit_shift, fill);
            case 12:
                return shift_right_value_16_by_limb_shift<12>(value, bit_shift, fill);
            case 13:
                return shift_right_value_16_by_limb_shift<13>(value, bit_shift, fill);
            case 14:
                return shift_right_value_16_by_limb_shift<14>(value, bit_shift, fill);
            default:
                return shift_right_value_16_by_limb_shift<15>(value, bit_shift, fill);
        }
    }

    template <size_t L = limbs>
    static GINT_CONSTEXPR14 typename std::enable_if<(L != 16), integer>::type
    shift_right_value_by_size_fixed16(const integer &, size_t) noexcept
    {
        return integer();
    }

    static GINT_CONSTEXPR14 GINT_FORCE_INLINE integer shift_right_value_by_size(const integer & value, size_t shift) noexcept
    {
        if (limbs == 16)
            return shift_right_value_by_size_fixed16(value, shift);

        const bool is_signed_t = std::is_same<Signed, signed>::value;
        const bool neg = is_signed_t && (value.data_[limbs - 1] >> 63);
        const limb_type fill = neg ? ~limb_type(0) : limb_type(0);
        const size_t total_bits = limbs * 64;
#    if __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
        if (shift >= total_bits)
        {
            for (size_t i = 0; i < limbs; ++i)
                result.data_[i] = fill;
            return result;
        }

        const size_t limb_shift = shift / 64;
        const unsigned bit_shift = static_cast<unsigned>(shift % 64);
        const size_t count = limbs - limb_shift;
        if (bit_shift)
        {
            const unsigned inv_shift = 64U - bit_shift;
            limb_type prev = value.data_[limb_shift];
            for (size_t i = 0; i + 1 < count; ++i)
            {
                const limb_type cur = value.data_[limb_shift + i + 1];
                result.data_[i] = (prev >> bit_shift) | (cur << inv_shift);
                prev = cur;
            }
            result.data_[count - 1] = (prev >> bit_shift) | (fill << inv_shift);
        }
        else
        {
            for (size_t i = 0; i < count; ++i)
                result.data_[i] = value.data_[i + limb_shift];
        }
        for (size_t i = count; i < limbs; ++i)
            result.data_[i] = fill;
        return result;
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
        const size_t total_bits = limbs * 64;
        const size_t shift = static_cast<size_t>(n);
#    if __cplusplus >= 201402L
        integer result;
#    else
        integer result(uninitialized_tag{});
#    endif
        if (shift >= total_bits)
        {
            for (size_t i = 0; i < limbs; ++i)
                result.data_[i] = fill;
            return result;
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
#if GINT_X86_64_GCC_TUNED_PATHS
        if (limbs == 4 && (rhs.data_[1] | rhs.data_[2] | rhs.data_[3]) == 0)
        {
            detail::sub_limbs4_by_limb(result.data_, lhs.data_, rhs.data_[0]);
            return result;
        }
#endif
        detail::sub_limbs_copy<limbs>(result.data_, lhs.data_, rhs.data_);
        return result;
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

#    if GINT_ENABLE_AARCH64_INT128_LEFT_SHIFT_FASTPATH
    template <size_t L = limbs, typename std::enable_if<(L == 2), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator<<(const integer & lhs, unsigned n) noexcept
    {
        return shift_left_int128_unsigned_value(lhs, n);
    }
#    endif

#    if GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH
    template <size_t L = limbs, typename std::enable_if<(L > 8), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator<<(const integer & lhs, unsigned n) noexcept
    {
        const size_t shift = static_cast<size_t>(n);
        return shift_left_value_by_size(lhs, shift);
    }
#    endif

    GINT_CONSTEXPR14 friend integer operator>>(integer lhs, int n) noexcept
    {
        lhs >>= n;
        return lhs;
    }

#    if GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH
    template <size_t L = limbs, typename std::enable_if<(L > 8), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, unsigned n) noexcept
    {
        const size_t shift = static_cast<size_t>(n);
        return shift_right_value_by_size(lhs, shift);
    }
#    endif
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

    template <
        size_t L = limbs,
        typename std::enable_if<
            (GINT_ENABLE_AARCH64_INT128_UNSIGNED_RIGHT_SHIFT_FASTPATH && L == 2 && std::is_same<Signed, signed>::value),
            int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, unsigned n) noexcept
    {
        return shift_right_int128_unsigned_value(lhs, n);
    }

    template <size_t L = limbs, typename std::enable_if<(L > 4), int>::type = 0>
    GINT_CONSTEXPR14 friend integer operator>>(const integer & lhs, int n) noexcept
    {
        return shift_right_value(lhs, n);
    }
#endif

    // Multiplication is left non-constexpr due to helper internals
    friend integer operator*(const integer & lhs, const integer & rhs) noexcept
    {
        integer result(uninitialized_tag{});
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

    friend integer operator/(integer lhs, const integer & rhs)
    {
#if GINT_ENABLE_POSITIVE_LIMB_DIV_FASTPATH
        limb_type positive_limb_divisor;
        if (positive_single_limb_value(rhs, positive_limb_divisor))
        {
            GINT_DIVZERO_CHECK(positive_limb_divisor == 0);
#    if GINT_ARCH_AARCH64 && GINT_GCC_TUNED_PATHS
            if (limbs == 2 && std::is_same<Signed, signed>::value && positive_limb_divisor > 0xFFFFFFFFULL
                && (positive_limb_divisor & (positive_limb_divisor - 1)) == 0)
                return div_by_positive_power_of_two(lhs, static_cast<int>(__builtin_ctzll(positive_limb_divisor)));
#    endif
            return div_by_positive_limb(lhs, positive_limb_divisor);
        }
#elif GINT_ENABLE_AARCH64_LIMB2_POSITIVE_LIMB_DIV_FASTPATH
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
#if GINT_ENABLE_AARCH64_INT128_NEGATIVE_ZERO_DIV_FASTPATH
            if (lhs_neg && rhs_neg && negative_negative_div_quotient_is_zero(lhs, divisor))
                return integer();
#endif
            const limb_type min_magnitude = static_cast<limb_type>(1ULL << 63);
            // 快路径：仅当最高 limb 匹配补码最小值时再调用 is_min_value，避免普通负数走慢分支。
            if (lhs_neg)
            {
                if (GINT_UNLIKELY(lhs.data_[limbs - 1] == min_magnitude && is_min_value(lhs)))
                    lhs_is_min = true;
                else
                    negate_for_division(lhs);
            }
            // 同理对除数执行一次最高 limb 检查，保持正负号拆分与检测逻辑一致。
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
        if (GINT_UNLIKELY(rhs == 0))
            return integer();
        // For unsigned integers, mimic native casts: reinterpret negative divisors as their two's complement magnitude.
        if (std::is_same<Signed, unsigned>::value && rhs < 0)
            return lhs / integer(rhs);
        integer q;
        lhs.div_mod_small(rhs, q);
        return q;
    }

    friend integer operator/(limb_type lhs, integer rhs) { return integer(lhs) / rhs; }

    friend integer operator%(integer lhs, const integer & rhs)
    {
        GINT_MODZERO_CHECK(rhs.is_zero());
#if GINT_ARCH_AARCH64 && (GINT_CLANG_TUNED_PATHS || GINT_ENABLE_AARCH64_UINT128_SMALL_MOD)
        if (limbs == 2)
        {
            limb_type positive_limb_divisor;
            if (positive_single_limb_value(rhs, positive_limb_divisor))
            {
                integer result;
                if (std::is_same<Signed, signed>::value && (lhs.data_[1] >> 63))
                {
#    if GINT_ENABLE_AARCH64_UINT128_SMALL_MOD
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

#    if GINT_ENABLE_AARCH64_UINT128_SMALL_MOD
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
#if GINT_ENABLE_POSITIVE_LIMB_REM_FASTPATH
        if (!(limbs == 2 && GINT_ARCH_AARCH64 && (GINT_CLANG_TUNED_PATHS || GINT_ENABLE_AARCH64_UINT128_SMALL_MOD)))
        {
            limb_type positive_limb_divisor;
            if (positive_single_limb_value(rhs, positive_limb_divisor))
                return rem_by_positive_limb(lhs, positive_limb_divisor);
        }
#endif
#if GINT_GCC_TUNED_PATHS
        if (std::is_same<Signed, signed>::value)
        {
            const bool lhs_neg = lhs.data_[limbs - 1] >> 63;
            const bool rhs_neg = rhs.data_[limbs - 1] >> 63;
#    if GINT_X86_64_GCC_TUNED_PATHS
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
        integer q = lhs / rhs;
        q *= rhs;
        lhs -= q;
        return lhs;
#endif
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
        if (GINT_UNLIKELY(rhs == 0))
            return lhs;
        // For unsigned integers, mimic native casts: reinterpret negative divisors as their two's complement magnitude.
        if (std::is_same<Signed, unsigned>::value && rhs < 0)
            return lhs % integer(rhs);
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
    } // LCOV_EXCL_LINE

    template <typename T, typename std::enable_if<detail::is_integral<T>::value, int>::type = 0>
    friend integer operator%(integer lhs, T rhs)
    {
        GINT_MODZERO_CHECK(rhs == 0);
        if (GINT_UNLIKELY(rhs == 0))
            return lhs;
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
    } // LCOV_EXCL_LINE

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
        typedef __int128 wide_signed;
        typedef unsigned __int128 wide_unsigned;
        const bool sign_fill = detail::is_signed<T>::value && v < 0;
        const limb_type fill = sign_fill ? ~limb_type(0) : limb_type(0);
        wide_unsigned val = sign_fill ? static_cast<wide_unsigned>(static_cast<wide_signed>(v)) : static_cast<wide_unsigned>(v);

        if (limbs > 0)
        {
            data_[0] = static_cast<limb_type>(val);
            val >>= 64;
        }
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
#if GINT_ENABLE_AARCH64_UINT128_SMALL_DIVMOD
        if (limbs == 2 && div > 0xFFFFFFFFULL)
        {
            const u128 num = (static_cast<u128>(data_[1]) << 64) | data_[0];
            const u128 q = num / div;
            quotient.data_[0] = static_cast<limb_type>(q);
            quotient.data_[1] = static_cast<limb_type>(q >> 64);
            return static_cast<limb_type>(num % div);
        }
#endif
#if GINT_ENABLE_X86_64_HW_SMALL_DIVMOD
#    if GINT_CLANG_TUNED_PATHS
        if (div != 10000000000000000000ULL)
#    endif
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

#if GINT_ENABLE_AARCH64_UINT128_SMALL_DIVMOD || GINT_ENABLE_AARCH64_UINT128_SMALL_MOD
        if (limbs == 2 && div > 0xFFFFFFFFULL)
        {
            const u128 num = (static_cast<u128>(data_[1]) << 64) | data_[0];
            return static_cast<limb_type>(num % div);
        }
#endif

#if GINT_ENABLE_X86_64_HW_SMALL_DIVMOD
        {
            u128 rem = 0;
            for (size_t i = n; i-- > 0;)
                rem = ((rem << 64) | data_[i]) % div;
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
            u128 q = detail::mulhi_u128(num, inv);
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

    static integer
    div_unsigned_path(const integer & lhs_value, const integer & rhs_value, bool lhs_neg, bool rhs_neg, bool lhs_is_min, bool rhs_is_min)
    {
        using Unsigned = integer<Bits, unsigned>;
        Unsigned lhs_mag;
        Unsigned divisor_mag;

        if (lhs_is_min)
        {
            // 直接构造绝对值幅度，避免对补码最小值执行求反导致符号保持为负。
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
            // 同理：除数为最小值时仅复制其幅度以复用无符号除法实现。
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
        size_t divisor_limbs = limbs;
        while (divisor_limbs > 0 && divisor_mag.data_[divisor_limbs - 1] == 0)
            --divisor_limbs;
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
#if GINT_ENABLE_AARCH64_TWO_LIMB_POSITIVE_POW2_DIV_FASTPATH
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
    static GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR typename std::enable_if<(L == 2), bool>::type
    negative_negative_div_quotient_is_zero(const integer & lhs, const integer & rhs) noexcept
    {
#if GINT_ARCH_AARCH64
        // For two negative two's-complement values, larger raw bits mean smaller magnitude.
        unsigned result;
        __asm__("cmp %[lhs_hi], %[rhs_hi]\n"
                "ccmp %[lhs_lo], %[rhs_lo], #0, eq\n"
                "cset %w[result], hi"
                : [result] "=r"(result)
                : [lhs_hi] "r"(lhs.data_[1]), [rhs_hi] "r"(rhs.data_[1]), [lhs_lo] "r"(lhs.data_[0]), [rhs_lo] "r"(rhs.data_[0])
                : "cc");
        return result != 0;
#else
        return lhs.data_[1] > rhs.data_[1] || (lhs.data_[1] == rhs.data_[1] && lhs.data_[0] > rhs.data_[0]);
#endif
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L != 2), bool>::type
    negative_negative_div_quotient_is_zero(const integer &, const integer &) noexcept
    {
        return false;
    }

    template <size_t L = limbs, typename std::enable_if<!(GINT_ARCH_AARCH64 && GINT_GCC_TUNED_PATHS && L == 2), int>::type = 0>
    static integer div_by_positive_limb(integer lhs, limb_type divisor) noexcept
    {
        integer result;
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
#if GINT_ENABLE_AARCH64_INT128_SIGNED_LIMB_DIV_FASTPATH
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

    template <size_t L = limbs, typename std::enable_if<(GINT_ARCH_AARCH64 && GINT_GCC_TUNED_PATHS && L == 2), int>::type = 0>
    static integer div_by_positive_limb(integer lhs, limb_type divisor) noexcept
    {
        integer result;
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
#if GINT_ENABLE_AARCH64_INT128_SIGNED_LIMB_DIV_FASTPATH
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

        Unsigned rem_mag = Unsigned::rem_unsigned_magnitude(lhs_mag, rhs_mag);
        integer result(uninitialized_tag{});
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = rem_mag.data_[i];
        if (lhs_neg)
            negate_for_division(result);
        return result;
    }

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

        integer quotient;
        if (limbs == 2)
            quotient = div_128(lhs, divisor);
        else if (divisor_limbs == 2)
            quotient = div_large_2(lhs, divisor);
        else if (divisor_limbs == 3)
            quotient = div_large_3(lhs, divisor);
        else if (limbs == 4 && divisor_limbs == 4)
            quotient = div_large_4(lhs, divisor);
        else
            quotient = div_large(lhs, divisor, divisor_limbs);

        result = lhs;
#if GINT_ENABLE_REM_QUOTIENT_MUL_FASTPATH
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
        integer result;
        if (GINT_UNLIKELY((rhs.data_[1] | rhs.data_[0]) == 0))
            return result;
#if GINT_ENABLE_AARCH64_TWO_LIMB_LARGE_DIVISOR_FASTPATH
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
#endif

        unsigned __int128 a = (static_cast<unsigned __int128>(lhs.data_[1]) << 64) | lhs.data_[0];
        unsigned __int128 b = (static_cast<unsigned __int128>(rhs.data_[1]) << 64) | rhs.data_[0];
        unsigned __int128 q = a / b;
        result.data_[0] = static_cast<limb_type>(q);
        result.data_[1] = static_cast<limb_type>(q >> 64);
        return result;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), integer>::type div_128(const integer & lhs, const integer & rhs) noexcept
    {
        integer result;
        if (GINT_UNLIKELY(rhs.data_[0] == 0))
            return result;
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
        if (GINT_UNLIKELY(div_limbs == 0) || n < div_limbs)
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
            if (static_cast<unsigned __int128>(u[j + div_limbs]) < borrow)
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

    // Optimized specialization: full-width 256-bit divisor (divisor_limbs == 4)
    template <size_t L = limbs>
    static GINT_NOINLINE typename std::enable_if<(L == 4), integer>::type div_large_4(integer lhs, const integer & divisor) noexcept
    {
        integer quotient;
        if (lhs.data_[3] == 0)
            return quotient;

        using u128 = unsigned __int128;

        // Normalize divisor so that its top limb has its MSB set.
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

    // Stub for non-256-bit instantiations to keep dependent calls well-formed.
    template <size_t L = limbs>
    static typename std::enable_if<(L != 4), integer>::type div_large_4(integer lhs, const integer & divisor) noexcept
    {
        return div_large(lhs, divisor, 4);
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
                u128 qhat = v1_is_half_base ? (numerator >> 63) : detail::mulhi_u128(numerator, inv128);
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
                u128 qhat = v1_is_half_base ? (numerator >> 63) : detail::mulhi_u128(numerator, inv128);
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

    // Stub for small-limb types to avoid -Warray-bounds during template instantiation
    template <size_t L = limbs>
    static typename std::enable_if<(L < 3), integer>::type div_large_3(integer lhs, const integer & divisor) noexcept
    {
        // Not reachable for L < 3 because divisor_limbs cannot be 3. Keep a safe fallback signature.
        return div_large(lhs, divisor, 3);
    }

    alignas((GINT_ARCH_AARCH64 || Bits == 64) ? alignof(limb_type) : 16) limb_type data_[limbs];
};

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
    static const bool traps = false;
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

#if (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS) || GINT_X86_64_GCC_TUNED_PATHS
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
#else
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
        typename Int::limb_type x = *it++;
        while (x)
        {
            buf[--idx] = static_cast<char>('0' + (x % 10));
            x /= 10;
        }
        out.append(buf + idx, 32 - idx);
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
#endif
    if (neg)
        out.insert(out.begin(), '-');
    return out;
}

template <size_t Bits, typename Signed>
inline std::ostream & operator<<(std::ostream & out, const integer<Bits, Signed> & value)
{
    return out << to_string(value);
} // LCOV_EXCL_LINE

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
    } // LCOV_EXCL_LINE
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
#undef GINT_CLANG_NOINLINE
#undef GINT_NOINLINE
#undef GINT_COLD
#undef GINT_RESTRICT
#undef GINT_HAS_IS_CONSTANT_EVALUATED
#undef GINT_ARCH_AARCH64
#undef GINT_ARCH_X86_64
#undef GINT_X86_64_GCC_TUNED_PATHS
#undef GINT_NON_X86_GCC_TUNED_PATHS
#undef GINT_ENABLE_AARCH64_LIMB_ASM
#undef GINT_ENABLE_AARCH64_UINT128_SMALL_DIVMOD
#undef GINT_ENABLE_AARCH64_UINT128_SMALL_MOD
#undef GINT_ENABLE_AARCH64_LIMB2_POSITIVE_LIMB_DIV_FASTPATH
#undef GINT_ENABLE_AARCH64_WIDE_ADDSUB_BUILTINS
#undef GINT_ENABLE_X86_64_ADD_INTRINSICS
#undef GINT_ENABLE_X86_64_WIDE_ADD_INTRINSICS
#undef GINT_ENABLE_X86_64_SUB_INTRINSICS
#undef GINT_GCC_TUNED_PATHS
#undef GINT_CLANG_TUNED_PATHS
#undef GINT_ENABLE_MUL4_LOW128_FASTPATH
#undef GINT_ENABLE_MUL4_RHS_SINGLE_LIMB_FASTPATH
#undef GINT_ENABLE_MUL4_LHS_SINGLE_LIMB_FASTPATH
#undef GINT_ENABLE_X86_64_GCC_MUL4_U64_FASTPATH
#undef GINT_ENABLE_REM_QUOTIENT_MUL_FASTPATH
#undef GINT_ENABLE_POSITIVE_LIMB_DIV_FASTPATH
#undef GINT_ENABLE_POSITIVE_LIMB_REM_FASTPATH
#undef GINT_ENABLE_X86_64_HW_SMALL_DIVMOD
#undef GINT_ENABLE_AARCH64_TWO_LIMB_LARGE_DIVISOR_FASTPATH
#undef GINT_ENABLE_AARCH64_INT128_NEGATIVE_ZERO_DIV_FASTPATH
#undef GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR
#undef GINT_ENABLE_AARCH64_TWO_LIMB_POSITIVE_POW2_DIV_FASTPATH
#undef GINT_ENABLE_AARCH64_INT128_SIGNED_LIMB_DIV_FASTPATH
#undef GINT_ENABLE_AARCH64_XOR16_UNROLL_FASTPATH
#undef GINT_ENABLE_AARCH64_INT128_LEFT_SHIFT_FASTPATH
#undef GINT_ENABLE_AARCH64_INT128_UNSIGNED_RIGHT_SHIFT_FASTPATH
#undef GINT_ENABLE_AARCH64_GCC_WIDE_SHIFT_UNSIGNED_FASTPATH
#undef GINT_WIDE_SHIFT_INLINE
