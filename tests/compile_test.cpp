#include <gint/gint.h>
#include <gtest/gtest.h>

TEST(CompileTest, Basic)
{
    gint::integer<128, unsigned> x = 42;
    (void)x;
}
