#include <typeinfo>

extern "C" int gint_checked_zero_division();
extern "C" int gint_unchecked_zero_division();
extern "C" int gint_gcc_tuned_arithmetic();
extern "C" const std::type_info * gint_checked_type_info();
extern "C" const std::type_info * gint_unchecked_type_info();
extern "C" const std::type_info * gint_gcc_tuned_type_info();

int main()
{
    if (gint_checked_zero_division() != 1 || gint_unchecked_zero_division() != 1 || gint_gcc_tuned_arithmetic() != 1)
        return 1;
    if (*gint_checked_type_info() == *gint_unchecked_type_info())
        return 2;
    if (*gint_unchecked_type_info() == *gint_gcc_tuned_type_info())
        return 3;
    return 0;
}
