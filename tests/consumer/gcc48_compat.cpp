#include <gint/gint.h>

#include <limits>
#include <sstream>
#include <string>

template <size_t Bits>
bool check_unsigned_arithmetic()
{
    typedef gint::integer<Bits, unsigned> UInt;

    const UInt high = UInt(1) << (Bits - 1);
    const UInt dividend = high | (UInt(1) << (Bits / 2)) | UInt(0x123456789ULL);
    const UInt divisor = (UInt(1) << (Bits / 2)) | UInt(97);
    const gint::divmod_result<UInt> result = gint::divmod(dividend, divisor);

    if (result.quotient * divisor + result.remainder != dividend || result.remainder >= divisor)
        return false;
    if (((dividend + divisor) - divisor) != dividend)
        return false;
    if (((dividend ^ divisor) ^ divisor) != dividend)
        return false;
    if ((std::numeric_limits<UInt>::max() + UInt(1)) != UInt(0))
        return false;

    return true;
}

template <size_t Bits>
bool check_signed_arithmetic()
{
    typedef gint::integer<Bits, signed> Int;

    const Int dividend = -(Int(1) << (Bits - 2)) + Int(123456789);
    const Int divisor = -Int(65537);
    const gint::divmod_result<Int> result = gint::divmod(dividend, divisor);
    return result.quotient * divisor + result.remainder == dividend && Int(-42) < Int(0);
}

bool check_conversions()
{
    const gint::UInt256 parsed = gint::from_string<gint::UInt256>("123456789012345678901234567890");
    if (gint::to_string(parsed) != "123456789012345678901234567890")
        return false;

    std::ostringstream stream;
    stream << parsed;
    if (stream.str() != gint::to_string(parsed))
        return false;

    const gint::Int128 from_float = -12345.75;
    if (from_float != gint::Int128(-12345))
        return false;
    return static_cast<double>(gint::UInt128(12345)) == 12345.0;
}

int main()
{
    const bool unsigned_ok = check_unsigned_arithmetic<64>() && check_unsigned_arithmetic<128>() && check_unsigned_arithmetic<256>()
        && check_unsigned_arithmetic<512>() && check_unsigned_arithmetic<1024>();
    const bool signed_ok = check_signed_arithmetic<128>() && check_signed_arithmetic<256>() && check_signed_arithmetic<1024>();
    return unsigned_ok && signed_ok && check_conversions() ? 0 : 1;
}
