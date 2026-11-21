#include <gint/gint.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace
{
const int width = 65536;
size_t largest_allocation = 0;
}

void * operator new(size_t size)
{
    if (size > largest_allocation)
        largest_allocation = size;
    if (void * ptr = std::malloc(size ? size : 1))
        return ptr;
    throw std::bad_alloc();
}

void operator delete(void * ptr) noexcept
{
    std::free(ptr);
}

namespace
{
bool check_padding(const char * spec, const char * expected, size_t expected_size)
{
    char buffer[8];
    largest_allocation = 0;
    const auto result = fmt::format_to_n(buffer, sizeof(buffer), fmt::runtime(spec), gint::Int128(-42), width);
    if (result.out == buffer + sizeof(buffer) && result.size == expected_size && std::memcmp(buffer, expected, sizeof(buffer)) == 0
        && largest_allocation < static_cast<size_t>(width))
        return true;
    std::fprintf(stderr, "%s: output size %zu, largest allocation %zu bytes\n", spec, static_cast<size_t>(result.size), largest_allocation);
    return false;
}
}

int main()
{
    return check_padding("{:0{}}", "-0000000", width) && check_padding("{:界^{}}", "界界界", 3 * (width - 3) + 3) ? 0 : 1;
}
