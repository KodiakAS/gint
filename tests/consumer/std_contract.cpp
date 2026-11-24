#include <gint/gint.h>

// Repeated inclusion must retain the same complete interface.
#include <gint/gint.h>

#include "header_contract_checks.h"

#include <limits>
#include <utility>

static_assert(__cplusplus >= GINT_EXPECT_CXX_MIN, "unexpected C++ dialect");

namespace
{
using gint::Int128;
using gint::Int256;
using gint::UInt128;
using gint::UInt256;
using Int64 = gint::integer<64, signed>;
using UInt64 = gint::integer<64, unsigned>;

constexpr unsigned __int128 bit64 = static_cast<unsigned __int128>(1) << 64;
constexpr unsigned __int128 bit127 = static_cast<unsigned __int128>(1) << 127;
constexpr UInt128 wide(bit127 | 42u);
static_assert(Int256(Int128(-42)) == Int256(-42), "signed widening must sign-extend");
static_assert(UInt256(UInt128(bit127)) == UInt256(bit127), "unsigned widening must zero-extend");
static_assert(UInt64(wide) == UInt64(42), "narrowing must discard high bits");
static_assert(Int64(UInt128(bit64 | 0xffffffffffffffffULL)) == Int64(-1), "signed narrowing must interpret the remaining sign bit");

constexpr bool assignments_work()
{
    const Int256 original(-42);
    Int256 copied(0);
    copied = original;
    Int256 moved(0);
    moved = std::move(copied);
    UInt256 scalar(0);
    scalar = bit127;
    Int256 widened(0);
    widened = Int128(-42);
    UInt64 narrowed(0);
    narrowed = wide;
    return moved == original && scalar == UInt256(bit127) && widened == original && narrowed == UInt64(42);
}
static_assert(assignments_work(), "copy, move, scalar and cross-width assignment must be constexpr");

static_assert((UInt128(3) << 0u) == UInt128(3) && (UInt128(3) >> 0u) == UInt128(3), "zero shift preserves value");
static_assert((UInt128(3) << 1u) == UInt128(6) && (UInt128(3) >> 1u) == UInt128(1), "one-bit unsigned shifts");
static_assert((UInt128(3) << 127u) == UInt128(bit127) && (UInt128(bit127) >> 127u) == UInt128(1), "shifts at the last bit");
static_assert((UInt128(3) << 128u) == UInt128(0) && (UInt128(bit127) >> 128u) == UInt128(0), "unsigned overshifts");
static_assert((Int128(-3) << 0u) == Int128(-3) && (Int128(-3) >> 0u) == Int128(-3), "signed zero shifts");
static_assert((Int128(-3) << 1u) == Int128(-6) && (Int128(-3) >> 1u) == Int128(-2), "one-bit signed shifts");
static_assert(
    (Int128(1) << 127u) == std::numeric_limits<Int128>::min() && (Int128(-3) >> 127u) == Int128(-1), "signed shifts at the sign bit");
static_assert((Int128(-3) << 128u) == Int128(0) && (Int128(-3) >> 128u) == Int128(-1), "signed overshifts");
static_assert(std::numeric_limits<Int256>::max() > Int256(0), "signed maximum must be positive");

template <typename Lhs, typename Rhs>
constexpr bool comparisons_match(const Lhs & lhs, const Rhs & rhs, int order)
{
    return ((lhs == rhs) == (order == 0)) && ((rhs == lhs) == (order == 0)) && ((lhs != rhs) == (order != 0))
        && ((rhs != lhs) == (order != 0)) && ((lhs < rhs) == (order < 0)) && ((rhs < lhs) == (order > 0)) && ((lhs <= rhs) == (order <= 0))
        && ((rhs <= lhs) == (order >= 0)) && ((lhs > rhs) == (order > 0)) && ((rhs > lhs) == (order < 0)) && ((lhs >= rhs) == (order >= 0))
        && ((rhs >= lhs) == (order <= 0));
}

static_assert(comparisons_match(Int64(-1), 0xffffffffffffffffULL, -1), "signed gint versus unsigned scalar");
static_assert(comparisons_match(UInt64(2), 1, 1), "unsigned gint versus signed scalar");
static_assert(comparisons_match(Int64(0), -static_cast<__int128>(bit64), 1), "preserve signed 128-bit scalar high bits");
static_assert(comparisons_match(UInt64(0), bit64, -1), "preserve unsigned 128-bit scalar high bits");
static_assert(comparisons_match(Int128(-42), -42, 0), "signed 128-bit gint versus signed scalar");
static_assert(comparisons_match(UInt128(bit127), bit127, 0), "unsigned 128-bit gint versus unsigned 128-bit scalar");
static_assert(comparisons_match(Int256(-42), static_cast<__int128>(-42), 0), "signed 256-bit gint versus 128-bit scalar");
static_assert(comparisons_match(UInt256(42), 42u, 0), "unsigned 256-bit gint versus unsigned scalar");
}

int main()
{
    return check_header_contract() && comparisons_match(UInt64(43), 42.5, 1) && comparisons_match(Int128(-42), -42.0, 0)
            && comparisons_match(UInt128(42), 42.5, -1) && comparisons_match(Int256(-42), -42.5, 1)
            && comparisons_match(UInt256(42), 42.0, 0)
        ? 0
        : 1;
}
