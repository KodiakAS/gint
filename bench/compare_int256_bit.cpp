#include <benchmark/benchmark.h>
#include <boost/multiprecision/cpp_int.hpp>

#include <gint/gint.h>

using WInt = gint::integer<256, signed>;
using BInt = boost::multiprecision::int256_t;

// Bitwise operations

template <typename Int>
static void BitAnd(benchmark::State & state)
{
    Int a = (Int{1} << 200) + Int{0x12345678};
    Int b = (Int{1} << 199) + Int{0x87654321};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a & b;
        benchmark::DoNotOptimize(c);
        benchmark::ClobberMemory();
        ++a;
        ++b;
    }
}

template <typename Int>
static void BitOr(benchmark::State & state)
{
    Int a = (Int{1} << 200) + Int{0x12345678};
    Int b = (Int{1} << 199) + Int{0x87654321};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a | b;
        benchmark::DoNotOptimize(c);
        benchmark::ClobberMemory();
        ++a;
        ++b;
    }
}

template <typename Int>
static void BitXor(benchmark::State & state)
{
    Int a = (Int{1} << 200) + Int{0x12345678};
    Int b = (Int{1} << 199) + Int{0x87654321};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        auto c = a ^ b;
        benchmark::DoNotOptimize(c);
        benchmark::ClobberMemory();
        ++a;
        ++b;
    }
}

// Comparison operations

template <typename Int>
static void CmpEq(benchmark::State & state)
{
    Int a = (Int{1} << 200) + Int{0x12345678};
    Int b = a;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        bool r = a == b;
        benchmark::DoNotOptimize(r);
        benchmark::ClobberMemory();
        ++a;
        ++b;
    }
}

template <typename Int>
static void CmpLt(benchmark::State & state)
{
    Int a = (Int{1} << 200) + Int{0x12345678};
    Int b = a + Int{1};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        bool r = a < b;
        benchmark::DoNotOptimize(r);
        benchmark::ClobberMemory();
        ++a;
        ++b;
    }
}

int main(int argc, char ** argv)
{
    benchmark::RegisterBenchmark("Bit/And/Wide", &BitAnd<WInt>);
    benchmark::RegisterBenchmark("Bit/And/Boost", &BitAnd<BInt>);
    benchmark::RegisterBenchmark("Bit/Or/Wide", &BitOr<WInt>);
    benchmark::RegisterBenchmark("Bit/Or/Boost", &BitOr<BInt>);
    benchmark::RegisterBenchmark("Bit/Xor/Wide", &BitXor<WInt>);
    benchmark::RegisterBenchmark("Bit/Xor/Boost", &BitXor<BInt>);

    benchmark::RegisterBenchmark("Cmp/Eq/Wide", &CmpEq<WInt>);
    benchmark::RegisterBenchmark("Cmp/Eq/Boost", &CmpEq<BInt>);
    benchmark::RegisterBenchmark("Cmp/LT/Wide", &CmpLt<WInt>);
    benchmark::RegisterBenchmark("Cmp/LT/Boost", &CmpLt<BInt>);

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
