#include <gint/gint.h>

int main()
{
    const gint::Int128 value = -84;
    return value / 2 == gint::Int128(-42) ? 0 : 1;
}
