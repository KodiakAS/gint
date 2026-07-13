#define GINT_DETAIL_CORE_ONLY
#include <gint/core.hpp>
#undef GINT_DETAIL_CORE_ONLY

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    error "internal core.hpp did not complete the core definition pass"
#endif
#ifdef GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    error "internal core.hpp unexpectedly completed the IO definition pass"
#endif

#include <gint/gint.hpp>

#ifndef GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    error "internal gint.hpp did not complete the IO upgrade pass"
#endif
#ifdef GINT_DETAIL_HEADER_PASS_ACTIVE
#    error "internal gint.hpp leaked its definition-pass cleanup marker"
#endif
#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal gint.hpp leaked a pass-local configuration macro"
#endif
#ifdef GINT_FORCE_INLINE
#    error "internal gint.hpp leaked a pass-local inline macro"
#endif

int main()
{
    const gint::UInt256 value = (gint::UInt256(1) << 128) + 42;
    return gint::to_string(value) == "340282366920938463463374607431768211498" ? 0 : 1;
}
