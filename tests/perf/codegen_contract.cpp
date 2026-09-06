#include <gint/gint.h>

#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
#    define GINT_PERF_NOINLINE __attribute__((noinline))
#else
#    error "The code-generation contract supports GCC and Clang only"
#endif

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

GINT_PERF_NOINLINE void gint_perf_sub_u64_zero(gint::UInt256 * out, const gint::UInt256 * lhs)
{
    *out = *lhs - std::uint64_t(0);
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

GINT_PERF_NOINLINE unsigned gint_perf_hex_digit(unsigned char value)
{
    return gint::detail::hexadecimal_digit_value(value);
}

} // extern "C"
