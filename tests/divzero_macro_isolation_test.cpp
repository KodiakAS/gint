#include <gtest/gtest.h>

int gint_divzero_macro_enabled_tu_anchor();
bool gint_unchecked_zero_division_throws();

TEST(WideIntegerDivZeroMacro, EnabledTranslationUnitDoesNotAffectUncheckedTranslationUnit)
{
    EXPECT_EQ(gint_divzero_macro_enabled_tu_anchor(), 1);
    EXPECT_FALSE(gint_unchecked_zero_division_throws());
}
