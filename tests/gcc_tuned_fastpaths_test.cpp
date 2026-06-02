#include <gint/gint.h>
#include <gtest/gtest.h>

namespace
{

using U256 = gint::integer<256, unsigned>;

U256 make_u256(uint64_t w3, uint64_t w2, uint64_t w1, uint64_t w0)
{
    U256 x = U256(w0);
    x |= U256(w1) << 64;
    x |= U256(w2) << 128;
    x |= U256(w3) << 192;
    return x;
}

} // namespace

TEST(GccTunedFastpaths, UInt256LowLimbMultiply)
{
    const uint64_t lhs64 = 0x123456789abcdef0ULL;
    const uint64_t rhs64 = 0x10fedcba98765432ULL;

    const U256 product = U256(lhs64) * U256(rhs64);
    const unsigned __int128 expected = static_cast<unsigned __int128>(lhs64) * rhs64;

    EXPECT_EQ(product, U256(expected));
}

TEST(GccTunedFastpaths, UInt256FullWidthModulo)
{
    struct Case
    {
        U256 divisor;
        U256 quotient;
        U256 remainder;
    };

    const Case cases[] = {
        {make_u256(0x1000000000000000ULL, 0x0fedcba987654321ULL, 0x0123456789abcdefULL, 0x1111111111111111ULL),
         U256(9),
         U256(123456789ULL)},
        {make_u256(0x8000000000000000ULL, 0x76543210fedcba98ULL, 0x89abcdef01234567ULL, 0x1111111111111111ULL),
         U256(1),
         make_u256(0x0000000000000000ULL, 0x0000000000000001ULL, 0x2222222222222222ULL, 0x3333333333333333ULL)},
        {make_u256(0x0000ffffffffffffULL, 0xfc18ffffffffffffULL, 0xffff000000000000ULL, 0x0000000000000001ULL),
         U256(5),
         make_u256(0x0000000000000000ULL, 0x0000000000000000ULL, 0x4444444444444444ULL, 0x5555555555555555ULL)},
    };

    for (const auto & c : cases)
    {
        const U256 lhs = c.divisor * c.quotient + c.remainder;

        EXPECT_EQ(lhs / c.divisor, c.quotient);
        EXPECT_EQ(lhs % c.divisor, c.remainder);
    }
}

TEST(GccTunedFastpaths, UInt256FullWidthModuloAddBackBorrow)
{
    const U256 divisor = make_u256(0x000094db6cdc2e34ULL, 0xb91396c790d85d37ULL, 0xeb85403c7e8db475ULL, 0xd9621895e98f559dULL);
    const U256 lhs = divisor - U256(0x40be4b66aaea7d9cULL);

    EXPECT_LT(lhs, divisor);
    EXPECT_EQ(lhs / divisor, U256(0));
    EXPECT_EQ(lhs % divisor, lhs);
}
