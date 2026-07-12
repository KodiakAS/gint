#include <gint/gint.h>

static_assert(GINT_VERSION_MAJOR == 0, "unexpected gint major version");
static_assert(GINT_VERSION_MINOR == 9, "unexpected gint minor version");
static_assert(GINT_VERSION_PATCH == 0, "unexpected gint patch version");
static_assert(GINT_VERSION == 900, "unexpected encoded gint version");

int main()
{
    const gint::UInt128 value = 42;
    return value == gint::UInt128(42) ? 0 : 1;
}
