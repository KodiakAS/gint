#include <gint/string_stream.hpp>

#ifndef GINT_ENABLE_FMT
#    error "fmt include-order test requires GINT_ENABLE_FMT"
#endif
#ifdef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    error "direct string_stream.hpp unexpectedly completed the core definition pass"
#endif

#include <gint/gint.hpp>

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    error "internal gint.hpp did not complete the core definition pass"
#endif
#ifndef GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    error "internal gint.hpp did not complete the IO definition pass"
#endif
#ifdef GINT_DETAIL_HEADER_PASS_ACTIVE
#    error "internal gint.hpp leaked its IO definition-pass cleanup marker"
#endif
#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal gint.hpp leaked a pass-local configuration macro"
#endif

#include <string>

int main()
{
    const gint::UInt256 value = (gint::UInt256(1) << 128) + 42;
    return fmt::format("{}", value) == "340282366920938463463374607431768211498" ? 0 : 1;
}
