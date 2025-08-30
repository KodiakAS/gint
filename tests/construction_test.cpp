#include <gint/gint.h>
#include <gtest/gtest.h>

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
