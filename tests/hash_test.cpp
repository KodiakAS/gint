#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(WideIntegerHash, SupportsUnorderedSet)
{
    using U128 = gint::integer<128, unsigned>;
    U128 a = U128(1) << 100;
    U128 b = a + U128(7);

    std::unordered_set<U128> values;
    values.insert(a);
    values.insert(b);

    EXPECT_EQ(values.count(a), 1u);
    EXPECT_EQ(values.count(b), 1u);
    EXPECT_EQ(values.count(U128(3)), 0u);
}

TEST(WideIntegerHash, SupportsUnorderedMap)
{
    using S256 = gint::integer<256, signed>;
    S256 key = -(S256(1) << 200);
    key += S256(42);

    std::unordered_map<S256, int> values;
    values[key] = 17;

    EXPECT_EQ(values.find(key)->second, 17);
    EXPECT_EQ(values.count(S256(-42)), 0u);
}

TEST(WideIntegerHash, EqualValuesHaveEqualHashes)
{
    using U256 = gint::integer<256, unsigned>;
    U256 lhs = (U256(1) << 180) + U256(123);
    U256 rhs = lhs;

    std::hash<U256> hasher;
    EXPECT_EQ(hasher(lhs), hasher(rhs));
}

static_assert(std::is_default_constructible<std::hash<gint::UInt128>>::value, "std::hash<UInt128> should be default constructible");
static_assert(
    noexcept(std::declval<std::hash<gint::UInt128> &>()(std::declval<const gint::UInt128 &>())), "std::hash<UInt128> should be noexcept");
