#pragma once

#if __cplusplus >= 202002L
#    include <bit>
#else
#    include <cstdint>
namespace std
{
enum class endian
{
#    if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
#        if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    native = little
#        else
    native = big
#        endif
#    else
    little = 0,
    big = 1,
#        ifdef _WIN32
    native = little
#        else
    native = little
#        endif
#    endif
};
} // namespace std
#endif
