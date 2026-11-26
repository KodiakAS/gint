#pragma once

#include "integer_type.hpp"

// Division and remainder kernels.

namespace gint
{
inline namespace GINT_DETAIL_CONFIG_NAMESPACE
{

namespace detail
{
template <size_t Bits, typename Signed>
struct division_ops
{
    using Int = integer<Bits, Signed>;
    using u128 = unsigned __int128;
    using limb_type = typename Int::limb_type;
    using signed_limb_type = typename Int::signed_limb_type;
    using Unsigned = integer<Bits, unsigned>;
    using UnsignedOps = division_ops<Bits, unsigned>;
    static constexpr size_t limbs = Int::limbs;

    // Keep the 256-bit unsigned dispatch and its bounded shift in the caller.
    // The single-limb and multi-limb kernels retain their own inline boundaries.
    static GINT_FORCE_INLINE Int div_unsigned_256(const Int & lhs, const Int & rhs)
    {
        const size_t n = Int::used_limbs(rhs);
        if (n <= 1)
        {
            GINT_DIVZERO_CHECK(n == 0);
            return n == 0 ? Int() : div_by_positive_limb(lhs, rhs.data_[0]);
        }
        const limb_type top = rhs.data_[n - 1];
        if ((top & (top - 1)) == 0)
        {
            limb_type lower = 0;
            for (size_t i = 0; i < n - 1; ++i)
                lower |= rhs.data_[i];
            if (lower == 0)
                return Int::shift_right_in_range(lhs, (n - 1) * 64 + __builtin_ctzll(top));
        }
        if (n == 2)
            return div_large_2(lhs, rhs);
        if (n == 3)
            return div_large_3(lhs, rhs);
        return div_large_4(lhs, rhs);
    }

    static Int modulo(const Int & lhs, const Int & rhs)
    {
        if (limbs == 2)
            return div_or_mod_128<true>(lhs, rhs);
        GINT_MODZERO_CHECK(rhs.is_zero());
        if (limbs == 1)
        {
            const limb_type divisor = rhs.data_[0];
            if (GINT_UNLIKELY(divisor == 0))
                return lhs;
            if (std::is_same<Signed, signed>::value)
            {
                const bool negative = lhs.data_[0] >> 63;
                const limb_type magnitude = negative ? -lhs.data_[0] : lhs.data_[0];
                const limb_type remainder = magnitude % ((divisor >> 63) ? -divisor : divisor);
                return Int(negative ? -remainder : remainder);
            }
            return Int(lhs.data_[0] % divisor);
        }
        limb_type positive_limb_divisor;
        if (positive_single_limb_value(rhs, positive_limb_divisor))
            return rem_by_positive_limb(lhs, positive_limb_divisor);

        if (std::is_same<Signed, signed>::value)
        {
            const bool rhs_neg = rhs.data_[limbs - 1] >> 63;
            // Keep the positive 512-bit fast path separate from GCC's signed magnitude path.
            if (GINT_DETAIL_AARCH64_GCC && limbs == 8 && ((lhs.data_[limbs - 1] | rhs.data_[limbs - 1]) >> 63) == 0)
                return rem_unsigned_magnitude_with_large_direct<true>(lhs, rhs);
            if ((GINT_DETAIL_AARCH64_GCC && (limbs == 4 || limbs == 8)) || rhs_neg || (lhs.data_[limbs - 1] >> 63))
                return rem_signed_magnitude(lhs, rhs, rhs_neg);
        }

        return rem_unsigned_magnitude_with_large_direct<std::is_same<Signed, signed>::value>(lhs, rhs);
    }

    static bool positive_single_limb_value(const Int & v, limb_type & value) noexcept
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

    static void copy_abs_magnitude(Unsigned & dst, const Int & src, bool neg) noexcept
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

    static bool is_power_of_two(const Int & v, int & bit_index) noexcept
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

    // With an incoming remainder below div, each reciprocal estimate needs at most one correction.
    static GINT_FORCE_INLINE uint32_t divrem_reciprocal_half(uint32_t word, uint32_t div, uint64_t inv, uint64_t & rem) noexcept
    {
        const uint64_t numerator = (rem << 32) | word;
        uint64_t q = static_cast<uint64_t>((u128(numerator) * inv) >> 64);
        rem = numerator - q * div;
        if (rem >= div)
        {
            ++q;
            rem -= div;
        }
        return static_cast<uint32_t>(q);
    }

    static GINT_FORCE_INLINE u128 divrem_reciprocal(u128 numerator, limb_type div, u128 inv, u128 & rem) noexcept
    {
        u128 q = detail::mulhi_u128_no_middle_wrap(numerator, inv);
        rem = numerator - q * div;
        if (rem >= div)
        {
            ++q;
            rem -= div;
        }
        return q;
    }

    static GINT_FORCE_INLINE limb_type divrem_reciprocal_limb(limb_type word, limb_type div, u128 inv, u128 & rem) noexcept
    {
        return static_cast<limb_type>(divrem_reciprocal((rem << 64) | word, div, inv, rem));
    }

    // Store quotient limbs only for callers that request them.
#if GINT_ARCH_X86_64
    template <bool WantQuotient>
    static GINT_FORCE_INLINE limb_type scan_divq(const Int & self, size_t n, limb_type div, Int * quotient) noexcept
    {
        limb_type rem = 0;
        for (size_t i = n; i-- > 0;)
        {
            const limb_type q = detail::divrem_u128_u64(self.data_[i], div, rem);
            if (WantQuotient)
                quotient->data_[i] = q;
        }
        return rem;
    }
#else
    template <bool WantQuotient>
    static GINT_FORCE_INLINE limb_type scan_reciprocal_half(const Int & self, size_t n, uint32_t div, Int * quotient) noexcept
    {
        const uint64_t inv = ~uint64_t(0) / static_cast<uint64_t>(div);
        uint64_t rem = 0;
        for (size_t i = n; i-- > 0;)
        {
            const limb_type cur = self.data_[i];
            const uint32_t hi = divrem_reciprocal_half(static_cast<uint32_t>(cur >> 32), div, inv, rem);
            const uint32_t lo = divrem_reciprocal_half(static_cast<uint32_t>(cur), div, inv, rem);
            if (WantQuotient)
                quotient->data_[i] = (limb_type(hi) << 32) | lo;
        }
        return static_cast<limb_type>(rem);
    }
#endif

    template <bool WantQuotient>
    static GINT_FORCE_INLINE limb_type scan_reciprocal_limb(const Int & self, size_t n, limb_type div, u128 inv, Int * quotient) noexcept
    {
        u128 rem = 0;
        for (size_t i = n; i-- > 0;)
        {
            const limb_type q = divrem_reciprocal_limb(self.data_[i], div, inv, rem);
            if (WantQuotient)
                quotient->data_[i] = q;
        }
        return static_cast<limb_type>(rem);
    }

    template <size_t L = limbs, limb_type FixedDivisor = 0>
    static typename std::enable_if<(L == 1), limb_type>::type div_mod_small(const Int & self, limb_type div, Int & quotient) noexcept
    {
        if (FixedDivisor != 0)
            div = FixedDivisor;
        // SFINAE provides a dedicated implementation for single-limb integers,
        // avoiding multi-limb code that would trigger -Warray-bounds warnings.
        quotient = Int();
        if (self.data_[0] == 0)
            return 0;
        quotient.data_[0] = static_cast<limb_type>(self.data_[0] / div);
        return static_cast<limb_type>(self.data_[0] % div);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L == 1), limb_type>::type mod_small(const Int & self, limb_type div) noexcept
    {
        return static_cast<limb_type>(self.data_[0] % div);
    }

    template <size_t L = limbs, limb_type FixedDivisor = 0>
    static typename std::enable_if<(L > 1), limb_type>::type div_mod_small(const Int & self, limb_type div, Int & quotient) noexcept
    {
        if (FixedDivisor != 0)
            div = FixedDivisor;
        // This overload is only instantiated for multi-limb integers, preventing
        // compilers from inspecting out-of-bounds accesses in single-limb cases.
        quotient = Int();
        const size_t n = Int::used_limbs(self);
        if (n == 0)
            return 0;
        // Callers handle zero divisors. Powers of two use a shift and mask.
        if ((div & (div - 1)) == 0)
        {
            const unsigned s = static_cast<unsigned>(__builtin_ctzll(div));
            if (s == 0)
            {
                quotient = self;
            }
            else
            {
                const limb_type mask = (limb_type(1) << s) - 1;
                limb_type carry = 0;
                for (size_t i = limbs; i-- > 0;)
                {
                    const limb_type cur = self.data_[i];
                    quotient.data_[i] = (cur >> s) | (carry << (64 - s));
                    carry = cur & mask;
                }
            }
            return static_cast<limb_type>(self.data_[0] & (div - 1));
        }
        if (limbs == 2)
        {
            const u128 num = detail::load_u128(self.data_);
            const u128 q = num / div;
            detail::store_u128(quotient.data_, q);
            return static_cast<limb_type>(num - q * div);
        }
#if GINT_ARCH_X86_64
        // Unknown single-limb divisors use hardware divq. Fixed divisors such
        // as 10^19 use a compile-time reciprocal, including at 1024 bits.
        if (FixedDivisor == 0)
            return scan_divq<true>(self, n, div, &quotient);
#else
        // Use a reciprocal estimate in base 2^32 for a 32-bit divisor.
        if (div <= 0xFFFFFFFFULL)
            return scan_reciprocal_half<true>(self, n, static_cast<uint32_t>(div), &quotient);
#endif
        // Use a reciprocal estimate with one correction for 64-bit divisors.
        const u128 inv = ~u128(0) / static_cast<u128>(FixedDivisor != 0 ? FixedDivisor : div);
        // Unroll for common 256-bit case (4 limbs) to reduce loop overhead
        if (limbs == 4)
        {
            switch (n)
            {
                case 1: {
                    u128 rem;
                    const u128 q = divrem_reciprocal(self.data_[0], div, inv, rem);
                    quotient.data_[0] = static_cast<limb_type>(q);
                    return static_cast<limb_type>(rem);
                }
                case 2: {
                    u128 rem;
                    const u128 q = divrem_reciprocal((static_cast<u128>(self.data_[1]) << 64) | self.data_[0], div, inv, rem);
                    quotient.data_[0] = static_cast<limb_type>(q);
                    quotient.data_[1] = static_cast<limb_type>(q >> 64);
                    return static_cast<limb_type>(rem);
                }
                case 3: {
                    u128 rem = 0;
                    quotient.data_[2] = divrem_reciprocal_limb(self.data_[2], div, inv, rem);
                    quotient.data_[1] = divrem_reciprocal_limb(self.data_[1], div, inv, rem);
                    quotient.data_[0] = divrem_reciprocal_limb(self.data_[0], div, inv, rem);
                    return static_cast<limb_type>(rem);
                }
                case 4:
                default: {
                    u128 rem = 0;
                    quotient.data_[3] = divrem_reciprocal_limb(self.data_[3], div, inv, rem);
                    quotient.data_[2] = divrem_reciprocal_limb(self.data_[2], div, inv, rem);
                    quotient.data_[1] = divrem_reciprocal_limb(self.data_[1], div, inv, rem);
                    quotient.data_[0] = divrem_reciprocal_limb(self.data_[0], div, inv, rem);
                    return static_cast<limb_type>(rem);
                }
            }
        }
        return scan_reciprocal_limb<true>(self, n, div, inv, &quotient);
    }

    template <limb_type Divisor>
    static limb_type div_mod_small_constant(const Int & self, Int & quotient) noexcept
    {
        return div_mod_small<limbs, Divisor>(self, Divisor, quotient);
    }

    template <size_t L = limbs, typename Divisor>
    static typename std::enable_if<(L > 1), limb_type>::type mod_small(const Int & self, Divisor div) noexcept
    {
        const size_t n = Int::used_limbs(self);
        if (n == 0)
            return 0;

        if ((div & (div - 1)) == 0)
            return static_cast<limb_type>(self.data_[0] & (div - 1));

        if (limbs == 2)
            return static_cast<limb_type>(detail::load_u128(self.data_) % div);

#if GINT_ARCH_X86_64
        return scan_divq<false>(self, n, div, nullptr);
#else
        if (sizeof(Divisor) <= 4 || div <= 0xFFFFFFFFULL)
            return scan_reciprocal_half<false>(self, n, static_cast<uint32_t>(div), nullptr);

        const u128 inv = ~u128(0) / static_cast<u128>(div);
        return scan_reciprocal_limb<false>(self, n, div, inv, nullptr);
#endif
    }

    static signed_limb_type div_mod_small(const Int & self, signed_limb_type div, Int & quotient) noexcept
    {
        Int tmp = self;
        bool lhs_neg = false;
        if (std::is_same<Signed, signed>::value && (tmp.data_[limbs - 1] >> 63))
        {
            Int::negate_in_place(tmp);
            lhs_neg = true;
        }
        bool div_neg = div < 0;
        limb_type abs_div = static_cast<limb_type>(div);
        if (div_neg)
            abs_div = limb_type(0) - abs_div;
        signed_limb_type rem = static_cast<signed_limb_type>(div_mod_small(tmp, abs_div, quotient));
        if (lhs_neg)
            rem = -rem;
        if (lhs_neg != div_neg)
            Int::negate_in_place(quotient);
        return rem;
    }

    static GINT_HIDDEN_VISIBILITY Int
    div_unsigned_path(const Int & lhs_value, const Int & rhs_value, bool lhs_neg, bool rhs_neg, bool lhs_is_min, bool rhs_is_min)
    {
        Unsigned lhs_mag;
        Unsigned divisor_mag;

        if (lhs_is_min)
        {
            // Build the absolute magnitude directly; negating two's-complement min would keep the sign bit set.
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
            // Copy the min-value magnitude directly so the unsigned division path can be reused.
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
        size_t divisor_limbs = Unsigned::used_limbs(divisor_mag);
        GINT_DIVZERO_CHECK(divisor_limbs == 0);
        if (divisor_limbs == 1)
        {
            UnsignedOps::div_mod_small(lhs_mag, divisor_mag.data_[0], quotient_mag);
        }
        else
        {
            int pow_bit;
            if (UnsignedOps::is_power_of_two(divisor_mag, pow_bit))
            {
                quotient_mag = lhs_mag >> pow_bit;
            }
            else if (limbs == 2)
            {
                quotient_mag = UnsignedOps::div_128(lhs_mag, divisor_mag);
            }
            else if (divisor_limbs == 2)
            {
                quotient_mag = UnsignedOps::div_large_2(lhs_mag, divisor_mag);
            }
            else if (divisor_limbs == 3)
            {
                quotient_mag = UnsignedOps::div_large_3(lhs_mag, divisor_mag);
            }
            else
            {
                quotient_mag = UnsignedOps::div_large(lhs_mag, divisor_mag, divisor_limbs);
            }
        }

        Int result;
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = quotient_mag.data_[i];

        if (lhs_neg != rhs_neg)
            Int::negate_in_place(result);
        return result;
    } // LCOV_EXCL_LINE

    template <bool WantRemainder>
    static Int divmod_int64_int128(__int128 lhs, __int128 rhs)
    {
        const signed_limb_type a = static_cast<signed_limb_type>(lhs);
        const signed_limb_type b = static_cast<signed_limb_type>(rhs);
        // For a zero divisor or -1, the low word of lhs determines the fixed-width result.
        // Handling both here keeps the native 128-bit operations below defined at INT128_MIN.
        if ((lhs == a && rhs == b) || rhs == 0 || rhs == -1)
            return WantRemainder ? Int(a) % b : Int(a) / b;
        return WantRemainder ? Int(lhs % rhs) : Int(lhs / rhs);
    }

    static bool positive_power_of_two_value(const Int & v, int & bit_index) noexcept
    {
        if (std::is_same<Signed, signed>::value && (v.data_[limbs - 1] >> 63))
            return false;
        return is_power_of_two(v, bit_index);
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L >= 16), bool>::type
    positive_power_of_two_fastpath_divisor(const Int & v, int & bit_index) noexcept
    {
        return v.data_[0] == 0 && positive_power_of_two_value(v, bit_index);
    }

    template <size_t L = limbs>
    static GINT_FORCE_INLINE typename std::enable_if<(L < 16), bool>::type
    positive_power_of_two_fastpath_divisor(const Int &, int &) noexcept
    {
        return false;
    }

    static GINT_FORCE_INLINE Int div_by_positive_power_of_two(Int lhs, int pow_bit) noexcept
    {
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
            const limb_type min_magnitude = static_cast<limb_type>(1ULL << 63);
            if (GINT_UNLIKELY(lhs.data_[limbs - 1] == min_magnitude && Int::is_min_value(lhs)))
                return div_unsigned_path(lhs, Int(1) << pow_bit, true, false, true, false);
            Int::negate_in_place(lhs);
            Int result = lhs >> pow_bit;
            Int::negate_in_place(result);
            return result;
        }
        return lhs >> pow_bit;
    }

    static Int div_by_positive_limb(Int lhs, limb_type divisor) noexcept
    {
        Int result;
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
            Int::negate_in_place(lhs);
            div_mod_small(lhs, divisor, result);
            Int::negate_in_place(result);
            return result;
        }

        div_mod_small(lhs, divisor, result);
        return result;
    }

    template <typename Divisor>
    static Int rem_by_positive_limb(const Int & lhs, Divisor divisor) noexcept
    {
        if (std::is_same<Signed, signed>::value && (lhs.data_[limbs - 1] >> 63))
        {
            Unsigned lhs_mag(typename Unsigned::uninitialized_tag{});
            copy_abs_magnitude(lhs_mag, lhs, true);
            Int result(UnsignedOps::mod_small(lhs_mag, divisor));
            Int::negate_in_place(result);
            return result;
        }

        return Int(mod_small(lhs, divisor));
    }

    static GINT_FORCE_INLINE Int rem_signed_magnitude(const Int & lhs, const Int & rhs, bool rhs_neg) noexcept
    {
        const bool lhs_neg = lhs.data_[limbs - 1] >> 63;
        Unsigned lhs_mag(typename Unsigned::uninitialized_tag{});
        Unsigned rhs_mag(typename Unsigned::uninitialized_tag{});
        copy_abs_magnitude(lhs_mag, lhs, lhs_neg);
        copy_abs_magnitude(rhs_mag, rhs, rhs_neg);

        // x86-64 GCC favors quotient reconstruction for 1024-bit signed magnitudes.
        const Unsigned rem_mag = GINT_DETAIL_X86_64_GCC && limbs == 16
            ? UnsignedOps::rem_unsigned_magnitude(lhs_mag, rhs_mag)
            : UnsignedOps::template rem_unsigned_magnitude_with_large_direct<true>(lhs_mag, rhs_mag);
        Int result(typename Int::uninitialized_tag{});
        for (size_t i = 0; i < limbs; ++i)
            result.data_[i] = rem_mag.data_[i];
        if (lhs_neg)
            Int::negate_in_place(result);
        return result;
    }

    static Int rem_unsigned_magnitude(const Int & lhs, const Int & divisor) noexcept
    {
        Int result(typename Int::uninitialized_tag{});
        size_t divisor_limbs = Int::used_limbs(divisor);

        if (divisor_limbs == 1)
        {
            for (size_t i = 1; i < limbs; ++i)
                result.data_[i] = 0;
            result.data_[0] = mod_small(lhs, divisor.data_[0]);
            return result;
        }

        int pow_bit;
        if (is_power_of_two(divisor, pow_bit))
        {
            result = lhs & (divisor - Int(1));
            return result;
        }

        if (limbs == 4 && divisor_limbs == 4)
            return rem_large_4(lhs, divisor);

        Int quotient;
        if (divisor_limbs == 2)
            quotient = div_large_2(lhs, divisor);
        else if (divisor_limbs == 3)
            quotient = div_large_3(lhs, divisor);
        else
            quotient = div_large(lhs, divisor, divisor_limbs);

        result = lhs;
        quotient *= divisor;
        result -= quotient;
        return result;
    }

    // Keep separate instantiations for signed magnitudes and regular unsigned modulo.
    // Sharing one instantiation regresses GCC code generation at 1024 bits.
    template <bool SignedOperands>
    static Int rem_unsigned_magnitude_with_large_direct(const Int & lhs, const Int & divisor) noexcept
    {
        constexpr size_t next_high = limbs >= 2 ? limbs - 2 : 0;
        if ((limbs >= 8 || (GINT_DETAIL_AARCH64_GCC && limbs == 4))
            && GINT_UNLIKELY((divisor.data_[limbs - 1] | divisor.data_[next_high]) != 0))
        {
            const size_t divisor_limbs = Int::used_limbs(divisor);
            int pow_bit;
            if (is_power_of_two(divisor, pow_bit))
                return lhs & (divisor - Int(1));
            return rem_large(lhs, divisor, divisor_limbs);
        }
        return rem_unsigned_magnitude(lhs, divisor);
    }

    // Left-shift an array of limbs by 'shift' bits (0..63) into dst, returning the carry-out limb.
    // The source and destination may alias; the operation proceeds from low to high.
    template <bool CopyUnshifted = true>
    static GINT_FORCE_INLINE limb_type lshift_limbs_to(const limb_type * src, size_t n, limb_type * dst, int shift) noexcept
    {
        if (CopyUnshifted && !shift)
        {
            for (size_t i = 0; i < n; ++i)
                dst[i] = src[i];
            return 0;
        }

        limb_type carry = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const limb_type cur = src[i];
            dst[i] = (cur << shift) | carry;
            carry = shift ? static_cast<limb_type>(cur >> (64 - shift)) : 0;
        }
        return carry;
    }

    enum class div128_output
    {
        quotient,
        remainder,
        both
    };

    template <div128_output Output>
    static GINT_FORCE_INLINE u128
    div_or_mod_128_two_limbs_kernel(u128 dividend, u128 divisor, limb_type high, u128 & remainder_out) noexcept
    {
        if (dividend < divisor)
        {
            if (Output == div128_output::both)
                remainder_out = dividend;
            return Output == div128_output::remainder ? dividend : 0;
        }
        const int shift = __builtin_clzll(high);
        const limb_type normalized_high = static_cast<limb_type>((divisor << shift) >> 64);
        // Discarding the low word of the normalized divisor rounds it down.
        // The resulting quotient is exact or one too large. Start one below
        // that estimate so its product fits, then correct using the remainder.
#if GINT_ARCH_X86_64
        // The normalized divisor is at least 2^63, above the high word of dividend >> 1.
        const u128 half = dividend >> 1;
        limb_type hi = static_cast<limb_type>(half >> 64);
        const limb_type estimate = detail::divrem_u128_u64(static_cast<limb_type>(half), normalized_high, hi) >> (63 - shift);
#else
        const limb_type estimate = static_cast<limb_type>(((dividend >> 1) / normalized_high) >> (63 - shift));
#endif
        const limb_type lower = estimate - 1;
        const u128 remainder = dividend - lower * divisor;
        const bool adjust = remainder >= divisor;
        if (Output == div128_output::both)
            remainder_out = adjust ? remainder - divisor : remainder;
        return Output == div128_output::remainder ? (adjust ? remainder - divisor : remainder) : lower + adjust;
    }

    template <div128_output Output>
    static GINT_NOINLINE u128 div_or_mod_128_by_two_limbs(u128 dividend, u128 divisor, limb_type high) noexcept
    {
        u128 unused = 0;
        return div_or_mod_128_two_limbs_kernel<Output>(dividend, divisor, high, unused);
    }

    template <div128_output Output>
    static GINT_FORCE_INLINE u128 div_or_mod_128_dispatch(const Int & lhs, const Int & rhs, u128 & remainder)
    {
        constexpr bool WantRemainder = Output == div128_output::remainder;
        constexpr bool WantBoth = Output == div128_output::both;
        const u128 lhs_bits = detail::load_u128(lhs.data_);
        const u128 rhs_bits = detail::load_u128(rhs.data_);
        const bool lhs_neg = std::is_same<Signed, signed>::value && (lhs_bits >> 127);
        const bool rhs_neg = std::is_same<Signed, signed>::value && (rhs_bits >> 127);
        // Raw bits order two negative values by decreasing magnitude.
        if (!WantRemainder && lhs_neg && rhs_neg && lhs_bits > rhs_bits)
        {
            if (WantBoth)
                remainder = lhs_bits;
            return 0;
        }
        // Unsigned negation also gives the magnitude of the signed minimum without overflow.
        const u128 dividend = lhs_neg ? -lhs_bits : lhs_bits;
        const u128 divisor = rhs_neg ? -rhs_bits : rhs_bits;
        const limb_type low = static_cast<limb_type>(divisor);
        const limb_type high = static_cast<limb_type>(divisor >> 64);
        u128 result;
        if (GINT_LIKELY(high == 0 && (low & (low - 1)) != 0))
        {
            result = WantRemainder ? dividend % low : dividend / low;
            if (WantBoth)
                remainder = dividend - result * low;
        }
        else if (high == 0 || (low == 0 && (high & (high - 1)) == 0))
        {
            if (GINT_UNLIKELY(divisor == 0))
            {
                if (WantRemainder)
                {
                    GINT_MODZERO_CHECK(true);
                    return lhs_bits;
                }
                GINT_DIVZERO_CHECK(true);
                if (WantBoth)
                    remainder = lhs_bits;
                return 0;
            }
            if (WantRemainder)
                result = dividend & (divisor - 1);
            else
            {
                const int shift = low ? __builtin_ctzll(low) : 64 + __builtin_ctzll(high);
                result = dividend >> shift;
                if (WantBoth)
                    remainder = dividend & (divisor - 1);
            }
        }
        else if (divisor >> 127)
        {
            const bool quotient = dividend >= divisor;
            result = WantRemainder ? (quotient ? dividend - divisor : dividend) : quotient;
            if (WantBoth)
                remainder = quotient ? dividend - divisor : dividend;
        }
        else if ((divisor >> 126) == 0)
        {
            if (WantBoth)
                result = div_or_mod_128_two_limbs_kernel<Output>(dividend, divisor, high, remainder);
            else
                result = div_or_mod_128_by_two_limbs<Output>(dividend, divisor, high);
        }
        else
        {
            result = WantRemainder ? dividend % divisor : div_128_nonzero(dividend, divisor);
            if (WantBoth)
                remainder = dividend - result * divisor;
        }

        if (WantBoth && lhs_neg)
            remainder = -remainder;
        const bool negative = WantRemainder ? lhs_neg : lhs_neg != rhs_neg;
        return negative ? -result : result;
    }

    template <bool WantRemainder>
    static GINT_FORCE_INLINE Int div_or_mod_128(const Int & lhs, const Int & rhs)
    {
        u128 unused = 0;
        constexpr div128_output Output = WantRemainder ? div128_output::remainder : div128_output::quotient;
        return Int(div_or_mod_128_dispatch<Output>(lhs, rhs, unused));
    }

    static GINT_FORCE_INLINE divmod_result<Int> divmod_128(const Int & lhs, const Int & rhs)
    {
        u128 remainder = 0;
        const u128 quotient = div_or_mod_128_dispatch<div128_output::both>(lhs, rhs, remainder);
        return {Int(quotient), Int(remainder)};
    }

    static GINT_FORCE_INLINE unsigned __int128 div_128_nonzero(unsigned __int128 dividend, unsigned __int128 divisor) noexcept
    {
#if GINT_DETAIL_AARCH64_GCC || GINT_DETAIL_AARCH64_CLANG
        // A divisor with either of its top two bits set gives a quotient of at most three.
        if (divisor >> 126)
        {
            limb_type quotient = 0;
            for (; quotient < 3 && dividend >= divisor; ++quotient)
                dividend -= divisor;
            return quotient;
        }
#endif
        return dividend / divisor;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L >= 2), Int>::type div_128(const Int & lhs, const Int & rhs) noexcept
    {
        Int result;
        const auto divisor = detail::load_u128(rhs.data_);
        if (GINT_LIKELY(divisor != 0))
            detail::store_u128(result.data_, div_128_nonzero(detail::load_u128(lhs.data_), divisor));
        return result;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L < 2), Int>::type div_128(const Int & lhs, const Int & rhs) noexcept
    {
        Int result;
        if (GINT_UNLIKELY(rhs.data_[0] == 0))
            return result;
        result.data_[0] = lhs.data_[0] / rhs.data_[0];
        return result;
    }

    static GINT_FORCE_INLINE limb_type left_shifted_limb_at(const limb_type * src, size_t i, int shift) noexcept
    {
        limb_type cur = src[i];
        if (shift == 0)
            return cur;
        limb_type prev = i == 0 ? 0 : src[i - 1];
        return static_cast<limb_type>((cur << shift) | (prev >> (64 - shift)));
    }

    static bool mul_by_limb_greater_than(const Int & lhs, const Int & divisor, size_t div_limbs, limb_type q) noexcept
    {
        if (q == 0)
            return false;

        std::array<limb_type, limbs + 1> product;
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor.data_[i]) * q + carry;
            product[i] = static_cast<limb_type>(p);
            carry = p >> 64;
        }
        if (carry != 0)
            return true;

        for (size_t i = div_limbs; i-- > 0;)
        {
            if (product[i] != lhs.data_[i])
                return product[i] > lhs.data_[i];
        }
        return false;
    }

    // Estimate a one-limb quotient for operands with the same used length of at least two limbs.
    static GINT_FORCE_INLINE limb_type estimate_single_limb_quotient(const Int & lhs, const Int & divisor, size_t div_limbs) noexcept
    {
        const int shift = __builtin_clzll(divisor.data_[div_limbs - 1]);
        const limb_type vtop = left_shifted_limb_at(divisor.data_, div_limbs - 1, shift);
        const limb_type vnext = left_shifted_limb_at(divisor.data_, div_limbs - 2, shift);
        const limb_type utop = shift ? static_cast<limb_type>(lhs.data_[div_limbs - 1] >> (64 - shift)) : 0;
        const limb_type unext = left_shifted_limb_at(lhs.data_, div_limbs - 1, shift);
        const limb_type uthird = left_shifted_limb_at(lhs.data_, div_limbs - 2, shift);

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

    static Int div_large_single_limb_quotient(const Int & lhs, const Int & divisor, size_t div_limbs) noexcept
    {
        Int quotient;
        if (div_limbs < 2)
            return quotient;

        limb_type q = estimate_single_limb_quotient(lhs, divisor, div_limbs);
        while (mul_by_limb_greater_than(lhs, divisor, div_limbs, q))
            --q;
        quotient.data_[0] = q;
        return quotient;
    }

    enum class div_large_mode
    {
        quotient_only,
        remainder_only,
        quotient_and_remainder,
    };

    template <div_large_mode Mode>
    static GINT_FORCE_INLINE Int
    div_large_core(const Int & lhs, const Int & divisor, size_t v_limbs, size_t u_limbs, Int * remainder) noexcept
    {
        Int result;
        if (GINT_UNLIKELY(v_limbs == 0) || u_limbs < v_limbs)
        {
            if (Mode == div_large_mode::quotient_and_remainder)
                *remainder = lhs;
            return Mode == div_large_mode::remainder_only ? lhs : result;
        }

        std::array<limb_type, limbs + 1> u;
        std::array<limb_type, limbs + 1> v;

        int shift = __builtin_clzll(divisor.data_[v_limbs - 1]);
        limb_type carry = lshift_limbs_to(lhs.data_, u_limbs, u.data(), shift);
        u[u_limbs] = carry;

        carry = lshift_limbs_to(divisor.data_, v_limbs, v.data(), shift);

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
            if (Mode != div_large_mode::remainder_only)
                result.data_[j] = static_cast<limb_type>(qhat);
        }

        if (Mode != div_large_mode::quotient_only)
        {
            Int & output = Mode == div_large_mode::remainder_only ? result : *remainder;
            if (shift == 0)
            {
                for (size_t i = 0; i < v_limbs; ++i)
                    output.data_[i] = u[i];
            }
            else
            {
                const int inv_shift = 64 - shift;
                for (size_t i = 0; i < v_limbs; ++i)
                {
                    const limb_type next = (i + 1 < v_limbs) ? u[i + 1] : 0;
                    output.data_[i] = (u[i] >> shift) | (next << inv_shift);
                }
            }
            if (Mode == div_large_mode::quotient_and_remainder)
            {
                for (size_t i = v_limbs; i < limbs; ++i)
                    output.data_[i] = 0;
            }
        }
        return result;
    }

    template <bool WantRemainder>
    static GINT_NOINLINE Int div_or_rem_large_core(Int lhs, const Int & divisor, size_t v_limbs, size_t u_limbs) noexcept
    {
        constexpr div_large_mode mode = WantRemainder ? div_large_mode::remainder_only : div_large_mode::quotient_only;
        return div_large_core<mode>(lhs, divisor, v_limbs, u_limbs, nullptr);
    }

    static GINT_NOINLINE Int divmod_large_core(Int lhs, const Int & divisor, size_t v_limbs, size_t u_limbs, Int & remainder) noexcept
    {
        return div_large_core<div_large_mode::quotient_and_remainder>(lhs, divisor, v_limbs, u_limbs, &remainder);
    }

    static Int div_large(Int lhs, const Int & divisor, size_t div_limbs) noexcept
    {
        const size_t dividend_limbs = Int::used_limbs(lhs);
#if !GINT_DETAIL_X86_64_CLANG
        if (dividend_limbs == div_limbs && div_limbs >= 2)
            return div_large_single_limb_quotient(lhs, divisor, div_limbs);
#endif
        return div_or_rem_large_core<false>(lhs, divisor, div_limbs, dividend_limbs);
    }

    static GINT_FORCE_INLINE bool rem_sub_mul_limb(Int & lhs, const Int & divisor, size_t div_limbs, limb_type q) noexcept
    {
        unsigned __int128 carry = 0;
        limb_type borrow = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 p = static_cast<unsigned __int128>(divisor.data_[i]) * q + carry;
            carry = p >> 64;

            unsigned __int128 subtrahend = static_cast<unsigned __int128>(static_cast<limb_type>(p)) + borrow;
            limb_type next_borrow = static_cast<unsigned __int128>(lhs.data_[i]) < subtrahend;
            lhs.data_[i] = static_cast<limb_type>(static_cast<unsigned __int128>(lhs.data_[i]) - subtrahend);
            borrow = next_borrow;
        }
        return carry != 0 || borrow != 0;
    }

    static bool rem_sub_mul_limb_full_width(Int & lhs, const Int & divisor, limb_type q) noexcept
    {
        return rem_sub_mul_limb(lhs, divisor, limbs, q);
    }

    static GINT_FORCE_INLINE void rem_add_divisor(Int & lhs, const Int & divisor, size_t div_limbs) noexcept
    {
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < div_limbs; ++i)
        {
            unsigned __int128 sum = static_cast<unsigned __int128>(lhs.data_[i]) + divisor.data_[i] + carry;
            lhs.data_[i] = static_cast<limb_type>(sum);
            carry = sum >> 64;
        }
    }

    static void rem_add_divisor_full_width(Int & lhs, const Int & divisor) noexcept { rem_add_divisor(lhs, divisor, limbs); }

    static Int rem_large_single_limb_quotient(Int lhs, const Int & divisor, size_t div_limbs) noexcept
    {
        if (div_limbs < 2)
            return lhs;

        const limb_type q = estimate_single_limb_quotient(lhs, divisor, div_limbs);
        if (div_limbs == limbs)
        {
            if (rem_sub_mul_limb_full_width(lhs, divisor, q))
                rem_add_divisor_full_width(lhs, divisor);
        }
        else if (rem_sub_mul_limb(lhs, divisor, div_limbs, q))
        {
            rem_add_divisor(lhs, divisor, div_limbs);
        }
        return lhs;
    }

#if GINT_DETAIL_X86_64_GCC
    // Keep wide remainder code out of callers to preserve inlining elsewhere.
    GINT_NOINLINE
#endif
    static Int rem_large(Int lhs, const Int & divisor, size_t div_limbs) noexcept
    {
        const size_t dividend_limbs = Int::used_limbs(lhs);
        if (dividend_limbs == div_limbs && div_limbs >= 2)
            return rem_large_single_limb_quotient(lhs, divisor, div_limbs);
        return div_or_rem_large_core<true>(lhs, divisor, div_limbs, dividend_limbs);
    }

    static GINT_FORCE_INLINE u128 sub_product(limb_type & word, u128 product) noexcept
    {
        const limb_type low = static_cast<limb_type>(product);
        word = static_cast<limb_type>(u128(word) - product);
        return (product >> 64) + (word > ~low);
    }

    // Full-width 256-bit divisors produce one quotient limb.
    template <bool WantRemainder>
    static GINT_FORCE_INLINE limb_type div_large_4_core(const Int & lhs, const Int & divisor, Int * remainder) noexcept
    {
        if (lhs.data_[3] == 0)
        {
            if (WantRemainder)
                *remainder = lhs;
            return 0;
        }

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
        u128 qhat;
        u128 rhat;
        // One or zero normalization bits bound the estimate to three.
        if (shift <= 1)
        {
            qhat = 0;
            rhat = numerator;
            const u128 twice = u128(v3) << 1;
            if (rhat >= twice)
            {
                qhat = 2;
                rhat -= twice;
            }
            if (rhat >= v3)
            {
                ++qhat;
                rhat -= v3;
            }
        }
        else
        {
            qhat = numerator / v3;
            rhat = numerator - qhat * v3;
        }

        while (qhat == (static_cast<u128>(1) << 64) || qhat * v2 > ((rhat << 64) | u2))
        {
            --qhat;
            rhat += v3;
            if (rhat >= (static_cast<u128>(1) << 64))
                break;
        }

        u128 borrow = sub_product(u0, qhat * v0);
        borrow = sub_product(u1, qhat * v1 + borrow);
        borrow = sub_product(u2, qhat * v2 + borrow);
        borrow = sub_product(u3, qhat * v3 + borrow);

        const bool add_back = u128(u4) < borrow;
        if (add_back)
            --qhat;
        if (WantRemainder)
        {
            if (add_back)
            {
                u128 sum = u128(u0) + v0;
                u0 = static_cast<limb_type>(sum);
                sum = u128(u1) + v1 + (sum >> 64);
                u1 = static_cast<limb_type>(sum);
                sum = u128(u2) + v2 + (sum >> 64);
                u2 = static_cast<limb_type>(sum);
                sum = u128(u3) + v3 + (sum >> 64);
                u3 = static_cast<limb_type>(sum);
            }
            // The corrected normalized remainder is strictly below divisor << shift < 2^256,
            // so the fifth limb is zero and only u0..u3 contribute after unshifting.
            if (shift == 0)
            {
                remainder->data_[0] = u0;
                remainder->data_[1] = u1;
                remainder->data_[2] = u2;
                remainder->data_[3] = u3;
            }
            else
            {
                const int inv_shift = 64 - shift;
                remainder->data_[0] = (u0 >> shift) | (u1 << inv_shift);
                remainder->data_[1] = (u1 >> shift) | (u2 << inv_shift);
                remainder->data_[2] = (u2 >> shift) | (u3 << inv_shift);
                remainder->data_[3] = u3 >> shift;
            }
        }
        return static_cast<limb_type>(qhat);
    }

    template <size_t L = limbs, bool WantRemainder = false>
    static GINT_NOINLINE typename std::enable_if<(L == 4), Int>::type
    div_large_4(Int lhs, const Int & divisor, Int * remainder = nullptr) noexcept
    {
        return Int(div_large_4_core<WantRemainder>(lhs, divisor, remainder));
    }

    static GINT_NOINLINE Int rem_large_4_impl(Int lhs, const Int & divisor) noexcept
    {
        Int result(typename Int::uninitialized_tag{});
        div_large_4_core<true>(lhs, divisor, &result);
        return result;
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L == 4), Int>::type rem_large_4(Int lhs, const Int & divisor) noexcept
    {
        return rem_large_4_impl(lhs, divisor);
    }

    // Stub for non-256-bit instantiations to keep dependent calls well-formed.
    template <size_t L = limbs, bool WantRemainder = false>
    static typename std::enable_if<(L != 4), Int>::type div_large_4(Int lhs, const Int & divisor, Int * = nullptr) noexcept
    {
        return div_large(lhs, divisor, 4);
    }

    template <size_t L = limbs>
    static typename std::enable_if<(L != 4), Int>::type rem_large_4(const Int & lhs, const Int &) noexcept
    {
        return lhs;
    }

    template <size_t N>
    static GINT_FORCE_INLINE void store_remainder(Int & remainder, const limb_type * normalized, int shift) noexcept
    {
        remainder = Int();
        for (size_t i = 0; i < N; ++i)
        {
            const limb_type high = i + 1 < N ? normalized[i + 1] : 0;
            remainder.data_[i] = shift ? (normalized[i] >> shift) | (high << (64 - shift)) : normalized[i];
        }
    }

    template <bool HalfBase>
    static GINT_FORCE_INLINE limb_type
    div_step_2(limb_type & uj0, limb_type & uj1, limb_type & uj2, limb_type v0, limb_type v1, u128 inv128) noexcept
    {
        u128 numerator = (static_cast<u128>(uj2) << 64) | uj1;
        // 1) Initial estimate via reciprocal multiply
        u128 qhat = HalfBase ? (numerator >> 63) : detail::mulhi_u128_no_middle_wrap(numerator, inv128);
        u128 qhat_v1 = HalfBase ? (qhat << 63) : qhat * v1;
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
        return static_cast<limb_type>(qhat);
    }

    template <bool HalfBase>
    static GINT_FORCE_INLINE void
    div_steps_2(Int & quotient, std::array<limb_type, limbs + 1> & u, size_t n, limb_type v0, limb_type v1, u128 inv128) noexcept
    {
        if (limbs >= 4 && n == 4)
        {
            quotient.data_[2] = div_step_2<HalfBase>(u[2], u[3], u[4], v0, v1, inv128);
            quotient.data_[1] = div_step_2<HalfBase>(u[1], u[2], u[3], v0, v1, inv128);
            quotient.data_[0] = div_step_2<HalfBase>(u[0], u[1], u[2], v0, v1, inv128);
        }
        else
        {
            for (int j = static_cast<int>(n - 2); j >= 0; --j)
                quotient.data_[j] = div_step_2<HalfBase>(u[j], u[j + 1], u[j + 2], v0, v1, inv128);
        }
    }

    // Optimized specialization: two-limb divisor (divisor_limbs == 2)
    template <size_t L = limbs, bool WantRemainder = false>
    static GINT_CLANG_NOINLINE typename std::enable_if<(L >= 2), Int>::type
    div_large_2(Int lhs, const Int & divisor, Int * remainder = nullptr) noexcept
    {
        Int quotient;
        const size_t n = Int::used_limbs(lhs);
        if (n < 2)
        {
            if (WantRemainder)
                *remainder = lhs;
            return quotient;
        }

        std::array<limb_type, limbs + 1> u = {{}};

        // Normalize divisor so that the top limb has its MSB set.
        const limb_type d0 = divisor.data_[0];
        const limb_type d1 = divisor.data_[1];
        int shift = __builtin_clzll(d1);

        u[n] = lshift_limbs_to(lhs.data_, n, u.data(), shift);

        limb_type v0 = (d0 << shift);
        limb_type v1 = (d1 << shift) | (shift ? static_cast<limb_type>(d0 >> (64 - shift)) : 0);
        // A normalized high limb equal to half the limb base needs no reciprocal.
        if (v1 == (limb_type(1) << 63))
            div_steps_2<true>(quotient, u, n, v0, v1, 0);
        else
            div_steps_2<false>(quotient, u, n, v0, v1, ~u128(0) / v1);
        if (WantRemainder)
            store_remainder<2>(*remainder, u.data(), shift);
        return quotient;
    }

    // Safe fallback for a direct test/internal call on a type that cannot have
    // a two-limb divisor. Normal operator dispatch never reaches this overload.
    template <size_t L = limbs, bool WantRemainder = false>
    static typename std::enable_if<(L < 2), Int>::type div_large_2(Int lhs, const Int & divisor, Int * = nullptr) noexcept
    {
        return lhs / divisor;
    }

    // Optimized specialization: three-limb divisor (divisor_limbs == 3)
    template <size_t L = limbs, bool WantRemainder = false>
    static typename std::enable_if<(L >= 3), Int>::type div_large_3(Int lhs, const Int & divisor, Int * remainder = nullptr) noexcept
    {
        Int quotient;
        const size_t n = Int::used_limbs(lhs);
        if (n < 3)
        {
            if (WantRemainder)
                *remainder = lhs;
            return quotient;
        }

        std::array<limb_type, limbs + 1> u = {{}};
        std::array<limb_type, 3> v = {{}};

        // Normalize divisor: ensure MSB of v[2] is set
        int shift = __builtin_clzll(divisor.data_[2]);
        // GCC and Clang on x86 generate a shorter correction path when
        // normalization keeps the shift test in the dividend loop.
        u[n] = lshift_limbs_to<!GINT_ARCH_X86_64>(lhs.data_, n, u.data(), shift);
        lshift_limbs_to(divisor.data_, 3, v.data(), shift);

        for (int j = static_cast<int>(n - 3); j >= 0; --j)
        {
            unsigned __int128 numerator = (static_cast<unsigned __int128>(u[j + 3]) << 64) | u[j + 2];
            u128 qhat;
            u128 rhat;
#if GINT_ARCH_X86_64
            limb_type high = u[j + 3];
            if (high < v[2])
            {
                qhat = detail::divrem_u128_u64(u[j + 2], v[2], high);
                rhat = high;
            }
            else
#endif
            {
                qhat = numerator / v[2];
                rhat = numerator - qhat * v[2];
            }

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
        if (WantRemainder)
            store_remainder<3>(*remainder, u.data(), shift);
        return quotient;
    }

    // Safe fallback for a direct test/internal call on a type that cannot have
    // a three-limb divisor. Normal operator dispatch never reaches this overload.
    template <size_t L = limbs, bool WantRemainder = false>
    static typename std::enable_if<(L < 3), Int>::type div_large_3(Int lhs, const Int & divisor, Int * = nullptr) noexcept
    {
        return lhs / divisor;
    }

    static GINT_NOINLINE Int divmod_large_single_limb_quotient(Int lhs, const Int & divisor, size_t div_limbs, Int & remainder) noexcept
    {
        if (div_limbs < 2)
        {
            remainder = lhs;
            return Int();
        }

        // For equal-length magnitudes with lhs > divisor, the estimate is exact or one too large.
        limb_type q = estimate_single_limb_quotient(lhs, divisor, div_limbs);
        if (div_limbs == limbs)
        {
            if (rem_sub_mul_limb_full_width(lhs, divisor, q))
            {
                --q;
                rem_add_divisor_full_width(lhs, divisor);
            }
        }
        else if (rem_sub_mul_limb(lhs, divisor, div_limbs, q))
        {
            --q;
            rem_add_divisor(lhs, divisor, div_limbs);
        }
        // Limbs above div_limbs were zero on entry and remain untouched.
        remainder = lhs;
        return Int(q);
    }

    static divmod_result<Int> divmod_signed_wide(const Int & dividend, const Int & divisor)
    {
        const bool lhs_neg = dividend.data_[limbs - 1] >> 63;
        const bool rhs_neg = divisor.data_[limbs - 1] >> 63;
        Unsigned rhs_mag(typename Unsigned::uninitialized_tag{});
        copy_abs_magnitude(rhs_mag, divisor, rhs_neg);
        Unsigned lhs_mag(typename Unsigned::uninitialized_tag{});
        copy_abs_magnitude(lhs_mag, dividend, lhs_neg);
        const divmod_result<Unsigned> magnitude = gint::divmod(lhs_mag, rhs_mag);
        divmod_result<Int> result = {Int(magnitude.quotient), Int(magnitude.remainder)};
        if (lhs_neg != rhs_neg)
            result.quotient = -result.quotient;
        if (lhs_neg)
            result.remainder = -result.remainder;
        return result;
    }
};
} // namespace detail

/// Quotient and remainder produced by a single public division operation.
template <typename Integer>
struct divmod_result
{
    Integer quotient;
    Integer remainder;
};

/// Compute quotient and remainder while sharing the expensive quotient work.
///
/// At widths of 128 bits and above, retain the remainder from the division
/// kernel when available. For 64 bits, rebuild it from the quotient.
template <size_t Bits, typename Signed>
inline divmod_result<integer<Bits, Signed>> divmod(const integer<Bits, Signed> & dividend, const integer<Bits, Signed> & divisor)
{
    using Int = integer<Bits, Signed>;
    using Division = detail::division_ops<Bits, Signed>;
    if (Bits == 128)
        return Division::divmod_128(dividend, divisor);
    if (Bits < 256)
    {
        if (GINT_UNLIKELY(divisor.is_zero()))
        {
            GINT_DIVZERO_CHECK(true);
            return {Int(), dividend};
        }
        const Int quotient = dividend / divisor;
        return {quotient, dividend - quotient * divisor};
    }

    if (std::is_same<Signed, signed>::value)
        return Division::divmod_signed_wide(dividend, divisor);

    // Keep the unsigned dispatch visible when the caller knows the divisor shape.
    using limb_type = typename Int::limb_type;
    constexpr size_t limbs = Int::limbs;
    if (limbs == 4 && dividend.data_[limbs - 1] < divisor.data_[limbs - 1])
        return {Int(), dividend};

    const size_t divisor_limbs = Int::used_limbs(divisor);
    if (GINT_UNLIKELY(divisor_limbs == 0))
    {
        GINT_DIVZERO_CHECK(true);
        return {Int(), dividend};
    }

    size_t different = limbs;
    while (different != 0 && dividend.data_[different - 1] == divisor.data_[different - 1])
        --different;
    if (different == 0)
        return {Int(1), Int()};
    if (dividend.data_[different - 1] < divisor.data_[different - 1])
        return {Int(), dividend};

    // Reuse the fixed-width power-of-two path for 256 bits to limit dispatch expansion.
    if (limbs == 4)
    {
        int shift;
        if (Division::is_power_of_two(divisor, shift))
            return {dividend >> shift, dividend & (divisor - Int(1))};
    }

    divmod_result<Int> result;
    if (divisor_limbs == 1)
    {
        result.remainder = Int(Division::div_mod_small(dividend, divisor.data_[0], result.quotient));
        return result;
    }

    if (limbs != 4)
    {
        const limb_type top = divisor.data_[divisor_limbs - 1];
        if ((top & (top - 1)) == 0)
        {
            limb_type lower = 0;
            for (size_t i = 0; i < divisor_limbs - 1; ++i)
                lower |= divisor.data_[i];
            if (lower == 0)
            {
                const size_t shift = (divisor_limbs - 1) * 64 + __builtin_ctzll(top);
                return {Int::shift_right_in_range(dividend, shift), dividend & (divisor - Int(1))};
            }
        }
    }

    if (divisor_limbs == 2)
        result.quotient = Division::template div_large_2<limbs, true>(dividend, divisor, &result.remainder);
    else if (divisor_limbs == 3)
        result.quotient = Division::template div_large_3<limbs, true>(dividend, divisor, &result.remainder);
    else if (limbs == 4)
        result.quotient = Division::template div_large_4<limbs, true>(dividend, divisor, &result.remainder);
    else
    {
        const size_t dividend_limbs = Int::used_limbs(dividend);
        if (dividend_limbs == divisor_limbs)
            result.quotient = Division::divmod_large_single_limb_quotient(dividend, divisor, divisor_limbs, result.remainder);
        else
            result.quotient = Division::divmod_large_core(dividend, divisor, divisor_limbs, dividend_limbs, result.remainder);
    }
    return result;
}

} // namespace GINT_DETAIL_CONFIG_NAMESPACE
} // namespace gint
