#include <chrono>
#include <random>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <fmt/format.h>

#include <gint/gint.h>

using WInt = gint::integer<256, signed>;
using BInt = boost::multiprecision::int256_t;

static constexpr size_t ITERATIONS = 100000;

static uint64_t g_seed = 0;

template <typename Int, typename Op>
static long long measure(const std::vector<std::pair<Int, Int>> & data, Op op)
{
    Int c{};
    auto start = std::chrono::steady_clock::now();
    for (const auto & item : data)
    {
        Int a = item.first;
        Int b = item.second;
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        c = op(a, b);
        benchmark::DoNotOptimize(c);
    }
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

template <typename Int>
struct AddSmallGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        return {Int(dist(rng)), Int(dist(rng))};
    }
};

template <typename Int>
struct AddLargeGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = (Int{1} << 255) - Int(dist(rng));
        Int b = (Int{1} << 200) + Int(dist(rng));
        return {a, b};
    }
};

template <typename Int>
struct AddMixedGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = (Int{1} << 200) + Int(dist(rng));
        Int b = -((Int{1} << 199) + Int(dist(rng)));
        return {a, b};
    }
};

template <typename Int>
struct SubSmallGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        uint64_t x = dist(rng);
        uint64_t y = dist(rng);
        if (x < y)
            std::swap(x, y);
        return {Int(x), Int(y)};
    }
};

template <typename Int>
struct SubLargeGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = (Int{1} << 255) - Int(dist(rng));
        Int b = (Int{1} << 200) + Int(dist(rng));
        return {a, b};
    }
};

template <typename Int>
struct SubMixedGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = -((Int{1} << 200) + Int(dist(rng)));
        Int b = (Int{1} << 199) + Int(dist(rng));
        return {a, b};
    }
};

template <typename Int>
struct MulSmallGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint32_t> dist;
        return {Int(dist(rng)), Int(dist(rng))};
    }
};

template <typename Int>
struct MulLargeGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = (Int{1} << 128) + Int(dist(rng));
        Int b = (Int{1} << 120) + Int(dist(rng));
        return {a, b};
    }
};

template <typename Int>
struct MulMixedGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = -((Int{1} << 128) + Int(dist(rng)));
        Int b = (Int{1} << 120) + Int(dist(rng));
        return {a, b};
    }
};

template <typename Int>
struct DivSmallGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        uint64_t b = dist(rng) | 1;
        uint64_t a = b + dist(rng);
        return {Int(a), Int(b)};
    }
};

template <typename Int>
struct DivLargeGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = (Int{1} << 255) - Int(dist(rng));
        Int b = (Int{1} << 128) + Int(dist(rng) | 1);
        return {a, b};
    }
};

template <typename Int>
struct DivMixedGen
{
    std::pair<Int, Int> operator()(std::mt19937_64 & rng) const
    {
        std::uniform_int_distribution<uint64_t> dist;
        Int a = -((Int{1} << 255) - Int(dist(rng)));
        Int b = (Int{1} << 128) + Int(dist(rng) | 1);
        return {a, b};
    }
};

template <typename Int, template <typename> class Gen>
static std::vector<std::pair<Int, Int>> generate_inputs(uint64_t seed)
{
    std::mt19937_64 rng(seed);
    Gen<Int> gen{};
    std::vector<std::pair<Int, Int>> data;
    data.reserve(ITERATIONS);
    for (size_t i = 0; i < ITERATIONS; ++i)
        data.emplace_back(gen(rng));
    return data;
}

template <template <typename> class Gen, typename Op>
static void run_case(const std::string & name, Op op, uint64_t seed_offset)
{
    auto wide_data = generate_inputs<WInt, Gen>(g_seed + seed_offset);
    auto boost_data = generate_inputs<BInt, Gen>(g_seed + seed_offset);
    auto wide_ns = measure(wide_data, op);
    auto boost_ns = measure(boost_data, op);
    fmt::print("{}: wide={}ns boost={}ns ratio={:.2f}\n", name, wide_ns, boost_ns, static_cast<double>(wide_ns) / boost_ns);
}

int main(int argc, char ** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg.find("--seed=") == 0)
        {
            g_seed = static_cast<uint64_t>(std::stoull(arg.substr(7)));
            for (int j = i; j < argc - 1; ++j)
                argv[j] = argv[j + 1];
            --argc;
            --i;
        }
    }
    benchmark::Initialize(&argc, argv);
    fmt::print("seed={}\n", g_seed);

    run_case<AddSmallGen>("Add/Small", [](const auto & x, const auto & y) { return x + y; }, 1);
    run_case<AddLargeGen>("Add/Large", [](const auto & x, const auto & y) { return x + y; }, 2);
    run_case<AddMixedGen>("Add/Mixed", [](const auto & x, const auto & y) { return x + y; }, 3);

    run_case<SubSmallGen>("Sub/Small", [](const auto & x, const auto & y) { return x - y; }, 4);
    run_case<SubLargeGen>("Sub/Large", [](const auto & x, const auto & y) { return x - y; }, 5);
    run_case<SubMixedGen>("Sub/Mixed", [](const auto & x, const auto & y) { return x - y; }, 6);

    run_case<MulSmallGen>("Mul/Small", [](const auto & x, const auto & y) { return x * y; }, 7);
    run_case<MulLargeGen>("Mul/Large", [](const auto & x, const auto & y) { return x * y; }, 8);
    run_case<MulMixedGen>("Mul/Mixed", [](const auto & x, const auto & y) { return x * y; }, 9);

    run_case<DivSmallGen>("Div/Small", [](const auto & x, const auto & y) { return x / y; }, 10);
    run_case<DivLargeGen>("Div/Large", [](const auto & x, const auto & y) { return x / y; }, 11);
    run_case<DivMixedGen>("Div/Mixed", [](const auto & x, const auto & y) { return x / y; }, 12);

    return 0;
}
