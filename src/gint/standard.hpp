#pragma once

#include "integer.hpp"

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED

#    if defined(__clang__)
#        pragma clang diagnostic push
#        pragma clang diagnostic ignored "-Wdeprecated-declarations"
#    elif defined(__GNUC__)
#        pragma GCC diagnostic push
#        pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#    endif

namespace std
{
template <size_t Bits, typename Signed>
class numeric_limits<gint::integer<Bits, Signed>>
{
public:
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = std::is_same<Signed, signed>::value;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr std::float_round_style round_style = std::round_toward_zero;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr int digits = static_cast<int>(Bits) - (is_signed ? 1 : 0);
    static constexpr int digits10 = digits * 30103 / 100000;
    static constexpr int max_digits10 = 0;
    static constexpr int radix = 2;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;

    static constexpr gint::integer<Bits, Signed> min() noexcept { return gint::integer<Bits, Signed>::numeric_min(); }

    static constexpr gint::integer<Bits, Signed> max() noexcept { return gint::integer<Bits, Signed>::numeric_max(); }

    static constexpr gint::integer<Bits, Signed> lowest() noexcept { return min(); }
    static constexpr gint::integer<Bits, Signed> epsilon() noexcept { return gint::integer<Bits, Signed>(0); }
    static constexpr gint::integer<Bits, Signed> round_error() noexcept { return gint::integer<Bits, Signed>(0); }
    static constexpr gint::integer<Bits, Signed> infinity() noexcept { return gint::integer<Bits, Signed>(0); }
    static constexpr gint::integer<Bits, Signed> quiet_NaN() noexcept { return gint::integer<Bits, Signed>(0); }
    static constexpr gint::integer<Bits, Signed> signaling_NaN() noexcept { return gint::integer<Bits, Signed>(0); }
    static constexpr gint::integer<Bits, Signed> denorm_min() noexcept { return gint::integer<Bits, Signed>(0); }
};

template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::is_specialized;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::is_signed;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::is_integer;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::is_exact;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::has_infinity;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::has_quiet_NaN;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::has_signaling_NaN;
template <size_t Bits, typename Signed>
constexpr std::float_denorm_style numeric_limits<gint::integer<Bits, Signed>>::has_denorm;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::has_denorm_loss;
template <size_t Bits, typename Signed>
constexpr std::float_round_style numeric_limits<gint::integer<Bits, Signed>>::round_style;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::is_iec559;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::is_bounded;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::is_modulo;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::digits;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::digits10;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::max_digits10;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::radix;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::min_exponent;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::min_exponent10;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::max_exponent;
template <size_t Bits, typename Signed>
constexpr int numeric_limits<gint::integer<Bits, Signed>>::max_exponent10;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::traps;
template <size_t Bits, typename Signed>
constexpr bool numeric_limits<gint::integer<Bits, Signed>>::tinyness_before;

template <size_t Bits, typename Signed>
struct hash<gint::integer<Bits, Signed>>
{
    size_t operator()(const gint::integer<Bits, Signed> & value) const noexcept
    {
        using Int = gint::integer<Bits, Signed>;
        size_t seed = 0;
        for (size_t i = 0; i < Int::limbs; ++i)
        {
            const size_t h = std::hash<typename Int::limb_type>()(value.data_[i]);
            seed ^= h + static_cast<size_t>(0x9e3779b97f4a7c15ULL) + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
} // namespace std

#    if defined(__clang__)
#        pragma clang diagnostic pop
#    elif defined(__GNUC__)
#        pragma GCC diagnostic pop
#    endif

#endif // GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
