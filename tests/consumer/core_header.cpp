#include <gint/core.h>

int main()
{
    const gint::UInt256 lhs = (gint::UInt256(1) << 200) + 123;
    const gint::UInt256 rhs = 7;
    const auto result = gint::divmod(lhs, rhs);
    return result.quotient * rhs + result.remainder == lhs ? 0 : 1;
}
