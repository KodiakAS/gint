#define GINT_ENABLE_DIVZERO_CHECKS
#include <gint/gint.h>

int gint_divzero_macro_enabled_tu_anchor()
{
    using U128 = gint::integer<128, unsigned>;
    U128 value = 1;
    return static_cast<int>(static_cast<unsigned long long>(value));
}
