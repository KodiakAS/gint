#pragma once

#include "core.hpp"

#if !defined(GINT_DETAIL_CORE_ONLY) && !defined(GINT_DETAIL_IO_DEFINITIONS_INCLUDED)
#    ifdef GINT_DETAIL_IO_PASS_IN_PROGRESS
#        error "recursive gint IO definition pass"
#    endif
#    define GINT_DETAIL_IO_PASS_IN_PROGRESS
#endif

#include "fmt.hpp"

#if !defined(GINT_DETAIL_CORE_ONLY) && !defined(GINT_DETAIL_IO_DEFINITIONS_INCLUDED)
#    define GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    undef GINT_DETAIL_IO_PASS_IN_PROGRESS
#endif
