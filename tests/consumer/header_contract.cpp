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
    const unsigned __int128 native_lhs = (static_cast<unsigned __int128>(0x1234) << 64) | 0x5678;
    const gint::UInt128 lhs = native_lhs;
    const gint::UInt128 rhs = (static_cast<unsigned __int128>(0x12ab) << 64) | 0x9abc;
    const gint::UInt128 expected = (static_cast<unsigned __int128>(0x009f) << 64) | 0xccc4;
    const gint::integer<64, signed> negative = -42;
    const gint::UInt128 narrow = 0x1ff;
    if (static_cast<unsigned __int128>(lhs) != native_lhs || static_cast<__int128>(negative) != -42
        || static_cast<signed char>(narrow) != -1 || static_cast<unsigned char>(narrow) != 0xff)
        return 1;
    return value == gint::UInt128(42) && gint::to_string(value) == "42" && (lhs ^ rhs) == expected ? 0 : 1;
}
