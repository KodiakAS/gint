#pragma once

#include "prelude.hpp"

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    define GINT_DETAIL_HEADER_PASS_ACTIVE

#    if defined(__GNUC__) || defined(__clang__)
#        define GINT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#        define GINT_LIKELY(x) __builtin_expect(!!(x), 1)
#        define GINT_FORCE_INLINE inline __attribute__((always_inline))
#    else
#        define GINT_UNLIKELY(x) (x)
#        define GINT_LIKELY(x) (x)
#        define GINT_FORCE_INLINE inline
#    endif

#    if defined(__clang__)
#        define GINT_CLANG_NOINLINE __attribute__((noinline))
#    else
#        define GINT_CLANG_NOINLINE
#    endif

#    if defined(__GNUC__) || defined(__clang__)
#        define GINT_NOINLINE __attribute__((noinline))
#        define GINT_COLD __attribute__((cold))
#        define GINT_HIDDEN_VISIBILITY __attribute__((visibility("hidden")))
#    else
#        define GINT_NOINLINE
#        define GINT_COLD
#        define GINT_HIDDEN_VISIBILITY
#    endif

#    if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#        define GINT_DETAIL_EXCEPTIONS_ENABLED 1
#        define GINT_THROW(exception) throw exception
#    else
#        define GINT_DETAIL_EXCEPTIONS_ENABLED 0
#        define GINT_THROW(exception) ::std::abort()
#    endif

#    if defined(GINT_ENABLE_DIVZERO_CHECKS)
#        define GINT_DETAIL_DIVZERO_CHECKS 1
#        define GINT_ZERO_CHECK(cond, msg) \
            do \
            { \
                if (GINT_UNLIKELY(cond)) \
                    GINT_THROW(std::domain_error(msg)); \
            } while (false)
#    else
#        define GINT_DETAIL_DIVZERO_CHECKS 0
#        define GINT_ZERO_CHECK(cond, msg) \
            do \
            { \
            } while (false)
#    endif
#    define GINT_DIVZERO_CHECK(cond) GINT_ZERO_CHECK(cond, "division by zero")
#    define GINT_MODZERO_CHECK(cond) GINT_ZERO_CHECK(cond, "modulo by zero")

#    if __cplusplus >= 201402L
#        define GINT_CONSTEXPR14 constexpr
#    else
#        define GINT_CONSTEXPR14
#    endif

#    if defined(__GNUC__) || defined(__clang__)
#        define GINT_RESTRICT __restrict
#    else
#        define GINT_RESTRICT
#    endif

#    ifdef __has_builtin
#        define GINT_DETAIL_HAS_BUILTIN(x) __has_builtin(x)
#    else
#        define GINT_DETAIL_HAS_BUILTIN(x) 0
#    endif

#    if GINT_DETAIL_HAS_BUILTIN(__builtin_is_constant_evaluated)
#        define GINT_HAS_IS_CONSTANT_EVALUATED 1
#    else
#        define GINT_HAS_IS_CONSTANT_EVALUATED 0
#    endif

#    if defined(__aarch64__)
#        define GINT_ARCH_AARCH64 1
#    else
#        define GINT_ARCH_AARCH64 0
#    endif

#    if defined(__x86_64__)
#        define GINT_ARCH_X86_64 1
#    else
#        define GINT_ARCH_X86_64 0
#    endif

// Public configuration boundary:
// - User-facing switches are GINT_ENABLE_FMT, GINT_ENABLE_DIVZERO_CHECKS,
//   GINT_GCC_TUNED_PATHS, GINT_CLANG_TUNED_PATHS, and
//   GINT_ENABLE_AARCH64_LIMB_ASM.
// - Fast paths are governed internally by the four platform policies below.
//   Keep one-off compiler defect checks separate from algorithm selection.
#    ifndef GINT_ENABLE_AARCH64_LIMB_ASM
#        if GINT_ARCH_AARCH64
#            define GINT_ENABLE_AARCH64_LIMB_ASM 1
#        else
#            define GINT_ENABLE_AARCH64_LIMB_ASM 0
#        endif
#    endif

// Compiler-tuned path policy:
// - GCC benefits from selected small-number/division/modulo fast paths.
// - Clang benefits from a narrower modulo fast path and otherwise prefers
//   several generic paths that optimize better under AppleClang on AArch64.
#    ifndef GINT_GCC_TUNED_PATHS
#        if defined(__clang__)
#            define GINT_GCC_TUNED_PATHS 0
#        elif defined(__GNUC__)
#            define GINT_GCC_TUNED_PATHS 1
#        else
#            define GINT_GCC_TUNED_PATHS 0
#        endif
#    endif

#    ifndef GINT_CLANG_TUNED_PATHS
#        if defined(__clang__)
#            define GINT_CLANG_TUNED_PATHS 1
#        else
#            define GINT_CLANG_TUNED_PATHS 0
#        endif
#    endif

#    if GINT_GCC_TUNED_PATHS && GINT_CLANG_TUNED_PATHS
#        error "GINT_GCC_TUNED_PATHS and GINT_CLANG_TUNED_PATHS are mutually exclusive"
#    endif

#    define GINT_DETAIL_X86_64_GCC (GINT_ARCH_X86_64 && GINT_GCC_TUNED_PATHS)
#    define GINT_DETAIL_X86_64_CLANG (GINT_ARCH_X86_64 && GINT_CLANG_TUNED_PATHS)
#    define GINT_DETAIL_AARCH64_GCC (GINT_ARCH_AARCH64 && GINT_GCC_TUNED_PATHS)
#    define GINT_DETAIL_AARCH64_CLANG (GINT_ARCH_AARCH64 && GINT_CLANG_TUNED_PATHS)

// GCC before 9 rejects intrinsics inside C++17 constexpr functions even when
// guarded by __builtin_is_constant_evaluated().
#    if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 9 && __cplusplus < 202002L
#        define GINT_DETAIL_X86_64_CONSTEXPR_INTRINSICS_SAFE 0
#    else
#        define GINT_DETAIL_X86_64_CONSTEXPR_INTRINSICS_SAFE 1
#    endif

#    ifndef GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR
#        if GINT_GCC_TUNED_PATHS
#            define GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR GINT_NOINLINE
#        else
#            define GINT_AARCH64_INT128_NEGATIVE_ZERO_DIV_ATTR GINT_FORCE_INLINE
#        endif
#    endif

// Internal fast-path index and validation contract:
// - Add/Sub: x86 carry/borrow intrinsics, AArch64 limb asm, and AArch64
//   addcll/subcll builtins. Recheck Add/CarryChain64 and Sub/BorrowChain64.
// - Mul: Mul4 small-operand, x86/GCC U64xU64, and wide single-limb paths.
//   Recheck Mul/ plus Div/Mod neighbors that consume quotient products.
// - Bitwise: x86 SSE2 xor and AArch64 xor unroll. Recheck Bitwise/Xor when
//   touching constexpr or SIMD gates.
// - Shift: AArch64/GCC unsigned wide shift and signed int128 right shift.
//   Recheck Shift/LeftVariable with Add/Sub neighbors.
// - Div/Mod: positive-limb, int128, two-limb divisor, quotient multiply,
//   and large-direct remainder paths. Recheck Div/ and Mod/ together.
#    if GINT_DETAIL_X86_64_GCC
#        define GINT_WIDE_SHIFT_INLINE inline GINT_NOINLINE GINT_COLD
#        define GINT_WIDE_PARSE_INLINE inline GINT_NOINLINE
#    else
#        define GINT_WIDE_SHIFT_INLINE GINT_FORCE_INLINE
#        define GINT_WIDE_PARSE_INLINE GINT_FORCE_INLINE
#    endif

#    if GINT_GCC_TUNED_PATHS
#        define GINT_DETAIL_GCC_TUNED_POLICY 1
#    else
#        define GINT_DETAIL_GCC_TUNED_POLICY 0
#    endif

#    if GINT_CLANG_TUNED_PATHS
#        define GINT_DETAIL_CLANG_TUNED_POLICY 1
#    else
#        define GINT_DETAIL_CLANG_TUNED_POLICY 0
#    endif

#    if GINT_ENABLE_AARCH64_LIMB_ASM
#        define GINT_DETAIL_AARCH64_ASM_POLICY 1
#    else
#        define GINT_DETAIL_AARCH64_ASM_POLICY 0
#    endif

// Encode every user-selectable code-generation/semantic policy in the ABI.
// The inline namespace is invisible at the source level (`gint::integer` keeps
// working), while differently configured translation units get distinct
// symbols instead of linker-order-dependent COMDAT selection.
#    define GINT_DETAIL_CONFIG_NAMESPACE_I(divzero, gcc_tuned, clang_tuned, aarch64_asm, exceptions) \
        config_d##divzero##_g##gcc_tuned##_c##clang_tuned##_a##aarch64_asm##_e##exceptions
#    define GINT_DETAIL_CONFIG_NAMESPACE_II(divzero, gcc_tuned, clang_tuned, aarch64_asm, exceptions) \
        GINT_DETAIL_CONFIG_NAMESPACE_I(divzero, gcc_tuned, clang_tuned, aarch64_asm, exceptions)
#    define GINT_DETAIL_CONFIG_NAMESPACE \
        GINT_DETAIL_CONFIG_NAMESPACE_II( \
            GINT_DETAIL_DIVZERO_CHECKS, \
            GINT_DETAIL_GCC_TUNED_POLICY, \
            GINT_DETAIL_CLANG_TUNED_POLICY, \
            GINT_DETAIL_AARCH64_ASM_POLICY, \
            GINT_DETAIL_EXCEPTIONS_ENABLED)

#endif // GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
