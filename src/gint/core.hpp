#pragma once

#include "prelude.hpp"

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    ifdef GINT_DETAIL_CORE_PASS_IN_PROGRESS
#        error "recursive gint core definition pass"
#    endif
#    define GINT_DETAIL_CORE_PASS_IN_PROGRESS
#endif

#include "standard.hpp"

#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    define GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    undef GINT_DETAIL_CORE_PASS_IN_PROGRESS
#endif

// End the core definition pass before a possible IO upgrade.
#include "cleanup_pass.hpp"
