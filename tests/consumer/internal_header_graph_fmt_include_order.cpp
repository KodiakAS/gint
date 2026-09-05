#include <gint/string_stream.hpp>

#include <sstream>

// Use the text module before the complete entry introduces standard-library adapters.
static bool text_module_works()
{
    std::ostringstream out;
    out << gint::UInt256(42);
    return out.str() == "42" && gint::from_string<gint::UInt256>("42") == 42;
}

#ifndef GINT_ENABLE_FMT
#    error "fmt include-order test requires GINT_ENABLE_FMT"
#endif

#include <gint/fmt.hpp>

// Direct fmt inclusion must provide its own external headers and text dependencies.
static bool fmt_module_works()
{
    return fmt::format("{}", gint::UInt256(42)) == "42";
}

#include <gint/gint.hpp>

static_assert(std::numeric_limits<gint::UInt256>::is_specialized, "complete entry must include standard-library adapters");

#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal gint.hpp leaked a private configuration macro"
#endif

#include <string>

int main()
{
    const gint::UInt256 value = (gint::UInt256(1) << 128) + 42;
    return text_module_works() && fmt_module_works() && fmt::format("{}", value) == "340282366920938463463374607431768211498" ? 0 : 1;
}
