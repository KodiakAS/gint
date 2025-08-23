#include <benchmark/benchmark.h>
#include <boost/multiprecision/cpp_int.hpp>

#include <gint/gint.h>

using WInt = gint::integer<256, signed>;
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

int main(int argc, char ** argv)
{
    benchmark::RegisterBenchmark("Add/Small/Wide", &AddSmall<WInt>);
    benchmark::RegisterBenchmark("Add/Small/Boost", &AddSmall<BInt>);
    benchmark::RegisterBenchmark("Add/Large/Wide", &AddLarge<WInt>);
    benchmark::RegisterBenchmark("Add/Large/Boost", &AddLarge<BInt>);
    benchmark::RegisterBenchmark("Add/Mixed/Wide", &AddMixed<WInt>);
    benchmark::RegisterBenchmark("Add/Mixed/Boost", &AddMixed<BInt>);

    benchmark::RegisterBenchmark("Sub/Small/Wide", &SubSmall<WInt>);
    benchmark::RegisterBenchmark("Sub/Small/Boost", &SubSmall<BInt>);
    benchmark::RegisterBenchmark("Sub/Large/Wide", &SubLarge<WInt>);
    benchmark::RegisterBenchmark("Sub/Large/Boost", &SubLarge<BInt>);
    benchmark::RegisterBenchmark("Sub/Mixed/Wide", &SubMixed<WInt>);
    benchmark::RegisterBenchmark("Sub/Mixed/Boost", &SubMixed<BInt>);

    benchmark::RegisterBenchmark("Mul/Small/Wide", &MulSmall<WInt>);
    benchmark::RegisterBenchmark("Mul/Small/Boost", &MulSmall<BInt>);
    benchmark::RegisterBenchmark("Mul/Large/Wide", &MulLarge<WInt>);
    benchmark::RegisterBenchmark("Mul/Large/Boost", &MulLarge<BInt>);
    benchmark::RegisterBenchmark("Mul/Mixed/Wide", &MulMixed<WInt>);
    benchmark::RegisterBenchmark("Mul/Mixed/Boost", &MulMixed<BInt>);

    benchmark::RegisterBenchmark("Div/Small/Wide", &DivSmall<WInt>);
    benchmark::RegisterBenchmark("Div/Small/Boost", &DivSmall<BInt>);
    benchmark::RegisterBenchmark("Div/Large/Wide", &DivLarge<WInt>);
    benchmark::RegisterBenchmark("Div/Large/Boost", &DivLarge<BInt>);
    benchmark::RegisterBenchmark("Div/Mixed/Wide", &DivMixed<WInt>);
    benchmark::RegisterBenchmark("Div/Mixed/Boost", &DivMixed<BInt>);

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
