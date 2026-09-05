#include <gint/gint.hpp>

#include <cstring>
#include <iostream>

#ifdef FIXTURE_ADD
#    error "fixture macro cleanup failed"
#endif

static_assert(fixture::shared == 17, "shared dependency must be emitted exactly once");
static_assert(fixture::left == 23 && fixture::right == 31, "diamond dependencies must preserve definitions");
#if FIXTURE_BRANCH == 1
static_assert(fixture::answer == 40, "first conditional branch must retain its value");
#else
static_assert(fixture::answer == 48, "else branch must retain its value");
#endif

int main()
{
    if (std::strcmp(fixture::text, "https://example.test/path") != 0)
        return 1;
    std::cout << fixture::shared << ':' << fixture::left << ':' << fixture::right << ':' << fixture::answer << ':' << fixture::text << '\n';
    return 0;
}
