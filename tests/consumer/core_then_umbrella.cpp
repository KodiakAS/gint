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

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    error "gint.h lost the completed core definition pass"
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
#ifdef GINT_FORCE_INLINE
#    error "gint.h leaked a pass-local inline macro"
#endif

#include <sstream>
#include <string>

int main()
{
    const gint::UInt256 value = gint::from_string<gint::UInt256>("12345678901234567890");
    std::ostringstream out;
    out << value;
    return out.str() == gint::to_string(value) ? 0 : 1;
}
