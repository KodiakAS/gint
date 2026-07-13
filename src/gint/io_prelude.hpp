#pragma once

// Keep IO-only dependencies outside the core prelude. Unlike the generated
// distribution header, normal internal headers retain #pragma once state, so a
// core-only pass cannot rely on prelude.hpp being entered again during a later
// umbrella upgrade.
#ifndef GINT_DETAIL_CORE_ONLY
#    include <cstring>
#    include <ios>
#    include <ostream>
#    include <string>

#    ifdef GINT_ENABLE_FMT
#        include <locale>
#        include <fmt/format.h>
#    endif
#endif
