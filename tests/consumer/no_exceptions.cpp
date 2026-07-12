#include <gint/gint.h>

int main()
{
    const gint::UInt256 lhs = (gint::UInt256(1) << 200) + 123;
    const gint::UInt256 rhs = 7;
    const gint::UInt256 quotient = lhs / rhs;
    const gint::UInt256 remainder = lhs % rhs;
    return quotient * rhs + remainder == lhs ? 0 : 1;
}
