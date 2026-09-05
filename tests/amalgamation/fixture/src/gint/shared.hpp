#pragma once

namespace fixture
{
constexpr unsigned shared = 17;
}

// clang-format off
#define FIXTURE_ADD(a, b) \
    ((a) + (b))
// clang-format on
