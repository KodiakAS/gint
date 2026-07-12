#include <limits>

#include <gint/gint.h>

int main()
{
    const gint::Int256 value = std::numeric_limits<gint::Int256>::max();
    return value > gint::Int256(0) ? 0 : 1;
}
