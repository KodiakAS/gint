#pragma once

// Arithmetic-only entry point. It keeps the exact same gint::integer type and
// numeric_limits/hash integration as <gint/gint.h>, while omitting string,
// stream, and fmt implementation parsing from translation units that do not
// use those facilities.
#define GINT_DETAIL_CORE_ONLY
#include <gint/gint.h>
#undef GINT_DETAIL_CORE_ONLY
