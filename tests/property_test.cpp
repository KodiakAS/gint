#include <cstdint>
#include <limits>
#include <string>
#include <gint/gint.h>
#include <gtest/gtest.h>

namespace
{
class DeterministicRng
{
public:
    explicit DeterministicRng(uint64_t seed)
        : state_(seed)
    {
    }

    uint64_t next() noexcept
    {
        uint64_t x = state_;
        x ^= x << 7;
        x ^= x >> 9;
        x ^= x << 8;
        state_ = x;
        return x;
    }

private:
    uint64_t state_;
};

std::string to_decimal(unsigned __int128 value)
{
    if (value == 0)
        return "0";

    char buf[64];
    unsigned pos = 64;
    while (value)
    {
        buf[--pos] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    return std::string(buf + pos, 64 - pos);
}

gint::UInt256 random_u256(DeterministicRng & rng)
{
    gint::UInt256 value = rng.next();
    for (unsigned i = 0; i < 3; ++i)
    {
        value <<= 64;
        value += gint::UInt256(rng.next());
    }
    return value;
}

gint::Int256 random_s256(DeterministicRng & rng)
{
    gint::Int256 value = gint::Int256(rng.next() & 0xffffu);
    for (unsigned i = 0; i < 2; ++i)
    {
        value <<= 64;
        value += gint::Int256(rng.next());
    }
    return (rng.next() & 1u) ? -value : value;
}

gint::Int256 abs_s256(gint::Int256 value)
{
    return value < gint::Int256(0) ? -value : value;
}
}

TEST(WideIntegerProperty, UInt128MatchesNativeArithmetic)
{
    DeterministicRng rng(0x9e3779b97f4a7c15ULL);
    for (unsigned i = 0; i < 4096; ++i)
    {
        const unsigned __int128 a = (static_cast<unsigned __int128>(rng.next()) << 64) | rng.next();
        unsigned __int128 b = (static_cast<unsigned __int128>(rng.next()) << 64) | rng.next();
        if (b == 0)
            b = 1;

        const gint::UInt128 ga = a;
        const gint::UInt128 gb = b;

        EXPECT_TRUE(static_cast<unsigned __int128>(ga + gb) == a + b);
        EXPECT_TRUE(static_cast<unsigned __int128>(ga - gb) == a - b);
        EXPECT_TRUE(static_cast<unsigned __int128>(ga * gb) == a * b);
        EXPECT_TRUE(static_cast<unsigned __int128>(ga / gb) == a / b);
        EXPECT_TRUE(static_cast<unsigned __int128>(ga % gb) == a % b);
        EXPECT_EQ(gint::to_string(ga), to_decimal(a));
    }
}

TEST(WideIntegerProperty, Int128MatchesNativeSmallSignedArithmetic)
{
    DeterministicRng rng(0x243f6a8885a308d3ULL);
    for (unsigned i = 0; i < 4096; ++i)
    {
        const int64_t a = static_cast<int64_t>(rng.next());
        int64_t b = static_cast<int64_t>(rng.next());
        if (b == 0 || (a == std::numeric_limits<int64_t>::min() && b == -1))
            b = 1;

        const gint::Int128 ga = a;
        const gint::Int128 gb = b;

        EXPECT_TRUE(static_cast<__int128>(ga + gb) == static_cast<__int128>(a) + static_cast<__int128>(b));
        EXPECT_TRUE(static_cast<__int128>(ga - gb) == static_cast<__int128>(a) - static_cast<__int128>(b));
        EXPECT_TRUE(static_cast<__int128>(ga * gb) == static_cast<__int128>(a) * static_cast<__int128>(b));
        EXPECT_TRUE(static_cast<__int128>(ga / gb) == static_cast<__int128>(a / b));
        EXPECT_TRUE(static_cast<__int128>(ga % gb) == static_cast<__int128>(a % b));
    }
}

TEST(WideIntegerProperty, UInt256DivModIdentity)
{
    DeterministicRng rng(0x13198a2e03707344ULL);
    for (unsigned i = 0; i < 2048; ++i)
    {
        const gint::UInt256 dividend = random_u256(rng);
        gint::UInt256 divisor = random_u256(rng);
        if (divisor == gint::UInt256(0))
            divisor = 1;

        const gint::UInt256 quotient = dividend / divisor;
        const gint::UInt256 remainder = dividend % divisor;

        EXPECT_EQ(quotient * divisor + remainder, dividend);
        EXPECT_LT(remainder, divisor);
    }
}

TEST(WideIntegerProperty, Int256DivModIdentityAndRemainderSign)
{
    DeterministicRng rng(0xa4093822299f31d0ULL);
    for (unsigned i = 0; i < 2048; ++i)
    {
        const gint::Int256 dividend = random_s256(rng);
        gint::Int256 divisor = random_s256(rng);
        if (divisor == gint::Int256(0))
            divisor = 7;

        const gint::Int256 quotient = dividend / divisor;
        const gint::Int256 remainder = dividend % divisor;

        EXPECT_EQ(quotient * divisor + remainder, dividend);
        EXPECT_LT(abs_s256(remainder), abs_s256(divisor));
        if (remainder != gint::Int256(0))
            EXPECT_EQ(remainder < gint::Int256(0), dividend < gint::Int256(0));
    }
}
