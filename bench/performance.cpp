#include <benchmark/benchmark.h>

#include <gint/gint.h>

template <size_t Bits>
using WInt = gint::integer<Bits, unsigned>;

template <size_t Bits>
static void BM_Addition(benchmark::State & state)
{
    WInt<Bits> a = 123456789;
    WInt<Bits> b = 987654321;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a += b);
    }
}

template <size_t Bits>
static void BM_Subtraction(benchmark::State & state)
{
    WInt<Bits> a = 987654321;
    WInt<Bits> b = 123456789;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a -= b);
    }
}

template <size_t Bits>
static void BM_Multiplication(benchmark::State & state)
{
    WInt<Bits> a = 123456789;
    WInt<Bits> b = 987654321;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a *= b);
    }
}

template <size_t Bits>
static void BM_Division(benchmark::State & state)
{
    WInt<Bits> a = 987654321;
    WInt<Bits> b = 123456;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a /= b);
    }
}

BENCHMARK_TEMPLATE(BM_Addition, 256);
BENCHMARK_TEMPLATE(BM_Addition, 512);
BENCHMARK_TEMPLATE(BM_Addition, 1024);
BENCHMARK_TEMPLATE(BM_Subtraction, 256);
BENCHMARK_TEMPLATE(BM_Subtraction, 512);
BENCHMARK_TEMPLATE(BM_Subtraction, 1024);
BENCHMARK_TEMPLATE(BM_Multiplication, 256);
BENCHMARK_TEMPLATE(BM_Multiplication, 512);
BENCHMARK_TEMPLATE(BM_Multiplication, 1024);
BENCHMARK_TEMPLATE(BM_Division, 256);
BENCHMARK_TEMPLATE(BM_Division, 512);
BENCHMARK_TEMPLATE(BM_Division, 1024);

// Division with a large, two-limb divisor (exercises Knuth D for Bits > 256)
template <size_t Bits>
static void BM_DivisionLargeDivisor128(benchmark::State & state)
{
    using U = WInt<Bits>;
    // Large dividend with top bit set
    U a = (U{1} << int(Bits - 1)) + U{123456789};
    // Two-limb divisor: highest bit at 127 ensures divisor_limbs == 2
    U b = (U{1} << 127) + U{12345};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
    }
}

// Division with a divisor of similar magnitude (multi-limb, non power-of-two)
template <size_t Bits>
static void BM_DivisionLargeSimilar(benchmark::State & state)
{
    using U = WInt<Bits>;
    U a = (U{1} << int(Bits - 1)) - U{1234567};
    U b = (U{1} << int(Bits - 64)) + U{987654321};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
    }
}

BENCHMARK_TEMPLATE(BM_DivisionLargeDivisor128, 256);
BENCHMARK_TEMPLATE(BM_DivisionLargeDivisor128, 512);
BENCHMARK_TEMPLATE(BM_DivisionLargeDivisor128, 1024);
BENCHMARK_TEMPLATE(BM_DivisionLargeSimilar, 256);
BENCHMARK_TEMPLATE(BM_DivisionLargeSimilar, 512);
BENCHMARK_TEMPLATE(BM_DivisionLargeSimilar, 1024);

template <size_t Bits>
static void BM_ToString(benchmark::State & state)
{
    WInt<Bits> a = (WInt<Bits>(1) << int(Bits - 1)) + 123456789;
    for (auto _ : state)
    {
        auto s = gint::to_string(a);
        benchmark::DoNotOptimize(s);
        a += WInt<Bits>{1};
    }
}

BENCHMARK_TEMPLATE(BM_ToString, 256);
BENCHMARK_TEMPLATE(BM_ToString, 512);
BENCHMARK_TEMPLATE(BM_ToString, 1024);

BENCHMARK_MAIN();
