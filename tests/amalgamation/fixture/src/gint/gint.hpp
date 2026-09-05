#pragma once

#include "left.hpp"
#include "right.hpp"
#include "shared.hpp"

namespace fixture
{
// Exercise directive token boundaries independently of whitespace formatting.
// clang-format off
#if(FIXTURE_BRANCH == 1)
constexpr unsigned selected = left;
#elif!defined(FIXTURE_BRANCH)// optional configuration
constexpr unsigned selected = shared;
#else// configured zero
constexpr unsigned selected = right;
#endif// selection
// clang-format on

constexpr unsigned answer = FIXTURE_ADD(selected, shared);
constexpr const char * text = "https://example.test/path";
}

#undef FIXTURE_ADD
