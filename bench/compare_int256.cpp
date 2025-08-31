#include <benchmark/benchmark.h>
#ifdef GINT_ENABLE_CH_COMPARE
#    include <wide_integer/wide_integer.h>
#endif
#include <boost/multiprecision/cpp_int.hpp>

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
using BInt = boost::multiprecision::int256_t;

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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a + b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void Add_FullCarry(benchmark::State & state)
{
    // Worst-case: all bits set + 1 causes ripple across all limbs
    Int a = Int{-1};
    Int b = Int{1};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a + b;
        benchmark::DoNotOptimize(c);
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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a - b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void Sub_FullBorrow(benchmark::State & state)
{
    // Worst-case: 0 - 1 borrows across all limbs
    Int a = Int{0};
    Int b = Int{1};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a - b;
        benchmark::DoNotOptimize(c);
    }
}

// -------- Multiplication --------
template <typename Int>
static void Mul_U64xU64(benchmark::State & state)
{
    static std::array<std::pair<uint64_t, uint64_t>, kDataN> data = []
    {
        std::array<std::pair<uint64_t, uint64_t>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0xC001'D00D'BADC'0FFEuLL);
        for (size_t i = 0; i < kDataN; ++i)
            d[i] = {rng(), rng()};
        return d;
    }();

    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        Int a = Int{p.first};
        Int b = Int{p.second};
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a * b;
        benchmark::DoNotOptimize(c);
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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a * b;
        benchmark::DoNotOptimize(c);
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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
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
#    define REG_CASE(Base, Func) \
        do \
        { \
            benchmark::RegisterBenchmark(Base "/gint", &Func<WInt>); \
            benchmark::RegisterBenchmark(Base "/ClickHouse", &Func<CInt>); \
            benchmark::RegisterBenchmark(Base "/Boost", &Func<BInt>); \
        } while (false)
#else
#    define REG_CASE(Base, Func) \
        do \
        { \
            benchmark::RegisterBenchmark(Base "/gint", &Func<WInt>); \
            benchmark::RegisterBenchmark(Base "/Boost", &Func<BInt>); \
        } while (false)
#endif

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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a + b;
        benchmark::DoNotOptimize(c);
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
        Int a = p.first;
        Int b = p.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a - b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void Mul_U32xWide(benchmark::State & state)
{
    static std::array<std::pair<Int, uint32_t>, kDataN> data = []
    {
        std::array<std::pair<Int, uint32_t>, kDataN> d{};
        std::mt19937_64 rng(kSeedBase ^ 0x55AA'AA55'55AA'AA55ull);
        for (size_t i = 0; i < kDataN; ++i)
        {
            Int a = assemble_u256<Int>(rng(), rng(), rng(), rng());
            uint32_t m = static_cast<uint32_t>(rng());
            d[i] = {a, m};
        }
        return d;
    }();
    size_t i = 0;
    for (auto _ : state)
    {
        const auto & p = data[i++ & (kDataN - 1)];
        Int a = p.first;
        Int b = Int{p.second};
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a * b;
        benchmark::DoNotOptimize(c);
    }
}

// Two-limb divisor (highest bit at 127) exercises multi-limb division
template <typename Int>
static void Div_LargeDivisor128(benchmark::State & state)
{
    using U = Int;
    U a = (U{1} << 255) + U{123456789};
    U b = (U{1} << 127) + U{12345};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
    }
}

// Similar magnitude multi-limb division with slightly different distribution
template <typename Int>
static void Div_SimilarMagnitude2(benchmark::State & state)
{
    using U = Int;
    U a = (U{1} << 255) - U{7777777};
    U b = (U{1} << 191) + U{314159265};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
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

    // Division
    REG_CASE("Div/SmallDivisor32", Div_SmallDivisor32);
    REG_CASE("Div/SmallDivisor64", Div_SmallDivisor64);
    REG_CASE("Div/Pow2Divisor", Div_Pow2Divisor);
    REG_CASE("Div/SimilarMagnitude", Div_SimilarMagnitude);

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
