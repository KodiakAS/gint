#pragma once

#include "string_stream.hpp"

// Optional fmt formatter, built on the shared integer-to-text conversion helpers.

#ifdef GINT_ENABLE_FMT
#    include <algorithm>
#    include <locale>
#    include <fmt/format.h>

namespace fmt
{
template <size_t Bits, typename Signed>
struct formatter<gint::integer<Bits, Signed>>
{
    char fill[4] = {' ', 0, 0, 0};
    unsigned fill_size = 1;
    char align = '>';
    bool explicit_align = false;
    char sign = 0;
    bool alternate = false;
    unsigned width = 0;
    int dynamic_width_arg_id = -1;
    basic_string_view<char> dynamic_width_arg_name;
    bool dynamic_width_is_named = false;
    bool localized = false;
    char presentation = 0;
#    if FMT_VERSION >= 100000 && (!defined(FMT_USE_LOCALE) || FMT_USE_LOCALE)
    basic_string_view<char> localized_specs;
#    endif

    static FMT_CONSTEXPR bool is_name_start(char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'; }

    static FMT_CONSTEXPR bool is_name_char(char ch) { return is_name_start(ch) || (ch >= '0' && ch <= '9'); }

    template <typename Iterator>
    static FMT_CONSTEXPR unsigned parse_decimal(Iterator & it, Iterator end, unsigned value)
    {
        while (it != end && *it >= '0' && *it <= '9')
        {
            const unsigned digit = static_cast<unsigned>(*it - '0');
            if (value > (static_cast<unsigned>((std::numeric_limits<int>::max)()) - digit) / 10u)
                GINT_THROW(fmt::format_error("number is too big"));
            value = value * 10u + digit;
            ++it;
        }
        return value;
    }

    static FMT_CONSTEXPR unsigned utf8_fill_size(const char * it, const char * end)
    {
        const unsigned char lead = static_cast<unsigned char>(*it);
        if (lead < 0x80)
            return 1;
        if (lead < 0xc2 || lead > 0xf4)
            return 0;

        const unsigned size = lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (end - it < size)
            return 0;
        for (unsigned i = 1; i < size; ++i)
        {
            const unsigned char unit = static_cast<unsigned char>(it[i]);
            if (unit < 0x80 || unit > 0xbf)
                return 0;
        }

        // These second-byte bounds exclude overlong encodings, surrogates, and values above U+10FFFF.
        const unsigned char second = static_cast<unsigned char>(it[1]);
        if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second > 0x9f) || (lead == 0xf0 && second < 0x90)
            || (lead == 0xf4 && second > 0x8f))
            return 0;
        return size;
    }

    template <typename OutputIt>
    OutputIt write_fill(OutputIt out, size_t count) const
    {
        if (fill_size == 1)
        {
            // fmt 11.0.0's appender does not provide a standard iterator category.
            for (size_t i = 0; i < count; ++i)
                *out++ = fill[0];
            return out;
        }
        for (size_t i = 0; i < count; ++i)
            out = std::copy_n(fill, fill_size, out);
        return out;
    }

    template <typename ParseContext>
    FMT_CONSTEXPR auto parse(ParseContext & ctx) -> typename ParseContext::iterator
    {
        auto it = ctx.begin();
        const auto end = ctx.end();
        if (it == end || *it == '}')
            return it;

        const unsigned code_units = utf8_fill_size(it, end);
        if (code_units == 0)
            GINT_THROW(fmt::format_error("invalid fill character"));
        const auto next = it + code_units;
        if (next != end && (*next == '<' || *next == '>' || *next == '^'))
        {
            if (*it == '{' || *it == '}')
                GINT_THROW(fmt::format_error("invalid fill character"));
            fill_size = code_units;
            for (unsigned i = 0; i < fill_size; ++i, ++it)
                fill[i] = *it;
            align = *next;
            explicit_align = true;
            it = next;
            ++it;
        }
        else if (*it == '<' || *it == '>' || *it == '^')
        {
            align = *it;
            explicit_align = true;
            ++it;
        }

        if (it != end && (*it == '+' || *it == '-' || *it == ' '))
        {
            if (std::is_same<Signed, unsigned>::value)
                GINT_THROW(fmt::format_error("invalid format specifier for gint::integer"));
            sign = *it;
            ++it;
        }

        if (it != end && *it == '#')
        {
            alternate = true;
            ++it;
        }

        if (it != end && *it == '0')
        {
            if (align == '>' && !explicit_align)
            {
                align = '=';
                fill[0] = '0';
                fill_size = 1;
            }
#    if FMT_VERSION < 100000
            else
            {
                // fmt 9 overwrites only the first code unit, retaining fill size.
                fill[0] = '0';
            }
#    endif
            ++it;
#    if FMT_VERSION >= 100000
            if (it != end && *it == '0')
                GINT_THROW(fmt::format_error("invalid format specifier for gint::integer"));
#    endif
        }

        if (it != end && *it == '{')
        {
            ++it;
            if (it != end && *it == '}')
            {
                dynamic_width_arg_id = ctx.next_arg_id();
#    if FMT_VERSION >= 90100
                ctx.check_dynamic_spec(dynamic_width_arg_id);
#    endif
                ++it;
            }
            else if (it != end && is_name_start(*it))
            {
                const auto name_begin = it;
                ++it;
                while (it != end && is_name_char(*it))
                    ++it;
                if (it == end || *it != '}')
                    GINT_THROW(fmt::format_error("invalid format specifier for gint::integer"));
                dynamic_width_arg_name = basic_string_view<char>(&*name_begin, static_cast<size_t>(it - name_begin));
                dynamic_width_is_named = true;
                ctx.check_arg_id(dynamic_width_arg_name);
                ++it;
            }
            else
            {
                if (it != end && *it == '0' && it + 1 != end && it[1] != '}')
                    GINT_THROW(fmt::format_error("invalid argument index"));
                const auto id_begin = it;
                const int arg_id = static_cast<int>(parse_decimal(it, end, 0));
                if (it == id_begin || it == end || *it != '}')
                    GINT_THROW(fmt::format_error("invalid format specifier for gint::integer"));
                ctx.check_arg_id(arg_id);
#    if FMT_VERSION >= 90100
                ctx.check_dynamic_spec(arg_id);
#    endif
                dynamic_width_arg_id = arg_id;
                ++it;
            }
        }
        else
        {
#    if FMT_VERSION < 100000
            const auto width_begin = it;
#    endif
            width = parse_decimal(it, end, width);
#    if FMT_VERSION < 100000
            // fmt 9 limits the width field's digits after consuming the zero flag.
            if (it - width_begin > std::numeric_limits<int>::digits10 + 1)
                GINT_THROW(fmt::format_error("number is too big"));
#    endif
        }

        if (it != end && *it == 'L')
        {
            localized = true;
            ++it;
        }

        if (it != end && *it != '}')
        {
            presentation = *it;
            if (presentation != 'd' && presentation != 'x' && presentation != 'X' && presentation != 'o' && presentation != 'b'
                && presentation != 'B' && presentation != 'c')
                GINT_THROW(fmt::format_error("invalid format specifier for gint::integer"));
            ++it;
        }

        if (it != end && *it != '}')
            GINT_THROW(fmt::format_error("invalid format specifier for gint::integer"));
#    if FMT_VERSION >= 100000 && (!defined(FMT_USE_LOCALE) || FMT_USE_LOCALE)
        if (localized)
            localized_specs = basic_string_view<char>(ctx.begin(), static_cast<size_t>(it - ctx.begin()));
#    endif
        return it;
    }

    struct dynamic_width_visitor
    {
        unsigned operator()(int value) const { return from_signed(value); }
        unsigned operator()(long value) const { return from_signed(value); }
        unsigned operator()(long long value) const { return from_signed(value); }
        unsigned operator()(unsigned value) const { return checked_width(value); }
        unsigned operator()(unsigned long value) const { return checked_width(value); }
        unsigned operator()(unsigned long long value) const { return checked_width(value); }

#    if FMT_USE_INT128
        unsigned operator()(__int128 value) const { return from_signed(value); }
        unsigned operator()(unsigned __int128 value) const { return checked_width(value); }
#    endif

        template <typename T>
        unsigned operator()(const T &) const
        {
            GINT_THROW(fmt::format_error("width is not integer"));
        }

    private:
        template <typename T>
        static unsigned from_signed(T value)
        {
            if (value < 0)
                GINT_THROW(fmt::format_error("negative width"));
            return checked_width(value);
        }

        template <typename T>
        static unsigned checked_width(T value)
        {
            if (value > static_cast<T>((std::numeric_limits<int>::max)()))
                GINT_THROW(fmt::format_error("width is too large"));
            return static_cast<unsigned>(value);
        }
    };

    // fmt 11.0.0 provides only a non-const basic_format_arg::visit().
    template <typename FormatArg>
    static unsigned visit_dynamic_width_arg(FormatArg arg)
    {
#    if FMT_VERSION >= 110000
        return arg.visit(dynamic_width_visitor{});
#    else
        return fmt::visit_format_arg(dynamic_width_visitor{}, arg);
#    endif
    }

    template <typename LocaleRef>
    static void apply_locale_grouping(std::string & digits, LocaleRef loc)
    {
#    if !defined(FMT_USE_LOCALE) || FMT_USE_LOCALE
        const std::locale locale = loc.template get<std::locale>();
        const std::numpunct<char> & punct = std::use_facet<std::numpunct<char>>(locale);
        const std::string grouping = punct.grouping();
        if (grouping.empty() || digits.size() < 2)
            return;

        const char separator = punct.thousands_sep();
        std::string grouped;
        grouped.reserve(digits.size() + digits.size() / 3);

        size_t end = digits.size();
        size_t grouping_index = 0;
        while (end > 0)
        {
            const int group_size = grouping[grouping_index];
            if (group_size <= 0 || group_size == (std::numeric_limits<char>::max)())
                break;
            if (static_cast<size_t>(group_size) >= end)
                break;

            const size_t begin = end - group_size;
            if (!grouped.empty())
                grouped.insert(grouped.begin(), separator);
            grouped.insert(0, digits, begin, group_size);
            end = begin;
            if (grouping_index + 1 < grouping.size())
                ++grouping_index;
        }

        if (end > 0)
        {
            if (!grouped.empty())
                grouped.insert(grouped.begin(), separator);
            grouped.insert(0, digits, 0, end);
        }
        digits.swap(grouped);
#    else
        (void)digits;
        (void)loc;
#    endif
    }

#    if FMT_VERSION >= 100000 && (!defined(FMT_USE_LOCALE) || FMT_USE_LOCALE)
    template <typename Native, typename FormatContext>
    auto format_native(Native value, unsigned resolved_width, FormatContext & ctx) const -> typename FormatContext::iterator
    {
        // Reuse fmt's integer specs for the facet, including a signed gint's sign
        // flag when its positive value needs an unsigned native representation.
        fmt::detail::dynamic_format_specs<char> specs;
        fmt::format_parse_context parse_ctx(localized_specs);
        fmt::detail::parse_format_specs(parse_ctx.begin(), parse_ctx.end(), specs, parse_ctx, fmt::detail::type::long_long_type);
        specs.width = static_cast<int>(resolved_width);
        auto out = ctx.out();
        auto && buffer = fmt::detail::get_buffer<char>(out);
        fmt::detail::write<char>(fmt::appender(buffer), value, specs, ctx.locale());
        return fmt::detail::get_iterator(buffer, out);
    }
#    endif

    template <typename FormatContext>
    auto format(const gint::integer<Bits, Signed> & value, FormatContext & ctx) const -> typename FormatContext::iterator
    {
        using Int = gint::integer<Bits, Signed>;
        unsigned base = 10;
        bool uppercase = false;
        if (presentation == 'x' || presentation == 'X')
        {
            base = 16;
            uppercase = (presentation == 'X');
        }
        else if (presentation == 'o')
        {
            base = 8;
        }
        else if (presentation == 'b' || presentation == 'B')
        {
            base = 2;
            uppercase = (presentation == 'B');
        }

        const bool negative = std::is_same<Signed, signed>::value && value < Int(0);
        unsigned resolved_width = width;
        if (dynamic_width_arg_id >= 0)
            resolved_width = visit_dynamic_width_arg(ctx.arg(dynamic_width_arg_id));
        else if (dynamic_width_is_named)
            resolved_width = visit_dynamic_width_arg(ctx.arg(dynamic_width_arg_name));
        const bool use_localized_digits = localized && presentation != 'c';
#    if FMT_VERSION >= 100000 && (!defined(FMT_USE_LOCALE) || FMT_USE_LOCALE)
        // fmt before 10.2 renders localized nondecimal integers in decimal.
        if (use_localized_digits && (FMT_VERSION >= 100200 || base == 10)
            && std::has_facet<fmt::format_facet<std::locale>>(ctx.locale().template get<std::locale>()))
        {
#        if FMT_USE_INT128
            using NativeSigned = __int128;
            using NativeUnsigned = unsigned __int128;
#        else
            using NativeSigned = long long;
            using NativeUnsigned = unsigned long long;
#        endif
            const auto signed_value = static_cast<NativeSigned>(value);
            if (std::is_same<Signed, signed>::value && (signed_value < 0) == negative && Int(signed_value) == value)
                return format_native(signed_value, resolved_width, ctx);
            const auto unsigned_value = static_cast<NativeUnsigned>(value);
            if (!negative && Int(unsigned_value) == value)
                return format_native(unsigned_value, resolved_width, ctx);
        }
#    endif
        std::string prefix;
        std::string digits;
        if (presentation == 'c')
        {
            const Int magnitude = negative ? -value : value;
            digits.assign(1, static_cast<char>(static_cast<unsigned char>(magnitude)));
        }
        else if (base == 10)
        {
            digits = gint::to_string(value);
            if (!digits.empty() && digits[0] == '-')
            {
                prefix = "-";
                digits.erase(digits.begin());
            }
        }
        else
        {
            const Int magnitude = negative ? -value : value;
            digits = gint::detail::to_base_string(magnitude, base, uppercase);
            if (negative)
                prefix = "-";
        }

        if (use_localized_digits)
            apply_locale_grouping(digits, ctx.locale());

        if (presentation != 'c' && prefix.empty())
        {
            if (sign == '+')
                prefix = "+";
            else if (sign == ' ')
                prefix = " ";
        }

        if (presentation != 'c' && alternate && (digits != "0" || base == 16 || base == 2))
        {
            if (base == 16)
                prefix += uppercase ? "0X" : "0x";
            else if (base == 2)
                prefix += uppercase ? "0B" : "0b";
            else if (base == 8 && digits[0] != '0')
                prefix += "0";
        }

        const size_t size = prefix.size() + digits.size();
        const size_t padding = resolved_width > size ? resolved_width - size : 0;
        const char effective_align = (presentation == 'c' && !explicit_align && align == '>') ? '<' : align;
        const size_t left = effective_align == '<' ? 0 : effective_align == '^' ? padding / 2 : padding;
        const bool pad_after_prefix = effective_align == '=' && !use_localized_digits;
        auto out = ctx.out();
        if (!pad_after_prefix)
            out = write_fill(out, left);
        out = std::copy(prefix.begin(), prefix.end(), out);
        if (pad_after_prefix)
            out = write_fill(out, left);
        out = std::copy(digits.begin(), digits.end(), out);
        return write_fill(out, padding - left);
    } // LCOV_EXCL_LINE
};
} // namespace fmt
#endif
