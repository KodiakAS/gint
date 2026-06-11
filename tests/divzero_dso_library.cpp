#include <limits>
#include <stdexcept>

#include <gint/gint.h>

extern "C" int gint_divzero_min_divzero_result()
{
    try
    {
        const gint::Int128 value = std::numeric_limits<gint::Int128>::min();
        const gint::Int128 zero = 0;
        const gint::Int128 quotient = value / zero;
        return quotient == gint::Int128(0) ? 0 : 3;
    }
    catch (const std::domain_error &)
    {
        return 1;
    }
    catch (...)
    {
        return 2;
    }
}
