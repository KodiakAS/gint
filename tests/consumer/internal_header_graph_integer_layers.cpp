#include <gint/integer_type.hpp>

static_assert(sizeof(gint::Int256) == 32, "integer_type.hpp must define the integer layout");
static_assert(gint::Int256(-7) == -7, "integer_type.hpp must support constexpr comparison");

bool exercise_integer_type()
{
    const gint::Int256 value = -7;
    const gint::Int256 truncated = -7.75;
    return (value + 2) == -5 && (2.0 - value) == 9 && truncated == value && -7.5 < value && value <= -7.0
        && static_cast<double>(value) == -7.0;
}

#include <gint/integer.hpp>

bool exercise_integer_division()
{
    const gint::UInt256 dividend = (gint::UInt256(1) << 128) + 4;
    const gint::UInt256 divisor = (gint::UInt256(1) << 64) + 1;
    const gint::UInt256 quotient = (gint::UInt256(1) << 64) - 1;
    const auto result = gint::divmod(dividend, divisor);
    return dividend / divisor == quotient && dividend % divisor == 5 && result.quotient == quotient && result.remainder == 5;
}

#include <gint/gint.hpp>

#include <sstream>

#if defined(GINT_DETAIL_CONFIG_NAMESPACE) || defined(GINT_FORCE_INLINE) || defined(GINT_CLANG_NOINLINE) || defined(GINT_CONSTEXPR14)
#    error "internal gint.hpp leaked private implementation macros"
#endif

static_assert(std::numeric_limits<gint::Int256>::digits == 255, "the full entry must provide numeric_limits");

int main()
{
    const gint::UInt256 value = 42;
    const std::hash<gint::UInt256> hash;
    std::ostringstream out;
    out << value;
    return exercise_integer_type() && exercise_integer_division() && hash(value) == hash(gint::UInt256(42)) && out.str() == "42" ? 0 : 1;
}
