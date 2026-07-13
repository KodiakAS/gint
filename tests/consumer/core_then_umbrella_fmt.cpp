#include <gint/core.h>

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    error "core.h did not complete the core definition pass"
#endif
#ifdef GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    error "core.h unexpectedly completed the IO definition pass"
#endif
#ifdef GINT_DETAIL_HEADER_PASS_ACTIVE
#    error "core.h leaked its definition-pass cleanup marker"
#endif

#include <gint/gint.h>

#ifndef GINT_ENABLE_FMT
#    error "fmt-enabled staged consumer requires GINT_ENABLE_FMT"
#endif
#ifndef GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    error "gint.h did not complete the IO upgrade pass"
#endif
#ifdef GINT_DETAIL_HEADER_PASS_ACTIVE
#    error "gint.h leaked its IO definition-pass cleanup marker"
#endif
#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "gint.h leaked a pass-local configuration macro"
#endif

#include <string>

int main()
{
    const gint::UInt256 value = (gint::UInt256(1) << 128) + 42;
    return fmt::format("{}", value) == "340282366920938463463374607431768211498" ? 0 : 1;
}
