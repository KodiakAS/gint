#include <gint/gint.hpp>

int main()
{
    const gint::UInt256 value = (gint::UInt256(1) << 128) + 42;
    return gint::to_string(value) == "340282366920938463463374607431768211498" ? 0 : 1;
}
