#include <gint/gint.h>

int main()
{
    const gint::UInt128 value = 42;
    return value / 2 == gint::UInt128(21) ? 0 : 1;
}
