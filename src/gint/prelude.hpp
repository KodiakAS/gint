#pragma once

#if defined(_MSC_VER)
#    error "gint does not support MSVC or clang-cl; use GCC or Clang with the GNU ABI"
#elif !defined(__GNUC__) && !defined(__clang__)
#    error "gint requires GCC, Clang, or AppleClang"
#endif

#if defined(__GNUC__) && !defined(__clang__)
#    if __GNUC__ < 4
#        error "gint requires GCC 4.8.5 or later"
#    elif __GNUC__ == 4 && (__GNUC_MINOR__ < 8 || (__GNUC_MINOR__ == 8 && __GNUC_PATCHLEVEL__ < 5))
#        error "gint requires GCC 4.8.5 or later"
#    endif
#endif

#if __cplusplus < 201103L
#    error "gint requires C++11 or later"
#endif

#if !defined(__SIZEOF_INT128__)
#    error "gint requires compiler support for __int128"
#endif

#define GINT_VERSION_MAJOR 0
#define GINT_VERSION_MINOR 9
#define GINT_VERSION_PATCH 0
#define GINT_VERSION (GINT_VERSION_MAJOR * 10000 + GINT_VERSION_MINOR * 100 + GINT_VERSION_PATCH)

#include <array>
#include <cassert>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <type_traits>

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#    include <x86intrin.h>
#endif
