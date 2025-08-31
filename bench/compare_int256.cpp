#include <benchmark/benchmark.h>
#ifdef GINT_ENABLE_CH_COMPARE
#    include <wide_integer/wide_integer.h>
#endif
#include <boost/multiprecision/cpp_int.hpp>

#include <gint/gint.h>

using WInt = gint::integer<256, signed>;
#ifdef GINT_ENABLE_CH_COMPARE
using CInt = wide::integer<256, signed>;
#endif
using BInt = boost::multiprecision::int256_t;

template <typename Int>
static void AddSmall(benchmark::State & state)
{
    Int a = 1234567890123456LL;
    Int b = 987654321098765LL;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a + b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void AddLarge(benchmark::State & state)
{
    Int a = (Int{1} << 255) - Int{1};
    Int b = (Int{1} << 200) + Int{123456789};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a + b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void AddMixed(benchmark::State & state)
{
    Int a = (Int{1} << 200);
    Int b = -((Int{1} << 199) + Int{1});
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a + b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void SubSmall(benchmark::State & state)
{
    Int a = 9876543212345678LL;
    Int b = 1234567890123456LL;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a - b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void SubLarge(benchmark::State & state)
{
    Int a = (Int{1} << 255) - Int{1};
    Int b = (Int{1} << 200) + Int{12345};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a - b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void SubMixed(benchmark::State & state)
{
    Int a = -((Int{1} << 200) + Int{123});
    Int b = (Int{1} << 199);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a - b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void MulSmall(benchmark::State & state)
{
    Int a = 123456789;
    Int b = 987654321;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a * b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void MulLarge(benchmark::State & state)
{
    Int a = (Int{1} << 128) + Int{12345};
    Int b = (Int{1} << 120) + Int{6789};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a * b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void MulMixed(benchmark::State & state)
{
    Int a = -((Int{1} << 128) + Int{1});
    Int b = (Int{1} << 120);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a * b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void DivSmall(benchmark::State & state)
{
    Int a = 9876543212345678LL;
    Int b = 123456789LL;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void DivLarge(benchmark::State & state)
{
    Int a = (Int{1} << 255) - Int{1};
    Int b = (Int{1} << 128) + Int{12345};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
    }
}

template <typename Int>
static void DivMixed(benchmark::State & state)
{
    Int a = -((Int{1} << 255) - Int{1});
    Int b = (Int{1} << 128);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a / b;
        benchmark::DoNotOptimize(c);
    }
}

// Similar-magnitude division (multi-limb, non power-of-two)
template <typename Int>
static void DivSimilar(benchmark::State & state)
{
    Int a = (Int{1} << 255) - Int{1234567};
    Int b = (Int{1} << 192) + Int{987654321};
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
    benchmark::RegisterBenchmark("Add/Small/gint", &AddSmall<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Add/Small/ClickHouse", &AddSmall<CInt>);
#endif
    benchmark::RegisterBenchmark("Add/Small/Boost", &AddSmall<BInt>);
    benchmark::RegisterBenchmark("Add/Large/gint", &AddLarge<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Add/Large/ClickHouse", &AddLarge<CInt>);
#endif
    benchmark::RegisterBenchmark("Add/Large/Boost", &AddLarge<BInt>);
    benchmark::RegisterBenchmark("Add/Mixed/gint", &AddMixed<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Add/Mixed/ClickHouse", &AddMixed<CInt>);
#endif
    benchmark::RegisterBenchmark("Add/Mixed/Boost", &AddMixed<BInt>);

    benchmark::RegisterBenchmark("Sub/Small/gint", &SubSmall<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Sub/Small/ClickHouse", &SubSmall<CInt>);
#endif
    benchmark::RegisterBenchmark("Sub/Small/Boost", &SubSmall<BInt>);
    benchmark::RegisterBenchmark("Sub/Large/gint", &SubLarge<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Sub/Large/ClickHouse", &SubLarge<CInt>);
#endif
    benchmark::RegisterBenchmark("Sub/Large/Boost", &SubLarge<BInt>);
    benchmark::RegisterBenchmark("Sub/Mixed/gint", &SubMixed<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Sub/Mixed/ClickHouse", &SubMixed<CInt>);
#endif
    benchmark::RegisterBenchmark("Sub/Mixed/Boost", &SubMixed<BInt>);

    benchmark::RegisterBenchmark("Mul/Small/gint", &MulSmall<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Mul/Small/ClickHouse", &MulSmall<CInt>);
#endif
    benchmark::RegisterBenchmark("Mul/Small/Boost", &MulSmall<BInt>);
    benchmark::RegisterBenchmark("Mul/Large/gint", &MulLarge<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Mul/Large/ClickHouse", &MulLarge<CInt>);
#endif
    benchmark::RegisterBenchmark("Mul/Large/Boost", &MulLarge<BInt>);
    benchmark::RegisterBenchmark("Mul/Mixed/gint", &MulMixed<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Mul/Mixed/ClickHouse", &MulMixed<CInt>);
#endif
    benchmark::RegisterBenchmark("Mul/Mixed/Boost", &MulMixed<BInt>);

    benchmark::RegisterBenchmark("Div/Small/gint", &DivSmall<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Div/Small/ClickHouse", &DivSmall<CInt>);
#endif
    benchmark::RegisterBenchmark("Div/Small/Boost", &DivSmall<BInt>);
    benchmark::RegisterBenchmark("Div/Large/gint", &DivLarge<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Div/Large/ClickHouse", &DivLarge<CInt>);
#endif
    benchmark::RegisterBenchmark("Div/Large/Boost", &DivLarge<BInt>);
    benchmark::RegisterBenchmark("Div/Mixed/gint", &DivMixed<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Div/Mixed/ClickHouse", &DivMixed<CInt>);
#endif
    benchmark::RegisterBenchmark("Div/Mixed/Boost", &DivMixed<BInt>);
    benchmark::RegisterBenchmark("Div/Similar/gint", &DivSimilar<WInt>);
#ifdef GINT_ENABLE_CH_COMPARE
    benchmark::RegisterBenchmark("Div/Similar/ClickHouse", &DivSimilar<CInt>);
#endif
    benchmark::RegisterBenchmark("Div/Similar/Boost", &DivSimilar<BInt>);

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
