#define GINT_ENABLE_AARCH64_LIMB_ASM 0
#define GINT_GCC_TUNED_PATHS 1
#define GINT_CLANG_TUNED_PATHS 0
#include <gint/gint.h>

#include <typeinfo>

extern "C" int gint_gcc_tuned_arithmetic()
{
    const gint::UInt256 lhs = (gint::UInt256(1) << 200) + 123;
    const gint::UInt256 rhs = 7;
    return (lhs + rhs) - rhs == lhs ? 1 : 0;
}

extern "C" const std::type_info * gint_gcc_tuned_type_info()
{
    return &typeid(gint::Int128);
}
