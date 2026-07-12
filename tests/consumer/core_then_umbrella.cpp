#include <gint/core.h>
#include <gint/gint.h>

#include <sstream>
#include <string>

int main()
{
    const gint::UInt256 value = gint::from_string<gint::UInt256>("12345678901234567890");
    std::ostringstream out;
    out << value;
    return out.str() == gint::to_string(value) ? 0 : 1;
}
