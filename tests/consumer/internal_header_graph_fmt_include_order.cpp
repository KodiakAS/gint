#include <gint/string_stream.hpp>

#ifndef GINT_ENABLE_FMT
#    error "fmt include-order test requires GINT_ENABLE_FMT"
#endif

#include <gint/gint.hpp>

#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal gint.hpp leaked a private configuration macro"
#endif

#include <string>

int main()
{
    const gint::UInt256 value = (gint::UInt256(1) << 128) + 42;
    return fmt::format("{}", value) == "340282366920938463463374607431768211498" ? 0 : 1;
}
