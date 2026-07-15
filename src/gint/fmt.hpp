#pragma once

#include "configuration_pass.hpp"
#include "string_stream.hpp"

#if !defined(GINT_DETAIL_CORE_ONLY) && !defined(GINT_DETAIL_IO_DEFINITIONS_INCLUDED)

#    ifdef GINT_ENABLE_FMT
namespace fmt
{
template <size_t Bits, typename Signed>
struct formatter<gint::integer<Bits, Signed>>
{
    char fill = ' ';
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

    static FMT_CONSTEXPR bool is_name_start(char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'; }

    static FMT_CONSTEXPR bool is_name_char(char ch) { return is_name_start(ch) || (ch >= '0' && ch <= '9'); }

    template <typename ParseContext>
    FMT_CONSTEXPR auto parse(ParseContext & ctx) -> typename ParseContext::iterator
    {
        auto it = ctx.begin();
        const auto end = ctx.end();
        if (it == end || *it == '}')
            return it;

        auto next = it;
        ++next;
        if (next != end && (*next == '<' || *next == '>' || *next == '^'))
        {
            if (*it == '{' || *it == '}')
                GINT_THROW(fmt::format_error("invalid fill character"));
            fill = *it;
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
                fill = '0';
            }
#        if FMT_VERSION < 120000
            else
            {
                fill = '0';
            }
#        endif
            ++it;
        }

        while (it != end && *it >= '0' && *it <= '9')
        {
            const unsigned digit = static_cast<unsigned>(*it - '0');
            if (width > ((std::numeric_limits<unsigned>::max)() - digit) / 10u)
                GINT_THROW(fmt::format_error("number is too big"));
            width = width * 10u + digit;
            ++it;
        }

        if (it != end && *it == '{')
        {
            ++it;
            if (it != end && *it == '}')
            {
                dynamic_width_arg_id = ctx.next_arg_id();
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
                int arg_id = 0;
                bool has_arg_id = false;
                while (it != end && *it >= '0' && *it <= '9')
                {
                    has_arg_id = true;
                    const int digit = static_cast<int>(*it - '0');
                    if (arg_id > ((std::numeric_limits<int>::max)() - digit) / 10)
                        GINT_THROW(fmt::format_error("number is too big"));
                    arg_id = arg_id * 10 + digit;
                    ++it;
                }
                if (!has_arg_id || it == end || *it != '}')
                    GINT_THROW(fmt::format_error("invalid format specifier for gint::integer"));
                ctx.check_arg_id(arg_id);
                dynamic_width_arg_id = arg_id;
                ++it;
            }
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
        return it;
    }

    struct dynamic_width_visitor
    {
        unsigned operator()(int value) const { return from_signed(value); }
        unsigned operator()(long value) const { return from_signed(value); }
        unsigned operator()(long long value) const { return from_signed(value); }
        unsigned operator()(unsigned value) const { return value; }
        unsigned operator()(unsigned long value) const { return from_unsigned(value); }
        unsigned operator()(unsigned long long value) const { return from_unsigned(value); }

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
            return from_unsigned(static_cast<typename std::make_unsigned<T>::type>(value));
        }

        template <typename T>
        static unsigned from_unsigned(T value)
        {
            if (value > static_cast<T>((std::numeric_limits<unsigned>::max)()))
                GINT_THROW(fmt::format_error("width is too large"));
            return static_cast<unsigned>(value);
        }
    };

    template <typename FormatArg>
    static unsigned visit_dynamic_width_arg(const FormatArg & arg)
    {
#        if FMT_VERSION >= 120000
        return arg.visit(dynamic_width_visitor{});
#        else
        return fmt::visit_format_arg(dynamic_width_visitor{}, arg);
#        endif
    }

    template <typename LocaleRef>
    static void apply_locale_grouping(std::string & digits, LocaleRef loc)
    {
#        if !defined(FMT_USE_LOCALE) || FMT_USE_LOCALE
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
            const unsigned char group_size = static_cast<unsigned char>(grouping[grouping_index]);
            if (group_size == 0 || group_size == static_cast<unsigned char>((std::numeric_limits<char>::max)()))
                break;
            if (group_size >= end)
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
#        else
        (void)digits;
        (void)loc;
#        endif
    }

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

        const bool use_localized_digits = localized && presentation != 'c'
#        if FMT_VERSION < 120000
            && base == 10
#        endif
            ;
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

        std::string text = prefix + digits;
        unsigned resolved_width = width;
        if (dynamic_width_arg_id >= 0)
            resolved_width = visit_dynamic_width_arg(ctx.arg(dynamic_width_arg_id));
        else if (dynamic_width_is_named)
            resolved_width = visit_dynamic_width_arg(ctx.arg(dynamic_width_arg_name));
        if (resolved_width > text.size())
        {
            const size_t padding = resolved_width - text.size();
            const char effective_align = (presentation == 'c' && !explicit_align && align == '>') ? '<' : align;
            if (effective_align == '<')
            {
                text.append(padding, fill);
            }
            else if (effective_align == '^')
            {
                const size_t left = padding / 2;
                const size_t right = padding - left;
                text.insert(0, left, fill);
                text.append(right, fill);
            }
            else if (effective_align == '=' && !use_localized_digits && !prefix.empty())
            {
                text = prefix + std::string(padding, fill) + digits;
            }
            else
            {
                text.insert(0, padding, fill);
            }
        }

        auto out = ctx.out();
        for (size_t i = 0; i < text.size(); ++i)
            *out++ = text[i];
        return out;
    } // LCOV_EXCL_LINE
};
} // namespace fmt
#    endif

#endif // !GINT_DETAIL_CORE_ONLY && !GINT_DETAIL_IO_DEFINITIONS_INCLUDED
