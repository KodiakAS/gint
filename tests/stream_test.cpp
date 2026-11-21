#include <iomanip>
#include <sstream>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerStream, Output)
{
    gint::integer<128, unsigned> v = 42;
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "42");
}

TEST(WideIntegerStream, OutputNegative)
{
    gint::integer<128, signed> v = -42;
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "-42");
}

TEST(WideIntegerStream, OutputZero)
{
    gint::integer<128, unsigned> v = 0;
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "0");
}

TEST(WideIntegerStream, HonorsIntegerBaseFlags)
{
    using U128 = gint::integer<128, unsigned>;
    const U128 value = (U128(1) << 68) + U128(0x2a);

    {
        std::ostringstream oss;
        oss << std::hex << value;
        EXPECT_EQ(oss.str(), "10000000000000002a");
    }

    {
        std::ostringstream oss;
        oss << std::showbase << std::uppercase << std::hex << value;
        EXPECT_EQ(oss.str(), "0X10000000000000002A");
    }

    {
        std::ostringstream oss;
        oss << std::oct << U128(71);
        EXPECT_EQ(oss.str(), "107");
    }
}

TEST(WideIntegerStream, FormatsSignedNegativeHexAsBitPattern)
{
    gint::integer<128, signed> value = -42;
    std::ostringstream oss;
    oss << std::hex << value;
    EXPECT_EQ(oss.str(), "ffffffffffffffffffffffffffffffd6");
}

TEST(WideIntegerStream, HonorsWidthFillAndSigns)
{
    using U128 = gint::integer<128, unsigned>;

    {
        std::ostringstream oss;
        oss << std::showpos << U128(42);
        EXPECT_EQ(oss.str(), "+42");
    }

    {
        std::ostringstream oss;
        oss << std::showbase << std::hex << std::internal << std::setw(8) << std::setfill('0') << U128(42);
        EXPECT_EQ(oss.str(), "0x00002a");
    }

    {
        std::ostringstream oss;
        oss << std::left << std::setw(5) << U128(42);
        EXPECT_EQ(oss.str(), "42   ");
    }
}

TEST(WideIntegerStream, PaddingUsesBoundedStreamFlushes)
{
    struct counting_buffer : std::stringbuf
    {
        int sync() override
        {
            ++flushes;
            return 0;
        }
        int flushes = 0;
    };
    for (auto align : {std::ios_base::left, std::ios_base::right, std::ios_base::internal})
    {
        counting_buffer buffer;
        std::ostream out(&buffer);
        out.setf(align, std::ios_base::adjustfield);
        out << std::unitbuf << std::showbase << std::hex << std::setfill('_') << std::setw(80) << gint::UInt128(1);
        const std::string pad(77, '_');
        EXPECT_EQ(
            buffer.str(),
            align == std::ios_base::internal   ? "0x" + pad + "1"
                : align == std::ios_base::left ? "0x1" + pad
                                               : pad + "0x1");
        EXPECT_LE(buffer.flushes, 2);
        EXPECT_EQ(out.width(), 0);
    }
}

TEST(WideIntegerStream, OutputAdditionalWidths)
{
    {
        gint::integer<256, unsigned> v = (gint::integer<256, unsigned>(1) << 128) + 42;
        std::ostringstream oss;
        oss << v;
        EXPECT_EQ(oss.str(), "340282366920938463463374607431768211498");
    }

    {
        gint::integer<512, signed> v = -((gint::integer<512, signed>(1) << 200) + 7);
        std::ostringstream oss;
        oss << v;
        EXPECT_EQ(oss.str(), "-1606938044258990275541962092341162602522202993782792835301383");
    }
}

TEST(WideIntegerStream, ToStringChunksWithZeros)
{
    using U = gint::integer<256, unsigned>;
    const U base = U(1000000000ULL); // 1e9
    U v = U(123);
    v *= base; // * 1e9
    v *= base; // * 1e18
    v *= base; // * 1e27
    v += U(45);
    // Expect three zero chunks (9 digits each) between 123 and 045
    EXPECT_EQ(gint::to_string(v), std::string("123000000000000000000000000045"));
}

TEST(WideIntegerStream, ToStringSignedMinValues)
{
    using S128 = gint::integer<128, signed>;
    using S256 = gint::integer<256, signed>;

    S128 s128_min = std::numeric_limits<S128>::min();
    EXPECT_EQ(gint::to_string(s128_min), std::string("-170141183460469231731687303715884105728"));

    S256 s256_min = std::numeric_limits<S256>::min();
    EXPECT_EQ(gint::to_string(s256_min), std::string("-57896044618658097711785492504343953926634992332820282019728792003956564819968"));
}
