#include <gint/gint.h>

// Repeated inclusion must retain the same complete interface.
#include <gint/gint.h>

#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "complete header leaked a private configuration macro"
#endif
#ifdef GINT_FORCE_INLINE
#    error "complete header leaked a private inline macro"
#endif

static_assert(GINT_VERSION_MAJOR == 0, "unexpected gint major version");
static_assert(GINT_VERSION_MINOR == 9, "unexpected gint minor version");
static_assert(GINT_VERSION_PATCH == 0, "unexpected gint patch version");
static_assert(GINT_VERSION == 900, "unexpected encoded gint version");

int main()
{
    const gint::UInt128 value = 42;
    const gint::UInt128 lhs = (static_cast<unsigned __int128>(0x1234) << 64) | 0x5678;
    const gint::UInt128 rhs = (static_cast<unsigned __int128>(0x12ab) << 64) | 0x9abc;
    const gint::UInt128 expected = (static_cast<unsigned __int128>(0x009f) << 64) | 0xccc4;
    return value == gint::UInt128(42) && gint::to_string(value) == "42" && (lhs ^ rhs) == expected ? 0 : 1;
}
