#include <gint/gint.hpp>

#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal gint.hpp leaked a private configuration macro"
#endif
#ifdef GINT_FORCE_INLINE
#    error "internal gint.hpp leaked a private inline macro"
#endif

int main()
{
    const gint::UInt256 value = (gint::UInt256(1) << 128) + 42;
    return gint::to_string(value) == "340282366920938463463374607431768211498" ? 0 : 1;
}
