#include <benchmark/benchmark.h>
#ifdef GINT_ENABLE_CH_COMPARE
#    include <wide_integer/wide_integer.h>
#endif
#ifdef GINT_ENABLE_BOOST_COMPARE
#    include <boost/multiprecision/cpp_int.hpp>
#endif

#include <array>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include <gint/gint.h>

using WInt = gint::integer<256, signed>;
#ifdef GINT_ENABLE_CH_COMPARE
using CInt = wide::integer<256, signed>;
#endif
#ifdef GINT_ENABLE_BOOST_COMPARE
using BInt = boost::multiprecision::int256_t;
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

template <typename Int>
inline Int assemble_u256(uint64_t w0, uint64_t w1, uint64_t w2, uint64_t w3)
{
    Int x = Int{0};
    x |= Int{w0};
    x |= (Int{w1} << 64);
    x |= (Int{w2} << 128);
    x |= (Int{w3} << 192);
    return x;
}

template <typename Int>
inline Int random_u256_clear_msb(std::mt19937_64 & rng)
{
    uint64_t w0 = rng();
    uint64_t w1 = rng();
    uint64_t w2 = rng();
    uint64_t w3 = rng() & 0x7FFF'FFFF'FFFF'FFFFull; // mask out the top bit to keep the value in a tighter range
    return assemble_u256<Int>(w0, w1, w2, w3);
}

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
            // a: random 256-bit; b: small 32-bit, avoids multi-limb carry in most cases
            uint64_t w0 = rng();
            uint64_t w1 = rng();
            uint64_t w2 = rng();
            uint64_t w3 = rng();
            Int a = assemble_u256<Int>(w0, w1, w2, w3);
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

// -------- Mixed-operand: wide * u64 (compare) --------
// Use fully random 256-bit 'a' to avoid accidentally benchmarking a degenerate one-limb case.
template <typename Int>
static void Mul_WideTimesU64(benchmark::State & state)
{
    static std::array<std::pair<Int, uint64_t>, kDataN> data = []
    {
        std::array<std::pair<Int, uint64_t>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x13579BDF'2468'ACE0ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            // assemble fully random 256-bit value
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
    // Use varied data to prevent constant folding while maintaining worst-case semantics
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xFCA12345678ULL);
        for (size_t i = 0; i < kDataN; ++i)
        {
            // All 1s with occasional random high bits to prevent optimization
            Int a = Int{-1};
            if (i % 8 == 0)
                a ^= (Int{rng()} << 192);
            d[i] = {a, Int{1}};
        }
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
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
static void Sub_FullBorrow(benchmark::State & state)
{
    // Worst-case: 0 - 1 borrows across all limbs
    // Use varied data to prevent constant folding while maintaining worst-case semantics
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xFB98765432ULL);
        for (size_t i = 0; i < kDataN; ++i)
        {
            // Zero with occasional random high bits to prevent optimization
            Int a = Int{0};
            if (i % 8 == 0)
                a ^= (Int{rng()} << 192);
            d[i] = {a, Int{1}};
        }
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
            Int a = (Int{1} << 200) | assemble_u256<Int>(rng(), rng(), rng(), 0);
            Int b = (Int{1} << 180) | assemble_u256<Int>(rng(), rng(), rng(), 0);
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
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
        std::uniform_int_distribution<int> dist_k(1, 200);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
        std::uniform_int_distribution<int> dist_shift(180, 220);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = (Int{1} << 255) - Int{uint32_t(rng())};
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
            // Generate numbers of different magnitudes (128-255 bits)
            int shift = 128 + static_cast<int>(rng() % 128);
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
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
            Int b = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
            Int b = assemble_u256<Int>(rng(), rng(), rng(), rng());
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

// -------- Shift --------
template <typename Int>
static void Shift_LeftVariable(benchmark::State & state)
{
    static std::array<std::pair<Int, unsigned>, kDataN> data = []
    {
        std::array<std::pair<Int, unsigned>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x12345678ABCDEF01ull);
        std::uniform_int_distribution<unsigned> dist(1, 255);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
        std::uniform_int_distribution<unsigned> dist(1, 255);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
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
            Int a = random_u256_clear_msb<Int>(rng);
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
        std::uniform_int_distribution<int> dist_shift(180, 220);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = (Int{1} << 255) - Int{uint32_t(rng())};
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
            Int a = assemble_u256<Int>(~0ULL, rng(), rng(), rng()); // low limb all 1s
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
            Int a = assemble_u256<Int>(0, rng(), rng(), rng()); // low limb zero
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
static void Mul_U32xWide(benchmark::State & state)
{
    static std::array<std::pair<Int, Int>, kDataN> data = []
    {
        std::array<std::pair<Int, Int>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x55AA'AA55'55AA'AA55ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
            Int b = Int{static_cast<uint32_t>(rng())};
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
            Int a = (Int{1} << 255) + Int{rng()};
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
            Int a = (Int{1} << 255) - Int{rng() % 10000000};
            int s = 191 + static_cast<int>(rng() % 30); // 191-220
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
    REG_CASE("Bitwise/Xor", Bitwise_Xor);

    // Shift
    REG_CASE("Shift/LeftVariable", Shift_LeftVariable);
    REG_CASE("Shift/RightVariable", Shift_RightVariable);

    // ToString
    REG_CASE("ToString/Base10", ToString);

    if (full_matrix)
    {
        // Full matrix: register additional, more granular cases
        REG_CASE("Add/CarryChain64", Add_CarryChain64);
        REG_CASE("Sub/BorrowChain64", Sub_BorrowChain64);
        REG_CASE("Mul/U32xWide", Mul_U32xWide);
        REG_CASE("Div/LargeDivisor128", Div_LargeDivisor128);
        REG_CASE("Div/SimilarMagnitude2", Div_SimilarMagnitude2);
    }

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
