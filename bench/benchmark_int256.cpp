#include <benchmark/benchmark.h>
#ifdef GINT_ENABLE_CH_COMPARE
#    include <wide_integer/wide_integer.h>
#endif
#ifdef GINT_ENABLE_BOOST_COMPARE
#    include <boost/multiprecision/cpp_int.hpp>
#endif

#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <gint/gint.h>

#ifndef GINT_BENCH_BITS
#    define GINT_BENCH_BITS 256
#endif

constexpr size_t kBenchBits = GINT_BENCH_BITS;
static_assert(kBenchBits % 64 == 0, "benchmark widths must be limb-aligned");
static_assert(kBenchBits >= 128, "benchmark widths must be at least 128 bits");

// Keep the shared comparison matrix in one mathematical value domain. gint and
// ClickHouse use two's-complement signed integers, while Boost's fixed signed
// backend uses signed-magnitude. Feeding the same raw words to those signed
// representations can therefore produce opposite signs. Unsigned fixed-width
// types preserve the same non-negative value for every generated bit pattern.
using WInt = gint::integer<kBenchBits, unsigned>;
#ifdef GINT_ENABLE_CH_COMPARE
using CInt = wide::integer<kBenchBits, unsigned>;
#endif
#ifdef GINT_ENABLE_BOOST_COMPARE
using BInt = boost::multiprecision::number<
    boost::multiprecision::
        cpp_int_backend<kBenchBits, kBenchBits, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
#endif

inline std::string to_string_convert(const WInt & x)
{
    return gint::to_string(x);
}
#ifdef GINT_ENABLE_CH_COMPARE
inline std::string to_string_convert(const CInt & x)
{
    return wide::to_string(x);
}
#endif
#ifdef GINT_ENABLE_BOOST_COMPARE
inline std::string to_string_convert(const BInt & x)
{
    return x.str();
}
#endif

namespace
{

constexpr size_t kDataN = 256; // small deterministic dataset per case
constexpr uint64_t kSeedBase = 0x9E3779B97F4A7C15ull;
constexpr size_t kBenchLimbs = kBenchBits / 64;

template <typename Int>
inline Int assemble_words(const std::array<uint64_t, kBenchLimbs> & words)
{
    Int x = Int{0};
    for (size_t i = 0; i < kBenchLimbs; ++i)
        x |= (Int{words[i]} << static_cast<int>(i * 64));
    return x;
}

template <typename Int>
inline Int random_wide(std::mt19937_64 & rng, bool clear_top_bit = false)
{
    std::array<uint64_t, kBenchLimbs> words{};
    for (size_t i = 0; i < kBenchLimbs; ++i)
        words[i] = rng();
    if (clear_top_bit)
        words[kBenchLimbs - 1] &= 0x7FFF'FFFF'FFFF'FFFFull;
    return assemble_words<Int>(words);
}

template <typename Int>
inline Int random_wide_with_low_limb(std::mt19937_64 & rng, uint64_t low_limb)
{
    std::array<uint64_t, kBenchLimbs> words{};
    words[0] = low_limb;
    for (size_t i = 1; i < kBenchLimbs; ++i)
        words[i] = rng();
    return assemble_words<Int>(words);
}

#if defined(GINT_ENABLE_CH_COMPARE) && defined(GINT_ENABLE_BOOST_COMPARE)
bool comparison_values_match(const char * label, const std::array<uint64_t, kBenchLimbs> & words)
{
    const std::string gint_value = to_string_convert(assemble_words<WInt>(words));
    const std::string clickhouse_value = to_string_convert(assemble_words<CInt>(words));
    const std::string boost_value = to_string_convert(assemble_words<BInt>(words));
    if (gint_value == clickhouse_value && gint_value == boost_value)
        return true;

    std::cerr << "comparison value-domain mismatch for " << label << ": gint=" << gint_value << ", ClickHouse=" << clickhouse_value
              << ", Boost=" << boost_value << '\n';
    return false;
}

bool verify_comparison_value_domain()
{
    std::array<uint64_t, kBenchLimbs> words{};
    words[kBenchLimbs - 1] = uint64_t(1) << 63;
    if (!comparison_values_match("top bit", words))
        return false;

    words.fill(~uint64_t(0));
    return comparison_values_match("all bits set", words);
}
#endif

} // namespace

// -------- Addition --------
template <typename Int>
static void Add_NoCarry(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xA55A'AA55'1234'5678ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            // a: random wide value; b: small 32-bit, avoids multi-limb carry in most cases
            Int a = random_wide<Int>(rng);
            Int b = Int{uint32_t(rng())};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a + b);
    }
}

template <typename Int>
static void Add_WidePlusU64(benchmark::State & state)
{
    static std::array<std::pair<Int, uint64_t>, kDataN> data = []
    {
        std::array<std::pair<Int, uint64_t>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x414444554C3634ull);
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {random_wide<Int>(rng), rng()};
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first + p.second);
    }
}

// -------- Mixed-operand: wide * u64 (compare) --------
// Use fully random wide 'a' to avoid accidentally benchmarking a degenerate one-limb case.
template <typename Int>
static void Mul_WideTimesU64(benchmark::State & state)
{
    static std::array<std::pair<Int, uint64_t>, kDataN> data = []
    {
        std::array<std::pair<Int, uint64_t>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x13579BDF'2468'ACE0ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            uint64_t b = rng();
            d[i] = {a, b};
        }
        return d;
    }();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        uint64_t b = p.second;
        benchmark::DoNotOptimize(a * b);
    }
}

template <typename Int>
static void Add_FullCarry(benchmark::State & state)
{
    // Worst-case: all bits set + 1 causes ripple across all limbs
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {~Int{0}, Int{1}};
        return d;
    }();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        Int result = p.first + p.second;
        benchmark::DoNotOptimize(result);
    }
}

// -------- Subtraction --------
template <typename Int>
static void Sub_NoBorrow(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xBEEF'FACE'CAFEBABEull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            // a has low 32 bits set high (1<<31); b fits in 31 bits => no cross-limb borrow
            Int a = random_wide<Int>(rng);
            a |= (Int{1} << 31);
            Int b = Int{uint32_t(rng() & 0x7FFF'FFFFu)};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a - b);
    }
}

template <typename Int>
static void Sub_WideMinusU64(benchmark::State & state)
{
    static std::array<std::pair<Int, uint64_t>, kDataN> data = []
    {
        std::array<std::pair<Int, uint64_t>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x535542554C3634ull);
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {random_wide<Int>(rng), rng()};
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first - p.second);
    }
}

template <typename Int>
static void Sub_FullBorrow(benchmark::State & state)
{
    // Worst-case: 0 - 1 borrows across all limbs
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {Int{0}, Int{1}};
        return d;
    }();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        Int result = p.first - p.second;
        benchmark::DoNotOptimize(result);
    }
}

// -------- Multiplication --------
template <typename Int>
static void Mul_U64xU64(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xC001'D00D'BADC'0FFEuLL);
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {Int{rng()}, Int{rng()}};
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a * b);
    }
}

template <typename Int>
static void Mul_HighxHigh(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xDEAD'BEEF'8BAD'F00Dull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            // Set high words to trigger multi-limb schoolbook paths
            Int a = (Int{1} << static_cast<int>(kBenchBits - 56)) | random_wide<Int>(rng);
            Int b = (Int{1} << static_cast<int>(kBenchBits - 76)) | random_wide<Int>(rng);
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a * b);
    }
}

// -------- Division --------
template <typename Int>
static void Div_SmallDivisor32(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x1234'5678'9ABC'DEF0ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            uint32_t dv = static_cast<uint32_t>((rng() | 1ull) & 0xFFFF'FFFFu); // ensure non-zero
            Int b = Int{dv};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a / b);
    }
}

// Division with a 64-bit small divisor (not fitting in 32-bit)
template <typename Int>
static void Div_SmallDivisor64(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xA1B2'C3D4'E5F6'1234ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            uint64_t dv = (rng() | (1ull << 33)); // ensure > 2^33 and non-zero
            if ((dv & 0xFFFFFFFFULL) == dv)
                dv |= (1ull << 40); // force out of 32-bit range
            Int b = Int{dv};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a / b);
    }
}

template <typename Int>
static void Div_Pow2Divisor(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xF00F'F00F'00F0'0F00ull);
        std::uniform_int_distribution<int> dist_k(1, static_cast<int>(kBenchBits - 56));
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            int k = dist_k(rng);
            Int b = (Int{1} << k);
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a / b);
    }
}

template <typename Int>
static void Div_SimilarMagnitude(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x0BAD'CAFE'FEED'FACEull);
        std::uniform_int_distribution<int> dist_shift(static_cast<int>(kBenchBits - 76), static_cast<int>(kBenchBits - 36));
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = (Int{1} << static_cast<int>(kBenchBits - 1)) - Int{uint32_t(rng())};
            int s = dist_shift(rng);
            Int b = (Int{1} << s) + Int{uint32_t(rng())};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a / b);
    }
}

template <typename Int>
static void ToString(benchmark::State & state)
{
    // Use a static dataset with varied magnitudes to prevent constant folding
    // and ensure reproducibility across iterations
    static std::array<Int, kDataN> data = []
    {
        std::array<Int, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xABCDEF123456ULL);
        for (size_t i = 0; i < kDataN; ++i)
        {
            // Generate numbers of different magnitudes across the upper half of the configured width.
            int shift = static_cast<int>(kBenchBits / 2 + (rng() % (kBenchBits / 2)));
            d[i] = (Int{1} << shift) + Int{rng()};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const Int & a = data[i++ & (kDataN - 1)];
        auto s = to_string_convert(a);
        benchmark::DoNotOptimize(s);
    }
}

#if !defined(GINT_ENABLE_CH_COMPARE) && !defined(GINT_ENABLE_BOOST_COMPARE)
template <unsigned Base>
static std::array<std::string, kDataN> make_from_string_data()
{
    std::array<std::string, kDataN> data{};
    std::mt19937_64 rng(kSeedBase ^ 0x46524F4D535452ull ^ Base);
    const size_t digit_count = Base == 2 ? kBenchBits
        : Base == 8                      ? (kBenchBits + 2u) / 3u
        : Base == 16                     ? (kBenchBits + 3u) / 4u
                                         : (kBenchBits * 30103u) / 100000u + 1u;
    static const char alphabet[] = "0123456789abcdef";
    for (size_t i = 0; i < kDataN; ++i)
    {
        std::string text(digit_count, '0');
        text[0] = alphabet[1u + rng() % (Base - 1u)];
        for (size_t j = 1; j < digit_count; ++j)
            text[j] = alphabet[rng() % Base];
        data[i] = text;
    }
    return data;
}

template <typename Int, unsigned Base>
static void FromString_String(benchmark::State & state)
{
    static const std::array<std::string, kDataN> data = make_from_string_data<Base>();

    size_t i = 0;
    for (auto _ : state)
    {
        const std::string & text = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(gint::from_string<Int>(text, Base));
    }
}

template <typename Int, unsigned Base>
static void FromString_CStr(benchmark::State & state)
{
    static const std::array<std::string, kDataN> data = make_from_string_data<Base>();

    size_t i = 0;
    for (auto _ : state)
    {
        const char * text = data[i++ & (kDataN - 1)].c_str();
        benchmark::DoNotOptimize(gint::from_string<Int>(text, Base));
    }
}
#endif

// -------- Bitwise --------
template <typename Int>
static void Bitwise_And(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xC0FFEE1234567890ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            Int b = random_wide<Int>(rng);
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first & p.second);
    }
}

template <typename Int>
static void Bitwise_Xor(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xBAD5EEDBADC0FFEEull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            Int b = random_wide<Int>(rng);
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first ^ p.second);
    }
}

template <typename Int>
static void Bitwise_Or(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x0123456789ABCDEFull);
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {random_wide<Int>(rng), random_wide<Int>(rng)};
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first | p.second);
    }
}

template <typename Int>
static void Bitwise_Not(benchmark::State & state)
{
    static std::array<Int, kDataN> data = []
    {
        std::array<Int, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xF0E1D2C3B4A59687ull);
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = random_wide<Int>(rng);
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
        benchmark::DoNotOptimize(~data[i++ & (kDataN - 1)]);
}

// -------- Equality --------
template <typename Int>
static void Equal_Equal(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x455155414C455155ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            const Int value = random_wide<Int>(rng);
            d[i] = {value, value};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first == p.second);
    }
}

template <typename Int, size_t DifferentBit>
static void Equal_DifferentBit(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x444946464552454Eull ^ DifferentBit);
        for (size_t i = 0; i < kDataN; ++i)
        {
            const Int value = random_wide<Int>(rng);
            d[i] = {value, value ^ (Int{1} << static_cast<int>(DifferentBit))};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first == p.second);
    }
}

template <typename Int>
static void Equal_TopDifferent(benchmark::State & state)
{
    Equal_DifferentBit<Int, kBenchBits - 1>(state);
}

template <typename Int>
static void Equal_LowDifferent(benchmark::State & state)
{
    Equal_DifferentBit<Int, 0>(state);
}

// -------- Shift --------
template <typename Int>
static void Shift_LeftVariable(benchmark::State & state)
{
    static std::array<std::pair<Int, unsigned>, kDataN> data = []
    {
        std::array<std::pair<Int, unsigned>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x12345678ABCDEF01ull);
        std::uniform_int_distribution<unsigned> dist(1, static_cast<unsigned>(kBenchBits - 1));
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            unsigned shift = dist(rng);
            d[i] = {a, shift};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first << p.second);
    }
}

template <typename Int>
static void Shift_RightVariable(benchmark::State & state)
{
    static std::array<std::pair<Int, unsigned>, kDataN> data = []
    {
        std::array<std::pair<Int, unsigned>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x0FEDCBA987654321ull);
        std::uniform_int_distribution<unsigned> dist(1, static_cast<unsigned>(kBenchBits - 1));
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng);
            unsigned shift = dist(rng);
            d[i] = {a, shift};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first >> p.second);
    }
}

// -------- Modulo --------
template <typename Int>
static void Mod_SmallDivisor64(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x55AA3311CCDD8899ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide<Int>(rng, true);
            uint64_t dv = rng() | (1ull << 32);
            dv |= 1ull; // keep it odd to avoid repeated powers of two
            Int b = Int{dv};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first % p.second);
    }
}

template <typename Int>
static void Mod_SimilarMagnitude(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x0F1E2D3C4B5A6978ull);
        std::uniform_int_distribution<int> dist_shift(static_cast<int>(kBenchBits - 76), static_cast<int>(kBenchBits - 36));
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = (Int{1} << static_cast<int>(kBenchBits - 1)) - Int{uint32_t(rng())};
            int s = dist_shift(rng);
            Int b = (Int{1} << s) + Int{uint32_t(rng())};
            if (b == Int{0})
                b = Int{1};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first % p.second);
    }
}

template <typename Int>
static const std::array<std::pair<Int, Int>, kDataN> & divmod_similar_magnitude_data()
{
    static const std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x4449564D4F44ull);
        std::uniform_int_distribution<int> dist_shift(static_cast<int>(kBenchBits - 76), static_cast<int>(kBenchBits - 36));
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = (Int{1} << static_cast<int>(kBenchBits - 1)) - Int{uint32_t(rng())};
            Int b = (Int{1} << dist_shift(rng)) + Int{uint32_t(rng())};
            d[i] = {a, b};
        }
        return d;
    }();
    return data;
}

template <typename Int>
static void DivMod_SimilarMagnitude(benchmark::State & state)
{
    const auto & data = divmod_similar_magnitude_data<Int>();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
#if !defined(GINT_ENABLE_CH_COMPARE) && !defined(GINT_ENABLE_BOOST_COMPARE)
        auto result = gint::divmod(p.first, p.second);
        benchmark::DoNotOptimize(result.quotient);
        benchmark::DoNotOptimize(result.remainder);
#else
        Int quotient = p.first / p.second;
        Int remainder = p.first % p.second;
        benchmark::DoNotOptimize(quotient);
        benchmark::DoNotOptimize(remainder);
#endif
    }
}

#if !defined(GINT_ENABLE_CH_COMPARE) && !defined(GINT_ENABLE_BOOST_COMPARE)
template <typename Int>
static void DivMod_SimilarMagnitudeSeparate(benchmark::State & state)
{
    const auto & data = divmod_similar_magnitude_data<Int>();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        Int quotient = p.first / p.second;
        Int remainder = p.first % p.second;
        benchmark::DoNotOptimize(quotient);
        benchmark::DoNotOptimize(remainder);
    }
}
#endif

static bool parse_full_matrix_flag(int & argc, char **& argv)
{
    bool full = false;
    if (const char * env = std::getenv("GINT_BENCH_FULL"))
        full = (std::string(env) == "1" || std::string(env) == "true");
    // strip custom flag(s) from argv so Google Benchmark doesn't see them
    static std::vector<char *> new_argv;
    new_argv.clear();
    new_argv.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        std::string s = argv[i] ? argv[i] : "";
        if (s == "--gint_full" || s == "--gint-full")
        {
            full = true;
            continue;
        }
        new_argv.push_back(argv[i]);
    }
    argv = new_argv.data();
    argc = static_cast<int>(new_argv.size());
    return full;
}

#ifdef GINT_ENABLE_CH_COMPARE
#    define REG_CASE_CH(Base, Func) benchmark::RegisterBenchmark(Base "/ClickHouse", &Func<CInt>)
#else
#    define REG_CASE_CH(Base, Func)
#endif
#ifdef GINT_ENABLE_BOOST_COMPARE
#    define REG_CASE_BOOST(Base, Func) benchmark::RegisterBenchmark(Base "/Boost", &Func<BInt>)
#else
#    define REG_CASE_BOOST(Base, Func)
#endif
#define REG_CASE(Base, Func) \
    do \
    { \
        benchmark::RegisterBenchmark(Base "/gint", &Func<WInt>); \
        REG_CASE_CH(Base, Func); \
        REG_CASE_BOOST(Base, Func); \
    } while (false)

// Extra cases for the full matrix only
template <typename Int>
static void Add_CarryChain64(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xCC55'DDAA'9988'7766ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide_with_low_limb<Int>(rng, ~0ULL); // low limb all 1s
            Int b = Int{1};
            d[i] = {a, b};
        }
        return d;
    }();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a + b);
    }
}

template <typename Int>
static void Sub_BorrowChain64(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x1122'3344'5566'7788ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = random_wide_with_low_limb<Int>(rng, 0); // low limb zero
            Int b = Int{1};
            d[i] = {a, b};
        }
        return d;
    }();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        const Int & a = p.first;
        const Int & b = p.second;
        benchmark::DoNotOptimize(a - b);
    }
}

template <typename Int>
static void Mul_WideTimesU32(benchmark::State & state)
{
    static std::array<std::pair<Int, uint32_t>, kDataN> data = []
    {
        std::array<std::pair<Int, uint32_t>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x55AA'AA55'55AA'AA55ull);
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {random_wide<Int>(rng), static_cast<uint32_t>(rng())};
        return d;
    }();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        benchmark::DoNotOptimize(p.first * p.second);
    }
}

// Two-limb divisor (highest bit at 127) exercises multi-limb division
template <typename Int>
static void Div_LargeDivisor128(benchmark::State & state)
{
    // Use a dataset to prevent constant folding
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xD128ABCDEF01ULL);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = (Int{1} << static_cast<int>(kBenchBits - 1)) + Int{rng()};
            Int b = (Int{1} << 127) + Int{rng() % 1000000};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        Int result = p.first / p.second;
        benchmark::DoNotOptimize(result);
    }
}

// Similar magnitude multi-limb division with slightly different distribution
template <typename Int>
static void Div_SimilarMagnitude2(benchmark::State & state)
{
    // Use a dataset to prevent constant folding
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xFEDCBA987654ULL);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = (Int{1} << static_cast<int>(kBenchBits - 1)) - Int{rng() % 10000000};
            int s = static_cast<int>(kBenchBits - 65 + (rng() % 30));
            Int b = (Int{1} << s) + Int{rng() % 1000000000};
            d[i] = {a, b};
        }
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        Int result = p.first / p.second;
        benchmark::DoNotOptimize(result);
    }
}

int main(int argc, char ** argv)
{
#if defined(GINT_ENABLE_CH_COMPARE) && defined(GINT_ENABLE_BOOST_COMPARE)
    if (!verify_comparison_value_domain())
        return 2;
#endif
    bool full_matrix = parse_full_matrix_flag(argc, argv);
    // Addition
    REG_CASE("Add/NoCarry", Add_NoCarry);
    REG_CASE("Add/FullCarry", Add_FullCarry);

    // Subtraction
    REG_CASE("Sub/NoBorrow", Sub_NoBorrow);
    REG_CASE("Sub/FullBorrow", Sub_FullBorrow);

    // Multiplication
    REG_CASE("Mul/U64xU64", Mul_U64xU64);
    REG_CASE("Mul/HighxHigh", Mul_HighxHigh);
    // Compare wide * u64 across libraries
    REG_CASE("Mul/WideTimesU64", Mul_WideTimesU64);

    // Division
    REG_CASE("Div/SmallDivisor32", Div_SmallDivisor32);
    REG_CASE("Div/SmallDivisor64", Div_SmallDivisor64);
    REG_CASE("Div/Pow2Divisor", Div_Pow2Divisor);
    REG_CASE("Div/SimilarMagnitude", Div_SimilarMagnitude);

    // Modulo
    REG_CASE("Mod/SmallDivisor64", Mod_SmallDivisor64);
    REG_CASE("Mod/SimilarMagnitude", Mod_SimilarMagnitude);

    // Bitwise
    REG_CASE("Bitwise/And", Bitwise_And);
    REG_CASE("Bitwise/Or", Bitwise_Or);
    REG_CASE("Bitwise/Xor", Bitwise_Xor);
    REG_CASE("Bitwise/Not", Bitwise_Not);

    // Equality
    REG_CASE("Equal/Equal", Equal_Equal);
    REG_CASE("Equal/TopDifferent", Equal_TopDifferent);

    // Shift
    REG_CASE("Shift/LeftVariable", Shift_LeftVariable);
    REG_CASE("Shift/RightVariable", Shift_RightVariable);

    // ToString
    REG_CASE("ToString/Base10", ToString);

#if !defined(GINT_ENABLE_CH_COMPARE) && !defined(GINT_ENABLE_BOOST_COMPARE)
    benchmark::RegisterBenchmark("FromString/Base2/gint", &FromString_String<WInt, 2>);
    benchmark::RegisterBenchmark("FromString/Base8/gint", &FromString_String<WInt, 8>);
    benchmark::RegisterBenchmark("FromString/Base10/gint", &FromString_String<WInt, 10>);
    benchmark::RegisterBenchmark("FromString/Base16/gint", &FromString_String<WInt, 16>);
    benchmark::RegisterBenchmark("FromStringCStr/Base2/gint", &FromString_CStr<WInt, 2>);
    benchmark::RegisterBenchmark("FromStringCStr/Base8/gint", &FromString_CStr<WInt, 8>);
    benchmark::RegisterBenchmark("FromStringCStr/Base10/gint", &FromString_CStr<WInt, 10>);
    benchmark::RegisterBenchmark("FromStringCStr/Base16/gint", &FromString_CStr<WInt, 16>);
#endif

    if (full_matrix)
    {
        // Full matrix: register additional, more granular cases
        REG_CASE("Add/CarryChain64", Add_CarryChain64);
        REG_CASE("Add/WidePlusU64", Add_WidePlusU64);
        REG_CASE("Sub/BorrowChain64", Sub_BorrowChain64);
        REG_CASE("Sub/WideMinusU64", Sub_WideMinusU64);
        REG_CASE("Mul/WideTimesU32", Mul_WideTimesU32);
        REG_CASE("Equal/LowDifferent", Equal_LowDifferent);
        REG_CASE("Div/LargeDivisor128", Div_LargeDivisor128);
        REG_CASE("Div/SimilarMagnitude2", Div_SimilarMagnitude2);
        REG_CASE("DivMod/SimilarMagnitude", DivMod_SimilarMagnitude);
#if !defined(GINT_ENABLE_CH_COMPARE) && !defined(GINT_ENABLE_BOOST_COMPARE)
        benchmark::RegisterBenchmark("DivMod/SimilarMagnitudeSeparate/gint", &DivMod_SimilarMagnitudeSeparate<WInt>);
#endif
    }

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
