#pragma once

#include "integer_division.hpp"

namespace gint
{
inline namespace GINT_DETAIL_CONFIG_NAMESPACE
{

#if __cplusplus < 201703L
template <size_t Bits, typename Signed>
constexpr size_t integer<Bits, Signed>::bits;

template <size_t Bits, typename Signed>
constexpr size_t integer<Bits, Signed>::limbs;
#endif

#ifdef GINT_TEST_ACCESS
namespace detail
{
template <size_t Bits, typename Signed>
struct integer_test_access
{
    using Int = integer<Bits, Signed>;
    using Division = division_ops<Bits, Signed>;
    using limb_type = typename Int::limb_type;

    static limb_type & limb(Int & value, size_t index) noexcept { return value.data_[index]; }

    static const limb_type & limb(const Int & value, size_t index) noexcept { return value.data_[index]; }

    static int highest_bit(const Int & value) noexcept { return value.highest_bit(); }

    static bool is_min_value(const Int & value) noexcept { return Int::is_min_value(value); }

    static Int div_unsigned_path(const Int & lhs, const Int & rhs, bool lhs_neg, bool rhs_neg, bool lhs_is_min, bool rhs_is_min)
    {
        return Division::div_unsigned_path(lhs, rhs, lhs_neg, rhs_neg, lhs_is_min, rhs_is_min);
    }

    static Int div_128(const Int & lhs, const Int & rhs) noexcept { return Division::div_128(lhs, rhs); }

    static Int div_large(Int lhs, const Int & rhs, size_t div_limbs) noexcept { return Division::div_large(lhs, rhs, div_limbs); }

    static Int rem_large(Int lhs, const Int & rhs, size_t div_limbs) noexcept { return Division::rem_large(lhs, rhs, div_limbs); }

    static Int div_large_2(Int lhs, const Int & rhs) noexcept { return Division::div_large_2(lhs, rhs); }

    static Int div_large_3(Int lhs, const Int & rhs) noexcept { return Division::div_large_3(lhs, rhs); }

    static Int div_large_4(Int lhs, const Int & rhs) noexcept { return Division::div_large_4(lhs, rhs); }

    template <typename T>
    static int compare_with_float_abs(const Int & lhs_abs, T rhs_abs) noexcept
    {
        return Int::template compare_with_float_abs<T>(lhs_abs, rhs_abs);
    }
};
} // namespace detail
#endif

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint
