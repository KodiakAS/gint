#include <gint/gint.h>

// Repeated inclusion must retain the same complete interface.
#include <gint/gint.h>

#include "header_contract_checks.h"

#ifdef GINT_TEST_INTEL_ASM
gint::UInt256 intel_asm_divide(const gint::UInt256 & dividend, std::uint64_t divisor)
{
    return dividend / divisor;
}

gint::UInt256 intel_asm_remainder(const gint::UInt256 & dividend, std::uint64_t divisor)
{
    return dividend % divisor;
}

gint::divmod_result<gint::UInt256> intel_asm_divmod(const gint::UInt256 & dividend, std::uint64_t divisor)
{
    return gint::divmod(dividend, gint::UInt256(divisor));
}
#endif

int main()
{
    return check_header_contract() ? 0 : 1;
}
