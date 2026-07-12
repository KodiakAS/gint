#include <gint/gint.h>

#if defined(GINT_EXPECT_VERSION_MAJOR)
static_assert(GINT_VERSION_MAJOR == GINT_EXPECT_VERSION_MAJOR, "gint major version mismatch");
static_assert(GINT_VERSION_MINOR == GINT_EXPECT_VERSION_MINOR, "gint minor version mismatch");
static_assert(GINT_VERSION_PATCH == GINT_EXPECT_VERSION_PATCH, "gint patch version mismatch");
static_assert(
    GINT_VERSION == GINT_EXPECT_VERSION_MAJOR * 10000 + GINT_EXPECT_VERSION_MINOR * 100 + GINT_EXPECT_VERSION_PATCH,
    "gint encoded version mismatch");
#endif

int main()
{
    const gint::UInt128 value = 42;
    return value / 2 == gint::UInt128(21) ? 0 : 1;
}
