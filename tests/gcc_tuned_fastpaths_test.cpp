#include <gint/gint.h>
#include <gtest/gtest.h>

namespace
{

using U256 = gint::integer<256, unsigned>;
using U512 = gint::integer<512, unsigned>;

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
    const U256 divisor = make_u256(0x1000000000000000ULL, 0x0fedcba987654321ULL, 0x0123456789abcdefULL, 0x1111111111111111ULL);
    const U256 remainder = 123456789ULL;
    const U256 lhs = divisor * U256(9) + remainder;

    EXPECT_EQ(lhs / divisor, U256(9));
    EXPECT_EQ(lhs % divisor, remainder);
}

TEST(GccTunedFastpaths, UInt512SingleLimbQuotientDivision)
{
    const U512 lhs = (U512(1) << 511) - U512(17);

    for (int shift : {448, 476})
    {
        const U512 divisor = (U512(1) << shift) + U512(123456789ULL);
        const U512 quotient = (U512(1) << (511 - shift)) - U512(1);
        const U512 remainder = lhs - divisor * quotient;

        EXPECT_EQ(lhs / divisor, quotient);
        EXPECT_EQ(lhs % divisor, remainder);
        EXPECT_LT(remainder, divisor);
    }
}
