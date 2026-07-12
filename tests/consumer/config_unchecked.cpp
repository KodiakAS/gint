#define GINT_ENABLE_AARCH64_LIMB_ASM 0
#define GINT_GCC_TUNED_PATHS 0
#define GINT_CLANG_TUNED_PATHS 1
#include <gint/gint.h>

#include <limits>
#include <typeinfo>

extern "C" int gint_unchecked_zero_division()
{
    const gint::Int128 value = std::numeric_limits<gint::Int128>::min();
    const gint::Int128 zero = 0;
    return value / zero == gint::Int128(0) ? 1 : 0;
}

extern "C" const std::type_info * gint_unchecked_type_info()
{
    return &typeid(gint::Int128);
}
