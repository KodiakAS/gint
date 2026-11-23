#include <exception>
#include <iomanip>
#include <sstream>
#include <gint/gint.h>
#include <gtest/gtest.h>

#if defined(__linux__) && defined(__GLIBCXX__)
#    include <pthread.h>
#endif

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

TEST(WideIntegerStream, FormatsOctalAcrossLimbAndWidthBoundaries)
{
    using U1024 = gint::integer<1024, unsigned>;
    using S1024 = gint::integer<1024, signed>;

    std::ostringstream out;
    out << std::oct << ((U1024(7) << 63) | U1024(1));
    EXPECT_EQ(out.str(), "7" + std::string(20, '0') + "1");

    out.str("");
    out << std::numeric_limits<U1024>::max();
    EXPECT_EQ(out.str(), "1" + std::string(341, '7'));

    out.str("");
    out << std::numeric_limits<S1024>::min();
    EXPECT_EQ(out.str(), "1" + std::string(341, '0'));
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
        const int width = 65536;
        out << std::unitbuf << std::showbase << std::hex << std::setfill('_') << std::setw(width) << gint::UInt128(1);
        const std::string pad(width - 3, '_');
        EXPECT_EQ(
            buffer.str(),
            align == std::ios_base::internal   ? "0x" + pad + "1"
                : align == std::ios_base::left ? "0x1" + pad
                                               : pad + "0x1");
        EXPECT_LE(buffer.flushes, 2);
        EXPECT_EQ(out.width(), 0);
    }
}

TEST(WideIntegerStream, PaddingConsumesWidthOnWriteFailure)
{
    struct rejecting_buffer : std::streambuf
    {
    };
    for (auto align : {std::ios_base::right, std::ios_base::internal})
    {
        rejecting_buffer buffer;
        std::ostream out(&buffer);
        out.setf(align, std::ios_base::adjustfield);
        out << std::showpos << std::setw(9) << gint::Int128(21);
        EXPECT_TRUE(out.bad());
        EXPECT_EQ(out.width(), 0);

        out << std::setw(9) << gint::Int128(21);
        EXPECT_EQ(out.width(), 0);

        out.clear();
        out.exceptions(std::ios_base::badbit);
#if defined(__GLIBCXX__) && defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 5 || __GNUC__ == 6) \
    && defined(_GLIBCXX_USE_CXX11_ABI) && _GLIBCXX_USE_CXX11_ABI == 1
        // libstdc++ throws the old ABI failure type on GCC 5/6 (PR66145).
        EXPECT_THROW(out << std::setw(9) << gint::Int128(21), std::exception);
#else
        EXPECT_THROW(out << std::setw(9) << gint::Int128(21), std::ios_base::failure);
#endif
        EXPECT_EQ(out.width(), 0);
    }
}

TEST(WideIntegerStream, ExtremeWidthReachesThrowingBuffer)
{
    struct write_error
    {
    };
    struct throwing_buffer : std::streambuf
    {
        int_type overflow(int_type) override { throw write_error(); }
    } buffer;

    std::ostream out(&buffer);
    out.exceptions(std::ios_base::badbit);
    out.width(std::numeric_limits<std::streamsize>::max());
    EXPECT_THROW(out << gint::UInt128(7), write_error);
    EXPECT_TRUE(out.bad());
    EXPECT_EQ(out.width(), 0);

    out.clear();
    out.exceptions(std::ios_base::goodbit);
    EXPECT_NO_THROW(out << std::setw(16) << gint::UInt128(7));
    EXPECT_TRUE(out.bad());
    EXPECT_EQ(out.width(), 0);
}

TEST(WideIntegerStream, TiedFlushExceptionHonorsOutputMaskWithAndWithoutPadding)
{
    struct sync_error
    {
    };
    struct throwing_buffer : std::stringbuf
    {
        int sync() override { throw sync_error(); }
    };

    for (int width : {0, 8})
    {
        for (auto mask : {std::ios_base::goodbit, std::ios_base::badbit})
        {
            SCOPED_TRACE(::testing::Message() << "width=" << width << ", exceptions=" << mask);
            throwing_buffer buffer;
            std::ostream tied(&buffer);
            tied.exceptions(std::ios_base::badbit);
            std::ostringstream out;
            out.tie(&tied);
            out.exceptions(mask);
            out.width(width);

            if (mask == std::ios_base::goodbit)
            {
                EXPECT_NO_THROW(out << gint::UInt128(42));
            }
            else
            {
                EXPECT_THROW(out << gint::UInt128(42), sync_error);
            }
            EXPECT_TRUE(out.bad());
            EXPECT_EQ(out.width(), 0);
        }
    }
}

#if defined(__GLIBCXX__)
TEST(WideIntegerStream, FailedSentryHonorsFailbitMaskWithAndWithoutPadding)
{
    for (int width : {0, 8})
    {
        SCOPED_TRACE(::testing::Message() << "width=" << width);
        std::ostream out(nullptr);
        out.exceptions(std::ios_base::failbit);
        out.width(width);
#    if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 5 || __GNUC__ == 6) && defined(_GLIBCXX_USE_CXX11_ABI) \
        && _GLIBCXX_USE_CXX11_ABI == 1
        EXPECT_THROW(out << gint::UInt128(42), std::exception);
#    else
        EXPECT_THROW(out << gint::UInt128(42), std::ios_base::failure);
#    endif
        EXPECT_NE(out.rdstate() & std::ios_base::failbit, 0);
        EXPECT_EQ(out.width(), 0);
    }
}
#endif

#if defined(__linux__) && defined(__GLIBCXX__)
TEST(WideIntegerStream, ThreadCancellationDuringWriteSetsBadbitAndUnwinds)
{
    struct canceling_buffer : std::streambuf
    {
        std::streamsize xsputn(const char *, std::streamsize count) override
        {
            if (pthread_cancel(pthread_self()) == 0)
                pthread_testcancel();
            return count;
        }
    };

    for (int width : {0, 8})
    {
        for (auto mask : {std::ios_base::goodbit, std::ios_base::badbit})
        {
            SCOPED_TRACE(::testing::Message() << "width=" << width << ", exceptions=" << mask);
            canceling_buffer buffer;
            std::ostream out(&buffer);
            out.width(width);
            out.exceptions(mask);
            pthread_t thread;
            ASSERT_EQ(
                pthread_create(
                    &thread,
                    nullptr,
                    [](void * argument) -> void *
                    {
                        *static_cast<std::ostream *>(argument) << gint::UInt128(42);
                        return nullptr;
                    },
                    &out),
                0);
            void * result = nullptr;
            ASSERT_EQ(pthread_join(thread, &result), 0);
            EXPECT_EQ(result, PTHREAD_CANCELED);
            EXPECT_TRUE(out.bad());
            EXPECT_EQ(out.width(), 0);
        }
    }
}
#endif

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
