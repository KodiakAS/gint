#include <array>
#include <random>
#include <string>

#include <benchmark/benchmark.h>
#include <gint/gint.h>

namespace
{
template <typename Int>
struct OrdinaryDivisor
{
    explicit OrdinaryDivisor(const Int & value)
        : divisor(value)
    {
    }

    gint::divmod_result<Int> divmod(const Int & value) const { return gint::divmod(value, divisor); }

    Int divisor;
};

// Compile the same harness against a revision before prepared_divisor existed.
template <typename Int>
using PreparedDivisor =
#ifdef GINT_BENCH_PREPARED_BASELINE
    OrdinaryDivisor<Int>;
#else
    gint::prepared_divisor<Int>;
#endif

gint::UInt256 powerOfTen(unsigned exponent)
{
    gint::UInt256 value(1);
    while (exponent--)
        value *= gint::UInt256(10);
    return value;
}

template <bool Round, typename Int, typename Divider>
void consume(const Divider & divider, const Int & value, const Int & divisor)
{
    auto result = divider.divmod(value);
    if (Round)
    {
        const Int magnitude = result.remainder < Int(0) ? -result.remainder : result.remainder;
        // The chosen scales are at most 38, so doubling the remainder cannot wrap.
        if (magnitude * uint64_t(2) >= divisor)
            result.quotient += value < Int(0) ? Int(-1) : Int(1);
        benchmark::DoNotOptimize(result.quotient);
    }
    else
    {
        benchmark::DoNotOptimize(result.quotient);
        benchmark::DoNotOptimize(result.remainder);
    }
}

template <typename Int, typename Divider, bool Round, unsigned Batch, bool Setup>
void run(benchmark::State & state, unsigned digits, unsigned scale, unsigned signs)
{
    std::array<Int, 256> data;
    const Int divisor(powerOfTen(scale));
    const gint::UInt256 limit = powerOfTen(digits);
    std::mt19937_64 random(0x49d1361 + digits * 73 + scale * 19);
    for (size_t i = 0; i < data.size(); ++i)
    {
        gint::UInt256 value;
        for (unsigned limb = 0; limb < 4; ++limb)
            value |= gint::UInt256(random()) << (limb * 64);
        data[i] = Int(value % limit);
        if (signs == 1 || (signs == 2 && i % 2))
            data[i] = -data[i];
    }

    const Divider prepared(divisor);
    size_t index = 0;
    for (auto _ : state)
    {
        if (Setup)
        {
            Int current_divisor(divisor);
            // Keep construction inside the timed batch instead of hoisting it.
            benchmark::DoNotOptimize(current_divisor);
            const Divider current(current_divisor);
            for (unsigned j = 0; j < Batch; ++j)
                consume<Round>(current, data[index++ & 255], divisor);
        }
        else
        {
            for (unsigned j = 0; j < Batch; ++j)
                consume<Round>(prepared, data[index++ & 255], divisor);
        }
    }
    state.SetItemsProcessed(state.iterations() * Batch);
}

template <typename Int, typename Divider, bool Round, unsigned Batch, bool Setup>
void add(const char * kind, const char * operation, unsigned digits, unsigned scale, unsigned sign)
{
    const char * signs[] = {"Positive", "Negative", "Mixed", "Unsigned"};
    const std::string label = std::string("PreparedDivisor/") + kind + "/" + operation + "/P" + std::to_string(digits) + "/S"
        + std::to_string(scale) + "/" + signs[sign];
    benchmark::RegisterBenchmark(
        label.c_str(), [=](benchmark::State & state) { run<Int, Divider, Round, Batch, Setup>(state, digits, scale, sign); });
}

template <typename Int, typename Divider>
void addOperations(const char * kind, unsigned digits, unsigned scale, unsigned sign)
{
    add<Int, Divider, false, 1, false>(kind, "DivMod", digits, scale, sign);
    add<Int, Divider, true, 1, false>(kind, "Round", digits, scale, sign);
    add<Int, Divider, false, 1, true>(kind, "Setup1", digits, scale, sign);
    add<Int, Divider, false, 16, true>(kind, "Setup16", digits, scale, sign);
    add<Int, Divider, false, 256, true>(kind, "Setup256", digits, scale, sign);
}
}

int main(int argc, char ** argv)
{
    for (unsigned digits : {18u, 38u, 65u})
        for (unsigned scale : {0u, 2u, 19u, 20u, 30u, 38u})
        {
            for (unsigned sign = 0; sign < 3; ++sign)
            {
                addOperations<gint::Int256, OrdinaryDivisor<gint::Int256>>("Ordinary", digits, scale, sign);
                addOperations<gint::Int256, PreparedDivisor<gint::Int256>>("Prepared", digits, scale, sign);
            }
            addOperations<gint::UInt256, OrdinaryDivisor<gint::UInt256>>("Ordinary", digits, scale, 3);
            addOperations<gint::UInt256, PreparedDivisor<gint::UInt256>>("Prepared", digits, scale, 3);
        }
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return 1;
    benchmark::RunSpecifiedBenchmarks();
}
