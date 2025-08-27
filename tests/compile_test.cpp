#include <type_traits>
#include <gint/gint.h>
#include <gtest/gtest.h>

#if defined(GINT_ENABLE_DIVZERO_CHECKS)
#    error "GINT_ENABLE_DIVZERO_CHECKS should be undefined by default"
#endif

// Ensure division by zero compiles when checks are disabled.
static void division_no_check()
{
    using U256 = gint::integer<256, unsigned>;
    U256 one = 1;
    U256 zero = 0;
    auto res = one / zero; // NOLINT: intentional UB if executed
    (void)res;
}

TEST(CompileTest, Basic)
{
    gint::integer<128, unsigned> x = 42;
    (void)x;

    static_assert(std::is_convertible<int, gint::Int256>::value, "int should convert to Int256 implicitly");
    static_assert(!std::is_convertible<gint::Int256, int>::value, "Int256 should not implicitly convert to int");
}
