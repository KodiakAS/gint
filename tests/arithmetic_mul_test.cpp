#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerMultiplication, SmallMul)
{
    using UInt256 = gint::integer<256, unsigned>;
    UInt256 a = (UInt256(1) << 128) + 5;
    UInt256 expected_b = (UInt256(3) << 128) + UInt256(15);
    EXPECT_EQ(a * 3ULL, expected_b);

    UInt256 c = UInt256(123456789);
    EXPECT_EQ(c * 7ULL, UInt256(864197523));
}

// 256-bit multiply where operands fit in 128 bits but product exceeds 2^128
TEST(WideIntegerMultiplication, UInt256_128x128_ProducesFull256)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = U256(1) << 64;
    U256 b = a;
    U256 p = a * b;
    EXPECT_EQ(p, U256(1) << 128);
}

// 256-bit multiply of 64-bit by 128-bit numbers
TEST(WideIntegerMultiplication, UInt256_64x128_ProducesFull256)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = U256(1) << 64;
    U256 b = U256(1) << 96;
    U256 p = a * b;
    EXPECT_EQ(p, U256(1) << 160);
}

// 128-bit wide×wide multiplication specialized path (mul_limbs<2>)
TEST(WideIntegerMultiplication, UInt128_WideTimesWide_Specialized)
{
    using U128 = gint::integer<128, unsigned>;
    unsigned __int128 ai = (static_cast<unsigned __int128>(1) << 100) + 0x1234;
    unsigned __int128 bi = (static_cast<unsigned __int128>(1) << 80) + 0x5678;
    U128 a = ai;
    U128 b = bi;
    U128 p = a * b; // keeps low 128 bits
    unsigned __int128 expected = ai * bi; // implicit mod 2^128
    EXPECT_EQ(p, U128(expected));
}

// 256-bit wide×wide multiplication (mul_limbs<4> Comba path)
TEST(WideIntegerMultiplication, UInt256_WideTimesWide_Comba)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = 0;
    a += U256(0x1122334455667788ULL);
    a += U256(0x99AABBCCDDEEFF00ULL) << 64;
    a += U256(0x0123456789ABCDEFULL) << 128;
    a += U256(0x0001112223334444ULL) << 192;

    U256 three = U256(3);
    U256 prod = a * three;
    U256 sum = a + a + a;
    EXPECT_EQ(prod, sum);
}

// 512-bit wide×wide multiplication (generic mul_limbs path)
TEST(WideIntegerMultiplication, UInt512_WideTimesWide_Generic)
{
    using U512 = gint::integer<512, unsigned>;
    U512 a = 0;
    a += U512(0xCAFEBABE8BADF00DULL);
    a += U512(0x0011223344556677ULL) << 64;
    a += U512(0x0102030405060708ULL) << 128;
    a += U512(0xA0B0C0D0E0F00123ULL) << 384;

    U512 five = U512(5);
    U512 prod = a * five;
    U512 sum = a + a + a + a + a;
    EXPECT_EQ(prod, sum);
}

// 512-bit multiply-by-1 to exercise generic mul path without carry
TEST(WideIntegerMultiplication, UInt512_TimesOne_NoCarry)
{
    using U512 = gint::integer<512, unsigned>;
    U512 a = 0;
    a += U512(0x1111222233334444ULL);
    a += U512(0x5555666677778888ULL) << 64;
    a += U512(0x9999AAAABBBBCCCCULL) << 128;
    a += U512(0xDDDD111122223333ULL) << 448;
    U512 one = U512(1);
    U512 prod = a * one;
    EXPECT_EQ(prod, a);
}

// 512-bit multiply-by-2 to force carries through generic mul
TEST(WideIntegerMultiplication, UInt512_TimesTwo_WithCarry)
{
    using U512 = gint::integer<512, unsigned>;
    U512 a = ~U512(0); // all ones
    U512 prod = a * U512(2);
    U512 expected = a;
    expected <<= 1; // fixed-width semantics
    EXPECT_EQ(prod, expected);
}

// 1024-bit wide×wide multiplication (generic mul_limbs path)
TEST(WideIntegerMultiplication, UInt1024_WideTimesWide_Generic)
{
    using U1024 = gint::integer<1024, unsigned>;
    U1024 a = 0;
    a += U1024(0xDEADBEEFDEADBEEFULL);
    a += U1024(0xC001D00DC001D00DULL) << 64;
    a += U1024(0x0123456789ABCDEFULL) << 128;
    a += U1024(0x2222222211111111ULL) << 512;
    a += U1024(0x3333333344444444ULL) << 960;

    U1024 two = U1024(2);
    U1024 prod = a * two;
    U1024 sum = a + a;
    EXPECT_EQ(prod, sum);
}

// Multiplication by power-of-two equals left shift (wide×wide trigger)
TEST(WideIntegerMultiplication, UInt256_MulByPow2EqualsShift)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = (U256(0xABCDEF0123456789ULL) << 128) + U256(0x0123456789ABCDEFULL);
    U256 pow2 = U256(1) << 13; // ensure wide×wide code path
    U256 prod = a * pow2;
    U256 shifted = a;
    shifted <<= 13;
    EXPECT_EQ(prod, shifted);
}

// Addition with cascading carries across multiple limbs
TEST(WideIntegerMultiplication, UInt256_AdditionCarryChain)
{
    using U256 = gint::integer<256, unsigned>;
    U256 a = U256(0xffffffffffffffffULL) + (U256(0xffffffffffffffffULL) << 64);
    U256 b = 1;
    U256 sum = a + b;
    EXPECT_EQ(sum - b, a);
}
