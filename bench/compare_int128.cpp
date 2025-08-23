#include <benchmark/benchmark.h>
#include <boost/multiprecision/cpp_int.hpp>

#include <gint/gint.h>

using WInt128 = gint::integer<128, unsigned>;
using BInt128 = boost::multiprecision::uint128_t;

static void BM_Wide128Addition(benchmark::State & state)
{
    WInt128 a = 123456789;
    WInt128 b = 987654321;
    for (auto _ : state)
    {
        WInt128 c = a;
        for (int i = 0; i < 1000; ++i)
        {
            c += b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Wide128Subtraction(benchmark::State & state)
{
    WInt128 a = 1234567890;
    WInt128 b = 987654321;
    for (auto _ : state)
    {
        WInt128 c = a;
        for (int i = 0; i < 1000; ++i)
        {
            c -= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Builtin128Addition(benchmark::State & state)
{
    unsigned __int128 a = 123456789;
    unsigned __int128 b = 987654321;
    for (auto _ : state)
    {
        unsigned __int128 c = a;
        for (int i = 0; i < 1000; ++i)
        {
            c += b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Builtin128Subtraction(benchmark::State & state)
{
    unsigned __int128 a = 1234567890;
    unsigned __int128 b = 987654321;
    for (auto _ : state)
    {
        unsigned __int128 c = a;
        for (int i = 0; i < 1000; ++i)
        {
            c -= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Wide128Multiplication(benchmark::State & state)
{
    WInt128 a = 123456789;
    WInt128 b = 987654321;
    for (auto _ : state)
    {
        WInt128 c = a;
        for (int i = 0; i < 100; ++i)
        {
            c *= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Wide128Division(benchmark::State & state)
{
    WInt128 a = 1234567890123456789ULL;
    WInt128 b = 987654321;
    for (auto _ : state)
    {
        WInt128 c = a;
        for (int i = 0; i < 100; ++i)
        {
            c /= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Builtin128Multiplication(benchmark::State & state)
{
    unsigned __int128 a = 123456789;
    unsigned __int128 b = 987654321;
    for (auto _ : state)
    {
        unsigned __int128 c = a;
        for (int i = 0; i < 100; ++i)
        {
            c *= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Builtin128Division(benchmark::State & state)
{
    unsigned __int128 a = static_cast<unsigned __int128>(1234567890123456789ULL) << 64;
    unsigned __int128 b = 987654321;
    for (auto _ : state)
    {
        unsigned __int128 c = a;
        for (int i = 0; i < 100; ++i)
        {
            c /= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Boost128Addition(benchmark::State & state)
{
    BInt128 a = 123456789;
    BInt128 b = 987654321;
    for (auto _ : state)
    {
        BInt128 c = a;
        for (int i = 0; i < 1000; ++i)
        {
            c += b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Boost128Subtraction(benchmark::State & state)
{
    BInt128 a = 1234567890;
    BInt128 b = 987654321;
    for (auto _ : state)
    {
        BInt128 c = a;
        for (int i = 0; i < 1000; ++i)
        {
            c -= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Boost128Multiplication(benchmark::State & state)
{
    BInt128 a = 123456789;
    BInt128 b = 987654321;
    for (auto _ : state)
    {
        BInt128 c = a;
        for (int i = 0; i < 100; ++i)
        {
            c *= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

static void BM_Boost128Division(benchmark::State & state)
{
    BInt128 a = 1234567890123456789ULL;
    BInt128 b = 987654321;
    for (auto _ : state)
    {
        BInt128 c = a;
        for (int i = 0; i < 100; ++i)
        {
            c /= b;
            benchmark::DoNotOptimize(c);
        }
    }
}

BENCHMARK(BM_Wide128Addition);
BENCHMARK(BM_Wide128Subtraction);
BENCHMARK(BM_Builtin128Addition);
BENCHMARK(BM_Builtin128Subtraction);
BENCHMARK(BM_Wide128Multiplication);
BENCHMARK(BM_Wide128Division);
BENCHMARK(BM_Builtin128Multiplication);
BENCHMARK(BM_Builtin128Division);
BENCHMARK(BM_Boost128Addition);
BENCHMARK(BM_Boost128Subtraction);
BENCHMARK(BM_Boost128Multiplication);
BENCHMARK(BM_Boost128Division);

BENCHMARK_MAIN();
