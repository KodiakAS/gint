#define GINT_ENABLE_AARCH64_LIMB_ASM 0
#define GINT_ENABLE_DIVZERO_CHECKS
#define GINT_GCC_TUNED_PATHS 0
#define GINT_CLANG_TUNED_PATHS 1
#include <gint/gint.h>

#include <limits>
#include <stdexcept>
#include <typeinfo>

extern "C" int gint_checked_zero_division()
{
    try
    {
        const gint::Int128 value = std::numeric_limits<gint::Int128>::min();
        const gint::Int128 zero = 0;
        static_cast<void>(value / zero);
    }
    catch (const std::domain_error &)
    {
        return 1;
    }
    return 0;
}

extern "C" const std::type_info * gint_checked_type_info()
{
    return &typeid(gint::Int128);
}
