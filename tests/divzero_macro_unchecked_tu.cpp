#include <stdexcept>
#include <gint/gint.h>

bool gint_unchecked_zero_division_throws()
{
    using U128 = gint::integer<128, unsigned>;
    U128 value = 1;
    U128 zero = 0;
    try
    {
        (void)(value / zero);
        return false;
    }
    catch (const std::domain_error &)
    {
        return true;
    }
}
