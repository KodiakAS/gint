#include <dlfcn.h>

#include <gtest/gtest.h>

#ifndef GINT_DSO_CHECKED_PATH
#    error "GINT_DSO_CHECKED_PATH must be defined"
#endif

#ifndef GINT_DSO_UNCHECKED_PATH
#    error "GINT_DSO_UNCHECKED_PATH must be defined"
#endif

namespace
{
using DivzeroResultFn = int (*)();

void * open_dso(const char * path, int flags)
{
    dlerror();
    void * handle = dlopen(path, flags);
    return handle;
}

DivzeroResultFn load_result_fn(void * handle)
{
    dlerror();
    void * symbol = dlsym(handle, "gint_divzero_min_divzero_result");
    return reinterpret_cast<DivzeroResultFn>(symbol);
}
} // namespace

TEST(WideIntegerDivzeroDso, CheckedDsoDoesNotPolluteUncheckedDso)
{
    void * checked = open_dso(GINT_DSO_CHECKED_PATH, RTLD_NOW | RTLD_GLOBAL);
    ASSERT_NE(checked, nullptr) << dlerror();
    DivzeroResultFn checked_fn = load_result_fn(checked);
    ASSERT_NE(checked_fn, nullptr) << dlerror();
    EXPECT_EQ(checked_fn(), 1);

    void * unchecked = open_dso(GINT_DSO_UNCHECKED_PATH, RTLD_NOW | RTLD_LOCAL);
    ASSERT_NE(unchecked, nullptr) << dlerror();
    DivzeroResultFn unchecked_fn = load_result_fn(unchecked);
    ASSERT_NE(unchecked_fn, nullptr) << dlerror();
    EXPECT_EQ(unchecked_fn(), 0);

    dlclose(unchecked);
    dlclose(checked);
}
