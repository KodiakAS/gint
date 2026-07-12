#include <cstddef>

#include <gint/gint.h>

#if defined(_MSC_VER)
#    define GINT_CONSUMER_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#    define GINT_CONSUMER_NOINLINE __attribute__((noinline))
#else
#    define GINT_CONSUMER_NOINLINE
#endif

GINT_CONSUMER_NOINLINE const std::size_t * bits_address()
{
    return &gint::Int256::bits;
}

GINT_CONSUMER_NOINLINE const std::size_t * limbs_address()
{
    return &gint::Int256::limbs;
}

const std::size_t * bits_address_other();
const std::size_t * limbs_address_other();

int main()
{
    return *bits_address() == 256 && *limbs_address() == 4 && bits_address() == bits_address_other()
            && limbs_address() == limbs_address_other()
        ? 0
        : 1;
}

#undef GINT_CONSUMER_NOINLINE
