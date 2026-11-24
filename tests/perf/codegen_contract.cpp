#define GINT_TEST_ACCESS
#include <gint/gint.h>
#undef GINT_TEST_ACCESS

#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
#    define GINT_PERF_NOINLINE __attribute__((noinline))
#else
#    error "The code-generation contract supports GCC and Clang only"
#endif

using UInt512 = gint::integer<512, unsigned>;
using UInt1024 = gint::integer<1024, unsigned>;

extern "C" {

GINT_PERF_NOINLINE void gint_perf_add256(gint::UInt256 * out, const gint::UInt256 * lhs, const gint::UInt256 * rhs)
{
    *out = *lhs + *rhs;
}

GINT_PERF_NOINLINE void gint_perf_sub256(gint::UInt256 * out, const gint::UInt256 * lhs, const gint::UInt256 * rhs)
{
    *out = *lhs - *rhs;
}

GINT_PERF_NOINLINE void gint_perf_add_u64(gint::UInt256 * out, const gint::UInt256 * lhs, std::uint64_t rhs)
{
    *out = *lhs + rhs;
}

GINT_PERF_NOINLINE void gint_perf_u64_add(gint::UInt256 * out, std::uint64_t lhs, const gint::UInt256 * rhs)
{
    *out = lhs + *rhs;
}

GINT_PERF_NOINLINE void gint_perf_sub_u64(gint::UInt256 * out, const gint::UInt256 * lhs, std::uint64_t rhs)
{
    *out = *lhs - rhs;
}

GINT_PERF_NOINLINE void gint_perf_mul256(gint::UInt256 * out, const gint::UInt256 * lhs, const gint::UInt256 * rhs)
{
    *out = *lhs * *rhs;
}

GINT_PERF_NOINLINE void gint_perf_mul_u32(gint::UInt256 * out, const gint::UInt256 * lhs, std::uint32_t rhs)
{
    *out = *lhs * rhs;
}

GINT_PERF_NOINLINE void gint_perf_u32_mul(gint::UInt256 * out, std::uint32_t lhs, const gint::UInt256 * rhs)
{
    *out = lhs * *rhs;
}

GINT_PERF_NOINLINE void gint_perf_xor256(gint::UInt256 * out, const gint::UInt256 * lhs, const gint::UInt256 * rhs)
{
    *out = *lhs ^ *rhs;
}

GINT_PERF_NOINLINE void gint_perf_shift_left256(gint::UInt256 * out, const gint::UInt256 * value, unsigned int shift)
{
    *out = *value << shift;
}

GINT_PERF_NOINLINE void gint_perf_shift_right256(gint::UInt256 * out, const gint::UInt256 * value, unsigned int shift)
{
    *out = *value >> shift;
}

GINT_PERF_NOINLINE bool gint_perf_equal256(const gint::UInt256 * lhs, const gint::UInt256 * rhs)
{
    return *lhs == *rhs;
}

GINT_PERF_NOINLINE void gint_perf_div_u32(gint::UInt256 * quotient, const gint::UInt256 * dividend, std::uint32_t divisor)
{
    *quotient = *dividend / divisor;
}

GINT_PERF_NOINLINE std::uint32_t gint_perf_mod_u32(const gint::UInt256 * dividend, std::uint32_t divisor)
{
    return static_cast<std::uint32_t>(*dividend % divisor);
}

GINT_PERF_NOINLINE void gint_perf_div_u64(gint::UInt256 * quotient, const gint::UInt256 * dividend, std::uint64_t divisor)
{
    // Keep the non-power-of-two divisor in the positive signed-limb range.
    divisor = (divisor & 0x7FFFFFFFFFFFFFFFULL) | 0x100000001ULL;
    *quotient = *dividend / divisor;
}

GINT_PERF_NOINLINE std::uint64_t gint_perf_mod_u64(const gint::UInt256 * dividend, std::uint64_t divisor)
{
    divisor = (divisor & 0x7FFFFFFFFFFFFFFFULL) | 0x100000001ULL;
    return static_cast<std::uint64_t>(*dividend % divisor);
}

GINT_PERF_NOINLINE unsigned gint_perf_hex_digit(unsigned char value)
{
    return gint::detail::hexadecimal_digit_value(value);
}

// Inline the public dispatcher here so its proven single-limb divisor prunes the other kernels.
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((flatten))
#endif
GINT_PERF_NOINLINE void
gint_perf_divmod256_u64(gint::divmod_result<gint::UInt256> * result, const gint::UInt256 * dividend, std::uint64_t divisor)
{
    divisor = (divisor & 0x7FFFFFFFFFFFFFFFULL) | 0x100000001ULL;
    const gint::UInt256 wide_divisor(divisor);
    const gint::UInt256 full_width_dividend = *dividend | (gint::UInt256(1) << 255u);
#if defined(__clang__)
    [[clang::always_inline]]
#endif
    {
        *result = gint::divmod(full_width_dividend, wide_divisor);
    }
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((flatten))
#endif
GINT_PERF_NOINLINE void gint_perf_divmod512_u64(gint::divmod_result<UInt512> * result, const UInt512 * dividend, std::uint64_t divisor)
{
    const UInt512 wide_divisor((divisor & 0x7FFFFFFFFFFFFFFFULL) | 0x100000001ULL);
    // Set the operand shape directly. GCC's out-of-line wide shift hides the
    // known top bit from the dispatcher and leaves unrelated kernels reachable.
    UInt512 full_width_dividend = *dividend;
    gint::detail::integer_test_access<512, unsigned>::limb(full_width_dividend, UInt512::limbs - 1) |= UINT64_C(1) << 63;
#if defined(__clang__)
    [[clang::always_inline]]
#endif
    {
        *result = gint::divmod(full_width_dividend, wide_divisor);
    }
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((flatten))
#endif
GINT_PERF_NOINLINE void gint_perf_divmod1024_u64(gint::divmod_result<UInt1024> * result, const UInt1024 * dividend, std::uint64_t divisor)
{
    const UInt1024 wide_divisor((divisor & 0x7FFFFFFFFFFFFFFFULL) | 0x100000001ULL);
    UInt1024 full_width_dividend = *dividend;
    gint::detail::integer_test_access<1024, unsigned>::limb(full_width_dividend, UInt1024::limbs - 1) |= UINT64_C(1) << 63;
#if defined(__clang__)
    [[clang::always_inline]]
#endif
    {
        *result = gint::divmod(full_width_dividend, wide_divisor);
    }
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((flatten))
#endif
GINT_PERF_NOINLINE void
gint_perf_divmod512_dense4(gint::divmod_result<UInt512> * result, const UInt512 * dividend, const gint::UInt256 * divisor)
{
    using Access = gint::detail::integer_test_access<512, unsigned>;
    UInt512 wide_divisor(*divisor);
    Access::limb(wide_divisor, gint::UInt256::limbs - 1) |= (UINT64_C(1) << 63) | 1;
    UInt512 full_width_dividend = *dividend;
    Access::limb(full_width_dividend, UInt512::limbs - 1) |= UINT64_C(1) << 63;
#if defined(__clang__)
    [[clang::always_inline]]
#endif
    {
        *result = gint::divmod(full_width_dividend, wide_divisor);
    }
}

GINT_PERF_NOINLINE void gint_perf_add512(UInt512 * out, const UInt512 * lhs, const UInt512 * rhs)
{
    *out = *lhs + *rhs;
}

GINT_PERF_NOINLINE void gint_perf_sub512(UInt512 * out, const UInt512 * lhs, const UInt512 * rhs)
{
    *out = *lhs - *rhs;
}

GINT_PERF_NOINLINE void gint_perf_div_u64_512(UInt512 * quotient, const UInt512 * dividend, std::uint64_t divisor)
{
    divisor = (divisor & 0x7FFFFFFFFFFFFFFFULL) | 0x100000001ULL;
    *quotient = *dividend / divisor;
}

} // extern "C"
