#include <stdexcept>

#ifndef GINT_ENABLE_DIVZERO_CHECKS
#    error "checked internal graph requires GINT_ENABLE_DIVZERO_CHECKS"
#endif

#include <gint/gint.hpp>

#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal gint.hpp leaked a private configuration macro"
#endif

int main()
{
    try
    {
        const gint::UInt256 value = 1;
        const gint::UInt256 zero = 0;
        static_cast<void>(value / zero);
    }
    catch (const std::domain_error &)
    {
        return 0;
    }
    return 1;
}
