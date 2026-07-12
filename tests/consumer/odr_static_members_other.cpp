#include <cstddef>

#include <gint/gint.h>

#if defined(_MSC_VER)
#    define GINT_CONSUMER_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#    define GINT_CONSUMER_NOINLINE __attribute__((noinline))
#else
#    define GINT_CONSUMER_NOINLINE
#endif

GINT_CONSUMER_NOINLINE const std::size_t * bits_address_other()
{
    return &gint::Int256::bits;
}

GINT_CONSUMER_NOINLINE const std::size_t * limbs_address_other()
{
    return &gint::Int256::limbs;
}

#undef GINT_CONSUMER_NOINLINE
