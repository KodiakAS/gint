#include <gint/gint.h>
#include <gtest/gtest.h>

#if __cplusplus >= 201402L
constexpr gint::UInt256 gint_constexpr_assignment_check()
{
    gint::UInt256 x = 42;
    gint::UInt256 y;
    y = x; // assignment must be allowed in constexpr since C++14
    return y;
}
static_assert(gint_constexpr_assignment_check() == gint::UInt256(42), "assignment must be constexpr");

constexpr bool gint_constexpr_integral_assignment_check()
{
    gint::UInt256 unsigned_value;
    unsigned_value = static_cast<unsigned __int128>(1) << 100;
    gint::Int256 signed_value;
    signed_value = -42;
    return unsigned_value == (gint::UInt256(1) << 100) && signed_value == gint::Int256(-42);
}
static_assert(gint_constexpr_integral_assignment_check(), "integral assignment must be constexpr");
#endif

static_assert(gint::integer<256, unsigned>() == 0, "default unsigned integer should be zero");
static_assert(gint::integer<256, signed>() == 0, "default signed integer should be zero");
static_assert(sizeof(gint::integer<64, unsigned>) == 8, "integer<64> must occupy exactly 64 bits");
static_assert(sizeof(gint::integer<128, unsigned>) == 16, "integer<128> must occupy exactly 128 bits");
static_assert(sizeof(gint::integer<256, unsigned>) == 32, "integer<256> must occupy exactly 256 bits");
static_assert(sizeof(gint::integer<512, unsigned>) == 64, "integer<512> must occupy exactly 512 bits");
static_assert(sizeof(gint::integer<1024, unsigned>) == 128, "integer<1024> must occupy exactly 1024 bits");

TEST(WideIntegerConstruction, ConstexprConstruction)
{
    constexpr gint::integer<128, unsigned> a = 42;
    static_assert(a == 42, "constexpr constructor failed");
    constexpr gint::integer<128, signed> b = -42;
    static_assert(b == -42, "constexpr constructor failed");
    constexpr gint::integer<256, unsigned> c = 1;
    static_assert(c == 1, "constexpr construction failed");
    constexpr gint::integer<256, signed> d = -1;
    static_assert(d == -1, "constexpr construction failed");
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}

TEST(WideIntegerConstruction, DefaultCopyMove)
{
    using U256 = gint::integer<256, unsigned>;

    U256 a; // default construction
    EXPECT_EQ(a, U256(0));

    U256 b(a); // copy construction
    EXPECT_EQ(b, U256(0));

    U256 c(std::move(b)); // move construction
    EXPECT_EQ(c, U256(0));

    U256 d; // copy assignment
    d = c;
    EXPECT_EQ(d, U256(0));

    U256 e; // move assignment
    e = std::move(d);
    EXPECT_EQ(e, U256(0));
}
