#include "../differential/oracle.h"

#include <cstdio>
#include <cstdlib>
#include <exception>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    try
    {
        gint_differential::exercise_input(data, size);
    }
    catch (const std::exception & error)
    {
        std::fprintf(stderr, "gint differential oracle failure: %s\n", error.what());
        std::abort();
    }
    return 0;
}
