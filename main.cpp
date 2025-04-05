// A grep program that allows search by grammar or by lexer spec only
// as well as the conventional way.

#include "pch.h"

#include "args.hpp"
#include "colours.hpp"
#include "gg_error.hpp"
#include "output.hpp"
#include "parser.hpp"
#include "search.hpp"
#include "types.hpp"
#include "version.hpp"

#include <lexertl/debug.hpp>
#include <lexertl/dot.hpp>
#include <lexertl/enums.hpp>
#include <parsertl/enums.hpp>
#include <lexertl/generator.hpp>
#include <parsertl/generator.hpp>
#include <lexertl/iterator.hpp>
#include <parsertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include <boost/regex.hpp>
#include <lexertl/rules.hpp>
#include <parsertl/rules.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/state_machine.hpp>
#include <lexertl/utf_iterators.hpp>
#include <wildcardtl/wildcard.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iosfwd>
#include <iostream>
#include <map>

#ifdef _WIN32
#include <minwindef.h>
#include <processenv.h>
#endif

#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

extern std::string build_text(const std::string& input,
    const capture_vector& captures);
extern const char* try_help();
extern std::string unescape(const std::string_view& vw);
extern const char* usage();

enum class file_type
{
    ansi, binary, utf8, utf16, utf16_flip
};

using match_rev_iter = std::reverse_iterator<std::vector<match>::iterator>;
namespace fs = std::filesystem;

boost::regex g_capture_rx(R"(\$\d+)");
condition_map g_conditions;
condition_parser g_condition_parser;
config_parser g_config_parser;
parser* g_curr_parser = nullptr;
uparser* g_curr_uparser = nullptr;
std::size_t g_files = 0;
std::size_t g_hits = 0;
options g_options;
pipeline g_pipeline;
std::size_t g_searched = 0;

static file_type fetch_file_type(const char* data, std::size_t size)
{
    file_type type = file_type::ansi;

    if (size > 1)
    {
        auto utf16 = std::bit_cast<const uint16_t*>(data);

        switch (*utf16)
        {
        case 0xfeff:
            type = file_type::utf16;
            break;
        case 0xfffe:
            type = file_type::utf16_flip;
            break;
        default:
            if (size > 2)
            {
                auto utf8 = std::bit_cast<const unsigned char*>(data);

                if (utf8[0] == 0xef && utf8[1] == 0xbb && utf8[2] == 0xbf)
                    type = file_type::utf8;
            }

            break;
        }

        if (type == file_type::ansi)
        {
            const char* second = data + size;

            if (std::find(data, second, '\0') != second)
                type = file_type::binary;
        }
    }

    return type;
}

static file_type load_file(std::vector<unsigned char>& utf8,
    const char*& data_first, const char*& data_second,
    std::vector<match>& ranges)
{
    const std::size_t size = data_second - data_first;
    file_type type = fetch_file_type(data_first, size);

    switch (type)
    {
    case file_type::utf16:
    {
        auto first = std::bit_cast<const uint16_t*>(data_first + 2);
        auto second = std::bit_cast<const uint16_t*>(data_second);
        utf16_in_iterator in(first, second);
        utf16_in_iterator in_end(second, second);
        utf8_out_iterator out(in, in_end);
        utf8_out_iterator out_end(in_end, in_end);

        utf8.reserve(size / 2 - 1);

        for (; out != out_end; ++out)
        {
            utf8.push_back(*out);
        }

        data_first = std::bit_cast<const char*>(&utf8.front());
        data_second = data_first + utf8.size();
        ranges.emplace_back(data_first, data_first, data_second);
        break;
    }
    case file_type::utf16_flip:
    {
        using in_flip_iter = lexertl::basic_utf16_in_iterator
            <lexertl::basic_flip_iterator<const uint16_t*>, int32_t>;
        using out_flip_iter = lexertl::basic_utf8_out_iterator<in_flip_iter>;
        lexertl::basic_flip_iterator
            first(std::bit_cast<const uint16_t*>(data_first + 2));
        lexertl::basic_flip_iterator
            second(std::bit_cast<const uint16_t*>(data_second));
        in_flip_iter in(first, second);
        in_flip_iter in_end(second, second);
        out_flip_iter out(in, in_end);
        out_flip_iter out_end(in_end, in_end);

        utf8.reserve(size / 2 - 1);

        for (; out != out_end; ++out)
        {
            utf8.push_back(*out);
        }

        data_first = std::bit_cast<const char*>(&utf8.front());
        data_second = data_first + utf8.size();
        ranges.emplace_back(data_first, data_first, data_second);
        break;
    }
    case file_type::utf8:
        data_first += 3;
        ranges.emplace_back(data_first, data_first, data_second);
        break;
    default:
        ranges.emplace_back(data_first, data_first, data_second);
        break;
    }

    return type;
}

[[nodiscard]] static bool perform_replacements(const match_rev_iter& iter,
    const match& tuple, match_data& data)
{
    // Only allow _replace if g_modify (grammar actions) not set
    if (g_options._perform_output && !g_options._modify)
    {
        const char* first = data._negate ? iter->_first : iter->_second;

        if (first < tuple._first || first > tuple._second)
        {
            output_text_nl(std::cerr, is_a_tty(stderr),
                g_options._wa_text.c_str(),
                std::format("{}Cannot replace text when source is not "
                    "contained in original string.",
                    gg_text()));
            return true;
        }
        else
        {
            // Replace with _replace.
            const char* second = iter->_eoi;

            if (data._captures.empty())
                data._replacements[std::make_pair(first - data._first,
                    second - first)] = g_options._replace;
            else
            {
                static lexertl::state_machine cap_sm;
                std::string replace;
                bool skip = false;

                if (cap_sm.empty())
                {
                    lexertl::rules rules;

                    rules.push(R"(\$\d)", 1);
                    lexertl::generator::build(rules, cap_sm);
                }

                lexertl::citerator i(g_options._replace.c_str(),
                    g_options._replace.c_str() + g_options._replace.size(),
                    cap_sm);
                lexertl::citerator e;

                for (; i != e; ++i)
                {
                    if (i->id == 1)
                    {
                        const std::size_t idx = atoi(i->first + 1);

                        if (idx < data._captures.size())
                            replace += data._captures[idx].front();
                        else
                        {
                            output_text_nl(std::cerr, is_a_tty(stderr),
                                g_options._wa_text.c_str(),
                                std::format("{}Capture ${} is "
                                    "out of range.",
                                    gg_text(),
                                    idx));
                            skip = true;
                        }
                    }
                    else
                        replace.push_back(*i->first);
                }

                if (!skip)
                    data._replacements[std::make_pair(first - data._first,
                        second - first)] = replace;
            }
        }
    }

    return false;
}

static std::string_view normalise_pathname(const std::string& pathname)
{
    std::string_view pn = (pathname[0] == '.' &&
        pathname[1] == fs::path::preferred_separator) ?
        pathname.c_str() + 2 :
        pathname.c_str();

    return pn;
}

static void print_pathname(const std::string& pathname)
{
    const std::string_view pn = normalise_pathname(pathname);

    output_text(std::cout, is_a_tty(stdout),
        g_options._fn_text.c_str(), pn);
}

static void print_separator(const std::string& separator)
{
    output_text(std::cout, is_a_tty(stdout),
        g_options._se_text.c_str(), separator);
}

static void print_prefix(const std::string& pathname,
    match_data& data, const std::string& separator)
{
    if (pathname.empty())
    {
        if (g_options._show_filename == show_filename::yes)
            output_text(std::cout, is_a_tty(stdout),
                g_options._fn_text.c_str(), g_options._label);
    }
    else if (g_options._show_filename != show_filename::no)
    {
        print_pathname(pathname);
    }

    if (((!g_options._label.empty() &&
        g_options._show_filename == show_filename::yes) ||
        (!pathname.empty() &&
        g_options._show_filename != show_filename::no)) &&
        g_options._line_numbers != line_numbers::with_parens)
    {
        print_separator(separator);
    }

    if (g_options._line_numbers == line_numbers::with_parens)
        print_separator("(");

    if (g_options._line_numbers != line_numbers::none)
    {
        output_text(std::cout, is_a_tty(stdout),
            g_options._ln_text.c_str(),
            std::to_string(1 + data._curr_line));
        print_separator(g_options._line_numbers == line_numbers::with_parens ?
            (')' + separator) :
            separator);
    }

    if (g_options._byte_offset)
    {
        output_text(std::cout, is_a_tty(stdout),
            g_options._bn_text.c_str(),
            std::to_string(data._curr - data._first));
        print_separator(separator);
    }

    if (g_options._initial_tab)
        std::cout << '\t';
}

static void find_bol(match_data& data)
{
    if (data._curr > data._eol)
    {
        data._bol =
            std::to_address(std::find(std::reverse_iterator(data._curr),
                std::reverse_iterator(data._first), '\n'));

        if (data._bol != data._first)
            ++data._bol;

        data._eol = std::find_if(data._curr, data._second,
            [](const char c)
            {
                return c == '\r' || c == '\n';
            });
    }
}

template<class InputIterator, class NthOccurence, class UnaryPredicate>
InputIterator find_nth_if(InputIterator first, InputIterator last,
    NthOccurence Nth, UnaryPredicate pred)
{
    if (Nth > 0)
    {
        for (; Nth && ++first != last; --Nth)
            first = std::find_if(first, last, pred);

        return first;
    }
    else
        return last;
}

[[nodiscard]] static const char* consume_eol(const char* ptr, const char* eoi)
{
    if (ptr != eoi && *ptr == '\r')
        ++ptr;

    if (ptr != eoi && *ptr == '\n')
        ++ptr;

    return ptr;
}

static std::size_t print_after(const std::string& pathname, match_data& data)
{
    std::size_t before = 0;

    if (data._curr_line)
    {
        std::size_t diff = data._curr_line -
            (data._prev_line == std::string::npos ?
                data._curr_line :
                data._prev_line);
        std::size_t until = 0;

        if (diff > g_options._after_context)
        {
            before = data._prev_line + 1;
            until = before + g_options._after_context;
        }
        else
        {
            before = data._curr_line - (diff - 1);
            until = data._curr_line;
        }

        if (before < until)
        {
            const char* ptr = find_nth_if(data._first, data._second, before,
                [](const char c)
                {
                    return c == '\n';
                });
            const char* curr = data._curr;
            std::size_t curr_line = data._curr_line;

            // Skip over \n
            if (ptr != data._second)
                ++ptr;

            for (; before < until; ++before)
            {
                const char* end = std::find_if(ptr, data._second,
                    [](const char c)
                    {
                        return c == '\r' || c == '\n';
                    });

                data._curr_line = before;
                print_prefix(pathname, data, "-");

                if (data._negate)
                    std::cout << std::string_view(ptr, end);
                else
                {
                    output_text(std::cout, is_a_tty(stdout),
                        g_options._cx_text.c_str(),
                        std::string_view(ptr, end));
                }

                std::cout << '\n';
                end = consume_eol(end, data._second);
                ptr = end;
            }

            data._curr = curr;
            data._curr_line = curr_line;
            data._prev_line = before;
        }

        if (data._prev_line == std::string::npos)
            diff = data._curr_line;
        else
            diff = data._curr_line - data._prev_line;

        if (diff > g_options._before_context)
            before = data._curr_line - g_options._before_context;
        else
            before = data._curr_line - (diff - 1);
    }

    return before;
}

static void print_separators(const std::string& pathname, match_data& data)
{
    if (g_options._hit_separator)
    {
        // Number of lines since last hit
        std::size_t before = print_after(pathname, data);

        if (data._curr_line - data._prev_line > 1)
        {
            const char* ptr = find_nth_if(data._first, data._second, before,
                [](const char c)
                {
                    return c == '\n';
                });
            const char* curr = data._curr;
            std::size_t curr_line = data._curr_line;

            // Skip over \n
            if (ptr != data._second)
                ++ptr;

            if (!g_options._separator.empty() &&
                data._prev_line != std::string::npos &&
                data._curr_line - 1 - data._prev_line >
                g_options._before_context)
            {
                print_separator(g_options._separator);
                std::cout << '\n';
            }

            for (; before < curr_line; ++before)
            {
                const char* first = ptr;

                data._curr_line = before;
                print_prefix(pathname, data, "-");
                ptr = std::find_if(ptr, data._second,
                    [](const char c)
                    {
                        return c == '\r' || c == '\n';
                    });

                if (data._negate)
                    std::cout << std::string_view(first, ptr);
                else
                    output_text(std::cout, is_a_tty(stdout),
                        g_options._cx_text.c_str(), std::string_view(first, ptr));

                std::cout << '\n';
                ptr = consume_eol(ptr, data._second);
                ++data._curr_line;
            }

            data._curr = curr;
            data._curr_line = curr_line;
        }
    }
}

static void display_match(const std::string& pathname,
    match_data& data, const match_rev_iter& iter, bool& finished)
{
    if (data._curr > data._eol)
    {
        if (g_options._max_count != std::string::npos)
        {
            ++data._count;

            if (data._count > g_options._max_count)
            {
                finished = true;
                return;
            }
        }

        if (data._eol)
        {
            if (g_options._colour && is_a_tty(stdout) &&
                !g_options._sl_text.empty())
            {
                std::cout << g_options._sl_text;

                if (!g_options._ne)
                    std::cout << szEraseEOL;
            }

            // Print remaining text until the end of line
            for (; data._bol != data._eol; ++data._bol)
                std::cout << *data._bol;

            if (g_options._colour && is_a_tty(stdout) &&
                !g_options._sl_text.empty())
            {
                std::cout << szDefaultText;

                if (!g_options._ne)
                    std::cout << szEraseEOL;
            }

            std::cout << '\n';
        }

        data._prev_line = data._curr_line;
        data._curr_line = std::count(data._first, data._curr, '\n');

        if (!data._negate)
            print_separators(pathname, data);

        print_prefix(pathname, data, ":");
    }

    if (!g_options._only_matching && !g_options._whole_match &&
        !data._negate)
    {
        find_bol(data);
    }

    if (g_options._whole_match)
    {
        output_text_nl(std::cout, is_a_tty(stdout),
            g_options._ms_text.c_str(), iter->view());
    }
    else if (g_options._only_matching)
    {
        const char* start = data._curr;

        data._curr = std::find_if(data._curr, iter->_eoi,
            [](const char c)
            {
                return c == '\r' || c == '\n';
            });

        output_text_nl(std::cout, is_a_tty(stdout),
            g_options._ms_text.c_str(),
            std::string_view(start, data._curr));
    }
    else
    {
        if (g_options._colour && is_a_tty(stdout) &&
            !g_options._sl_text.empty())
        {
            std::cout << g_options._sl_text;

            if (!g_options._ne)
                std::cout << szEraseEOL;
        }

        if (data._bol)
        {
            for (; data._bol < data._curr; ++data._bol)
                std::cout << *data._bol;
        }
        else
        {
            const char* first = data._curr;

            data._curr = std::find_if(first, iter->_eoi,
                [](const char c)
                {
                    return c == '\r' || c == '\n';
                });

            std::cout << std::string_view(first, data._curr);
            std::cout << '\n';
            data._curr = consume_eol(data._curr, data._second);
        }

        if (!data._negate)
        {
            const char* eoi = data._eol > iter->_second ?
                (iter->_first == iter->_second ?
                    (iter->_eoi < data._eol ?
                        iter->_eoi :
                        data._eol) :
                    iter->_second) :
                data._eol;

            output_text(std::cout, is_a_tty(stdout),
                g_options._ms_text.c_str(),
                std::string_view(data._bol, eoi));
            data._bol = eoi;
        }
    }
}

static bool process_matches(match_data& data,
    std::map<std::pair<std::size_t, std::size_t>,
    std::string>& temp_replacements, const std::string& pathname)
{
    bool finished = false;
    const auto& tuple = data._ranges.front();
    auto iter = data._ranges.rbegin();
    auto end = data._ranges.rend();

    data._replacements.insert(temp_replacements.begin(), temp_replacements.end());
    temp_replacements.clear();

    if (perform_replacements(iter, tuple, data))
        return true;

    for (; iter != end; ++iter)
    {
        const char* first = iter->_second;

        if (first >= tuple._first && first <= tuple._eoi)
        {
            data._curr = iter->_first;

            if (g_options._pathname_only == pathname_only::yes)
            {
                print_pathname(pathname);
                std::cout << output_nl;
                finished = true;
            }
            else if (!g_options._print.empty())
            {
                std::cout << build_text(g_options._print, data._captures);
            }
            else if (!g_options._exec.empty())
            {
                const std::string cmd = build_text(g_options._exec,
                    data._captures);

                output_text_nl(std::cout, is_a_tty(stdout),
                    g_options._wa_text.c_str(),
                    std::format("Executing: {}", cmd));
                std::cout << exec_ret(cmd);
            }
            else if (g_options._pathname_only != pathname_only::negated &&
                !g_options._show_count && !g_options._rule_print && !g_options._quiet)
            {
                if (data._negate && data._curr != data._first)
                {
                    data._curr = std::find_if(data._curr, data._second,
                        [](const char c) { return c == '\r' || c == '\n'; });
                    data._curr = consume_eol(data._curr, data._second);
                    data._curr_line =
                        std::count(data._first, data._ranges.back()._eoi, '\n');
                }

                if (!(data._negate && data._curr_line - data._prev_line <= 1))
                {
                    // do ... while in case the line is blank
                    do
                    {
                        display_match(pathname, data, iter, finished);
                    } while (data._negate && std::find_if(data._curr, iter->_eoi,
                        [](const char c) { return c == '\r' || c == '\n'; }) !=
                        iter->_eoi);
                }

                if (data._negate)
                    data._prev_line = std::count(data._first, data._curr, '\n');
            }

            if (g_options._show_count)
            {
                auto eol = std::find_if(data._ranges.back()._eoi, data._second,
                    [](const char c)
                    {
                        return c == '\r' || c == '\n';
                    });

                if (data._eol != eol)
                {
                    if (data._negate)
                    {
                        if (data._eol)
                        {
                            const std::size_t count =
                                std::count(data._curr, eol, '\n') - 1;

                            data._count += count;
                        }
                        else
                            data._count += std::count(data._first,
                                data._ranges.back()._eoi, '\n');
                    }
                    else
                        data._count += std::count(data._ranges.back()._first,
                            consume_eol(eol, data._second), '\n');
                }

                data._eol = eol;

                if (data._count == g_options._max_count)
                    finished = true;
            }

            ++data._hits;
            break;
        }
    }


    return finished;
}

static void perform_output(match_data& data, const std::string& pathname,
    lexertl::memory_file& mf,
    const file_type type, const std::size_t size)
{
    const auto perms = fs::status(pathname.c_str()).permissions();

    ++g_files;
    g_hits += data._hits;

    if ((perms & fs::perms::owner_write) != fs::perms::owner_write)
    {
        // Read-only
        if (!g_options._checkout.empty())
        {
            std::string checkout = g_options._checkout;

            if (const std::size_t index = checkout.find("$1");
                index != std::string::npos)
            {
                checkout.erase(index, 2);
                checkout.insert(index, pathname);
            }

            if (::system(checkout.c_str()) != 0)
                throw gg_error(std::format("Failed to execute {}.\n",
                    g_options._checkout));
        }
        else if (g_options._force_write)
            fs::permissions(pathname.c_str(), perms | fs::perms::owner_write);
    }

    if (!data._replacements.empty())
    {
        std::string content(data._first, data._second);

        for (auto iter = data._replacements.rbegin(),
            end = data._replacements.rend(); iter != end; ++iter)
        {
            content.erase(iter->first.first, iter->first.second);
            content.insert(iter->first.first, iter->second);
        }

        data._replacements.clear();
        // In case the memory_file is still open
        mf.close();

        if ((fs::status(pathname.c_str()).permissions() &
            fs::perms::owner_write) != fs::perms::owner_write)
        {
            if (!g_options._no_messages)
            {
                output_text_nl(std::cerr, is_a_tty(stderr),
                    g_options._wa_text.c_str(),
                    std::format("{}{} is read only.",
                        gg_text(),
                        pathname));
            }
        }
        else
        {
            switch (type)
            {
            case file_type::utf16:
            {
                std::vector<uint16_t> utf16;
                auto first = std::bit_cast<const unsigned char*>(&content.front());
                const unsigned char* second = first + content.size();
                lexertl::basic_utf8_in_iterator<const unsigned char*, int32_t>
                    iter(first, second);
                lexertl::basic_utf8_in_iterator<const unsigned char*, int32_t>
                    end(second, second);

                utf16.reserve(size);
                utf16.push_back(0xfeff);

                for (; iter != end; ++iter)
                {
                    const int32_t val = *iter;
                    lexertl::basic_utf16_out_iterator<const int32_t*, uint16_t>
                        out_iter(&val, &val + 1);
                    lexertl::basic_utf16_out_iterator<const int32_t*, uint16_t>
                        out_end(&val + 1, &val + 1);

                    for (; out_iter != out_end; ++out_iter)
                    {
                        utf16.push_back(*out_iter);
                    }
                }

                std::ofstream os(pathname.c_str(), std::ofstream::binary);

                os.exceptions(std::ofstream::badbit);
                os.write(std::bit_cast<const char*>(&utf16.front()),
                    utf16.size() * sizeof(uint16_t));
                os.close();
                break;
            }
            case file_type::utf16_flip:
            {
                std::vector<uint16_t> utf16;
                auto first = std::bit_cast<const unsigned char*>(&content.front());
                const unsigned char* second = first + content.size();
                lexertl::basic_utf8_in_iterator<const unsigned char*, int32_t>
                    iter(first, second);
                lexertl::basic_utf8_in_iterator<const unsigned char*, int32_t>
                    end(second, second);

                utf16.reserve(size);
                utf16.push_back(0xfffe);

                for (; iter != end; ++iter)
                {
                    const int32_t val = *iter;
                    lexertl::basic_utf16_out_iterator<const int32_t*, uint16_t>
                        out_iter(&val, &val + 1);
                    lexertl::basic_utf16_out_iterator<const int32_t*, uint16_t>
                        out_end(&val + 1, &val + 1);

                    for (; out_iter != out_end; ++out_iter)
                    {
                        uint16_t flip = *out_iter;
                        lexertl::basic_flip_iterator flip_iter(&flip);

                        utf16.push_back(*flip_iter);
                    }
                }

                std::ofstream os(pathname.c_str(), std::ofstream::binary);

                os.exceptions(std::ofstream::badbit);
                os.write(std::bit_cast<const char*>(&utf16.front()),
                    utf16.size() * sizeof(uint16_t));
                os.close();
                break;
            }
            case file_type::utf8:
            {
                std::ofstream os(pathname.c_str(), std::ofstream::binary);
                const unsigned char header[] = { 0xef, 0xbb, 0xbf };

                os.exceptions(std::ofstream::badbit);
                os.write(std::bit_cast<const char*>(&header[0]), sizeof(header));
                os.write(content.c_str(), content.size());
                os.close();
                break;
            }
            default:
            {
                std::ofstream os(pathname.c_str(), std::ofstream::binary);

                os.exceptions(std::ofstream::badbit);
                os.write(content.c_str(), content.size());
                os.close();
                break;
            }
            }
        }
    }
}

static void process_file(const std::string& pathname, std::string* cin = nullptr)
{
    if (g_options._writable && (fs::status(pathname).permissions() &
        fs::perms::owner_write) == fs::perms::none)
    {
        return;
    }

    lexertl::memory_file mf(pathname.c_str());
    std::vector<unsigned char> utf8;
    file_type type = file_type::ansi;
    match_data data;
    bool first_hit = true;
    bool finished = false;

    if (!mf.data() && !cin)
    {
        if (!g_options._no_messages)
        {
            output_text_nl(std::cerr, is_a_tty(stderr),
                g_options._wa_text.c_str(),
                std::format("{}failed to open {}.",
                    gg_text(),
                    pathname));
        }

        return;
    }

    if (cin)
    {
        std::ostringstream ss;

        ss << std::cin.rdbuf();
        *cin = ss.str();
        data._first = cin->c_str();
        data._second = data._first + cin->size();
    }
    else
    {
        data._first = mf.data();
        data._second = data._first + mf.size();
    }

    type = load_file(utf8, data._first, data._second, data._ranges);

    if (type == file_type::utf16 || type == file_type::utf16_flip)
        // No need for original data
        mf.close();
    else if (type == file_type::binary)
    {
        switch (g_options._binary_files)
        {
        case binary_files::text:
            type = file_type::ansi;
            break;
        case binary_files::without_match:
            return;
        default:
            break;
        }
    }

    do
    {
        std::map<std::pair<std::size_t, std::size_t>, std::string>
            temp_replacements;

        if (bool success = search(data, temp_replacements);
            success)
        {
            if (g_options._hit_separator && g_hits && first_hit &&
                g_options._pathname_only != pathname_only::negated &&
                !g_options._show_count && g_options._print.empty() &&
                !g_options._rule_print && !g_options._quiet)
            {
                print_separator(g_options._separator);
                std::cout << '\n';
            }

            first_hit = false;

            if (type == file_type::binary)
            {
                if (g_options._pathname_only == pathname_only::no)
                    output_text(std::cout, is_a_tty(stdout),
                        g_options._fn_text.c_str(),
                        std::format("{}Binary file ", gg_text()));

                output_text(std::cout, is_a_tty(stdout),
                    g_options._fn_text.c_str(),
                    normalise_pathname(pathname));

                if (g_options._pathname_only == pathname_only::no)
                    output_text(std::cout, is_a_tty(stdout),
                        g_options._fn_text.c_str(),
                        " matches");

                std::cout << '\n';
                return;
            }
            else
            {
                finished = process_matches(data, temp_replacements, pathname);
            }
        }
        else
            data._negate = false;

        const match old = data._ranges.back();

        data._ranges.pop_back();

        if (!data._ranges.empty())
        {
            // Cleardown any stale strings in matches.
            // First makes sure current range is not from the same
            // string (in matches) as the last.
            if (const auto& curr = data._ranges.back();
                !data._matches.empty() &&
                (old._first < curr._first || old._first > curr._eoi))
            {
                while (!data._matches.empty() &&
                    old._first >= data._matches.top().c_str() &&
                    old._eoi <= data._matches.top().c_str() +
                    data._matches.top().size())
                {
                    data._matches.pop();
                }
            }

            // Start searching from end of last match
            data._ranges.back()._first = data._ranges.back()._second;
        }
    } while (!finished && !data._ranges.empty());

    if (g_options._pathname_only != pathname_only::negated &&
        !g_options._show_count && g_options._print.empty() &&
        !g_options._rule_print && !g_options._quiet)
    {
        if (g_options._colour && is_a_tty(stdout) &&
            !g_options._sl_text.empty())
        {
            std::cout << g_options._sl_text;

            if (!g_options._ne)
                std::cout << szEraseEOL;
        }

        for (; data._bol != data._eol; ++data._bol)
            std::cout << *data._bol;

        if (g_options._colour && is_a_tty(stdout) &&
            !g_options._sl_text.empty())
        {
            std::cout << szDefaultText;

            if (!g_options._ne)
                std::cout << szEraseEOL;
        }

        if (data._bol)
            // Only output newline if there has been at least one match
            std::cout << '\n';

        data._prev_line = data._curr_line;
        data._curr_line = std::count(data._first, data._second, '\n');
        data._curr = data._second;

        if (!data._negate)
            print_after(pathname, data);
    }

    if (data._hits)
    {
        perform_output(data, pathname, mf, type, utf8.size());
    }

    if (g_options._show_count)
    {
        if (g_options._show_filename != show_filename::no)
        {
            print_pathname(pathname);
            print_separator(":");
        }

        std::cout << data._count << output_nl;
    }

    if (g_options._pathname_only == pathname_only::negated && !data._hits)
    {
        print_pathname(pathname);
        std::cout << output_nl;
    }

    ++g_searched;
}

static bool process_file(const std::string& pathname, const wildcards &wcs)
{
    bool process = false;
    const char* filename = pathname.c_str() +
        pathname.rfind(fs::path::preferred_separator) + 1;
    bool skip = !g_options._exclude._negative.empty();

    for (const auto& pn : g_options._exclude._negative)
    {
        if (!pn._wc.match(filename))
        {
            skip = false;
            break;
        }
    }

    if (!skip)
    {
        for (const auto& pn : g_options._exclude._positive)
        {
            if (pn._wc.match(filename))
            {
                skip = true;
                break;
            }
        }
    }

    if (!skip)
    {
        process = !wcs._negative.empty();

        for (const auto& pn : wcs._negative)
        {
            if (!pn._wc.match(pathname))
            {
                process = false;
                break;
            }
        }

        if (!process)
        {
            for (const auto& pn : wcs._positive)
            {
                if (pn._wc.match(pathname))
                {
                    process = true;
                    break;
                }
            }
        }
    }

    return process;
}

static bool include_file(const std::string& path)
{
    return (g_options._include._positive.empty() ||
        std::ranges::any_of(g_options._include._positive,
            [&path](const auto& pn)
            {
                return pn._wc.match(path);
            })) &&
        std::ranges::none_of(g_options._include._negative,
            [&path](const auto& pn)
            {
                return pn._wc.match(path);
            });
}

static bool include_dir(const std::string& path)
{
    return (g_options._exclude_dirs._negative.empty() ||
        std::ranges::any_of(g_options._exclude_dirs._negative,
        [&path](const auto& pn)
        {
            return pn._wc.match(path);
        })) &&
    std::ranges::none_of(g_options._exclude_dirs._positive,
        [&path](const auto& pn)
        {
            return pn._wc.match(path);
        });
}

static void process()
{
    std::queue<std::pair<std::string, const wildcards*>> queue;

    for (const auto& [path, wcs] : g_options._pathnames)
    {
        queue.emplace(path, &wcs);
    }

    for (; !queue.empty(); queue.pop())
    {
        const auto& [path, wcs] = queue.front();
        std::error_code err;
        bool processed = false;

        for (auto iter = fs::directory_iterator(path,
            fs::directory_options::skip_permission_denied, err),
            end = fs::directory_iterator(); iter != end; ++iter)
        {
            const auto& p = iter->path();

            // Don't throw if there is a Unicode pathname
            const std::string pathname = reinterpret_cast<const char*>
                (p.u8string().c_str());

            if (!fs::is_directory(p) ||
                g_options._directories == directories::read)
            {
                if (!process_file(pathname, *wcs))
                    continue;
            }

            if (fs::is_directory(p))
            {
                switch (g_options._directories)
                {
                case directories::read:
                    if (!g_options._no_messages)
                    {
                        output_text_nl(std::cerr, is_a_tty(stderr),
                            g_options._wa_text.c_str(),
                            std::format("{}{}: Is a directory",
                                gg_text(),
                                normalise_pathname(p.string())));
                    }

                    break;
                case directories::recurse:
                    if (!(fs::is_symlink(p) && !g_options._follow_symlinks) &&
                        include_dir(pathname.substr(pathname.
                        rfind(fs::path::preferred_separator) + 1)))
                    {
                        queue.emplace(pathname, wcs);
                    }

                    break;
                case directories::skip:
                    // Do nothing
                    break;
                }
            }
            else
            {
                if ((g_options._writable && (fs::status(p).permissions() &
                    fs::perms::owner_write) == fs::perms::none) ||
                    // Skip zero length files
                    fs::file_size(p) == 0)
                {
                    continue;
                }

                if (include_file(pathname.substr(pathname.
                    rfind(fs::path::preferred_separator) + 1)))
                {
                    process_file(pathname);
                    processed = true;
                }
            }
        }

        if (!processed && g_options._directories != directories::recurse &&
            !g_options._no_messages)
        {
            for (const auto& wildcard : wcs->_positive)
            {
                if (!wildcard._pathname.empty())
                {
                    output_text_nl(std::cerr, is_a_tty(stderr),
                        g_options._wa_text.c_str(),
                        std::format("{}{}: No such file or directory",
                            gg_text(),
                            normalise_pathname(wildcard._pathname)));
                }
            }
        }
    }
}

static void add_pathname(std::string pn,
    std::map<std::string, wildcards>& map)
{
    const std::size_t wc_idx = pn.find_first_of("*?[");
    const std::size_t sep_idx = pn.rfind(fs::path::preferred_separator,
        wc_idx);
    const bool negate = pn[0] == '!';
    const std::string path = sep_idx == std::string::npos ? "." :
        pn.substr(negate ? 1 : 0, sep_idx + (negate ? 0 : 1));
    auto& wcs = map[path];

    if (g_options._show_filename == show_filename::undefined &&
        !g_options._quiet && !pn.empty() &&
        (g_options._directories == directories::recurse ||
            wc_idx != std::string::npos || negate))
    {
        g_options._show_filename = show_filename::yes;
    }

    if (sep_idx == std::string::npos)
    {
        if (!((!negate && wc_idx == 0) || (negate && wc_idx == 1)))
        {
            if (g_options._directories == directories::recurse)
                pn.insert(negate ? 1 : 0, std::string(1, '*') +
                    static_cast<char>(fs::path::preferred_separator));
            else
                pn.insert(negate ? 1 : 0, path +
                    static_cast<char>(fs::path::preferred_separator));
        }
    }
    else if (g_options._directories == directories::recurse &&
        !((!negate && wc_idx == 0) || (negate && wc_idx == 1)))
    {
        pn = std::string(1, '*') + pn.substr(sep_idx);

        if (negate)
            pn = '!' + pn;
    }

    if (negate)
        // Not using emplace_back() for compatibility with Macintosh
        wcs._negative.push_back({ wildcardtl::wildcard{ pn, is_windows() },
            wc_idx == std::string::npos ?
            pn :
            std::string() });
    else
        // Not using emplace_back() for compatibility with Macintosh
        wcs._positive.push_back({ wildcardtl::wildcard{ pn, is_windows() },
            wc_idx == std::string::npos ?
            pn :
            std::string() });
}

lexertl::state_machine word_lexer()
{
    static lexertl::state_machine sm;

    if (sm.empty())
    {
        lexertl::rules rules;

        rules.push(R"([A-Z_a-z]\w*)", 1);
        rules.push("(?s:.)", lexertl::rules::skip());
        lexertl::generator::build(rules, sm);
    }

    return sm;
}

static void queue_dfa_regex(config& cfg)
{
    if (g_options._force_unicode)
    {
        // Use the lexertl enum operator
        using namespace lexertl;
        using rules_type = basic_rules<char, char32_t>;
        using ugenerator = basic_generator<rules_type, u32state_machine>;
        rules_type rules;
        ulexer lexer;

        lexer._flags = cfg._flags;
        lexer._conditions = std::move(cfg._conditions);

        if (lexer._flags & *config_flags::icase)
            rules.flags(*regex_flags::icase |
                *regex_flags::dot_not_cr_lf);

        rules.push(cfg._param, 1);

        if (g_options._dump == dump::no)
            rules.push("(?s:.)", rules_type::skip());

        ugenerator::build(rules, lexer._sm);
        g_pipeline.emplace_back(std::move(lexer));
    }
    else
    {
        // Use the lexertl enum operator
        using namespace lexertl;
        rules rules;
        lexer lexer;

        lexer._flags = cfg._flags;
        lexer._conditions = std::move(cfg._conditions);

        if (lexer._flags & *config_flags::icase)
            rules.flags(*regex_flags::icase |
                *regex_flags::dot_not_cr_lf);

        rules.push(cfg._param, 1);

        if (g_options._dump == dump::no)
            rules.push("(?s:.)", rules::skip());

        generator::build(rules, lexer._sm);
        g_pipeline.emplace_back(std::move(lexer));
    }
}

static void queue_parser(config& cfg)
{
    if (g_options._force_unicode)
    {
        uparser parser;
        config_state state;

        parser._flags = cfg._flags;
        parser._conditions = std::move(cfg._conditions);
        g_curr_uparser = &parser;

        if (g_config_parser._gsm.empty())
            build_config_parser();

        state.parse(cfg._flags, cfg._param);
        g_options._rule_print |= state._print;

        if (parser._gsm.empty())
        {
            ulexer lexer;

            lexer._flags = parser._flags;
            lexer._conditions = std::move(parser._conditions);
            lexer._sm.swap(parser._lsm);
            g_pipeline.emplace_back(std::move(lexer));
        }
        else
            g_pipeline.emplace_back(std::move(parser));
    }
    else
    {
        parser parser;
        config_state state;

        parser._flags = cfg._flags;
        parser._conditions = std::move(cfg._conditions);
        g_curr_parser = &parser;

        if (g_config_parser._gsm.empty())
            build_config_parser();

        state.parse(cfg._flags, cfg._param);
        g_options._rule_print |= state._print;

        if (parser._gsm.empty())
        {
            lexer lexer;

            lexer._flags = parser._flags;
            lexer._conditions = std::move(parser._conditions);
            lexer._sm.swap(parser._lsm);
            g_pipeline.emplace_back(std::move(lexer));
        }
        else
            g_pipeline.emplace_back(std::move(parser));
    }
}

static void queue_regex(config& cfg)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    regex regex;
    boost::regex::flag_type rx_flags{};

    regex._flags = cfg._flags;
    regex._conditions = std::move(cfg._conditions);

    if (regex._flags & *config_flags::icase)
        rx_flags |= boost::regex_constants::icase;

    if (regex._flags & *config_flags::grep)
        rx_flags |= boost::regex_constants::grep;
    else if (regex._flags & *config_flags::egrep)
        rx_flags |= boost::regex_constants::egrep;
    else
        rx_flags |= boost::regex_constants::ECMAScript;

    regex._rx.assign(cfg._param, rx_flags);
    g_pipeline.emplace_back(std::move(regex));
}

static void queue_text(config& cfg)
{
    text text;

    text._flags = cfg._flags;
    text._conditions = std::move(cfg._conditions);
    text._text = cfg._param;
    g_pipeline.emplace_back(std::move(text));
}

static void queue_word_list(config& cfg, std::size_t& word_list_idx)
{
    word_list words;
    lexertl::memory_file& mf =
        g_options._word_list_files[word_list_idx++];
    const lexertl::state_machine sm = word_lexer();
    lexertl::citerator iter;

    words._flags = cfg._flags;
    words._conditions = std::move(cfg._conditions);
    mf.open(cfg._param.c_str());

    if (mf.data() == nullptr)
        throw gg_error(std::format("Cannot open {}", cfg._param));

    iter = lexertl::citerator(mf.data(), mf.data() + mf.size(), sm);

    for (; iter->id != 0; ++iter)
    {
        words._list.push_back(iter->view());
    }

    std::ranges::sort(words._list);
    g_pipeline.emplace_back(std::move(words));
}

static void fill_pipeline(std::vector<config>&& configs)
{
    std::size_t word_list_idx = 0;

    // Postponed to allow -i to be processed first.
    for (auto&& cfg : std::move(configs))
    {
        switch (cfg._type)
        {
        case match_type::dfa_regex:
            queue_dfa_regex(cfg);
            break;
        case match_type::parser:
            queue_parser(cfg);
            break;
        case match_type::regex:
            queue_regex(cfg);
            break;
        case match_type::text:
            queue_text(cfg);
            break;
        case match_type::word_list:
            queue_word_list(cfg, word_list_idx);
            break;
        default:
            break;
        }
    }
}

static void parse_colours(const std::string& colours)
{
    parsertl::rules grules;
    parsertl::state_machine gsm;
    lexertl::rules lrules;
    lexertl::state_machine lsm;

    grules.token("VALUE");
    grules.push("start", "list");
    grules.push("list", "item | list ':' item");

    const uint16_t name_val_idx = grules.push("item", "name '=' value");
    // rv flag
    const uint16_t rv_idx = grules.push("item", "'rv'");
    // ne flag
    const uint16_t ne_idx = grules.push("item", "'ne'");

    grules.push("name",
        "'sl' | 'cx' | 'mt' | 'ms' | 'mc' | 'fn' | 'ln' | 'bn' | 'se' | 'wa'");
    grules.push("value", "%empty | VALUE");
    parsertl::generator::build(grules, gsm);

    lrules.push(":", grules.token_id("':'"));
    lrules.push("=", grules.token_id("'='"));
    lrules.push("sl", grules.token_id("'sl'"));
    lrules.push("cx", grules.token_id("'cx'"));
    lrules.push("rv", grules.token_id("'rv'"));
    lrules.push("mt", grules.token_id("'mt'"));
    lrules.push("ms", grules.token_id("'ms'"));
    lrules.push("mc", grules.token_id("'mc'"));
    lrules.push("fn", grules.token_id("'fn'"));
    lrules.push("ln", grules.token_id("'ln'"));
    lrules.push("bn", grules.token_id("'bn'"));
    lrules.push("se", grules.token_id("'se'"));
    lrules.push("ne", grules.token_id("'ne'"));
    lrules.push("wa", grules.token_id("'wa'"));
    lrules.push(R"(\d{1,3}(;\d{1,3}){0,2})", grules.token_id("VALUE"));
    lexertl::generator::build(lrules, lsm);

    lexertl::citerator liter(colours.c_str(),
        colours.c_str() + colours.size(), lsm);
    parsertl::citerator giter(liter, gsm);

    for (; giter->entry.action != parsertl::action::accept &&
        giter->entry.action != parsertl::action::error; ++giter)
    {
        if (giter->entry.param == name_val_idx)
        {
            const auto value = giter.dollar(2).view();

            if (value.empty())
                // Ignore blank value
                continue;

            static std::pair<const char*, std::string&> lookup[] =
            {
                {"sl", g_options._sl_text},
                {"cx", g_options._cx_text},
                {"ms", g_options._ms_text},
                {"mc", g_options._mc_text},
                {"fn", g_options._fn_text},
                {"ln", g_options._ln_text},
                {"bn", g_options._bn_text},
                {"se", g_options._se_text},
                {"wa", g_options._wa_text}
            };
            const auto name = giter.dollar(0).view();

            if (name == "mt")
            {
                g_options._ms_text = std::format("\x1b[{}m", value);
                g_options._mc_text = std::format("\x1b[{}m", value);
            }
            else if (auto iter = std::ranges::find_if(lookup,
                [name](const auto& pair) { return name == pair.first; });
                iter != std::end(lookup))
            {
                iter->second = std::format("\x1b[{}m", value);
            }
        }
        else if (giter->entry.param == rv_idx)
            g_options._rv = true;
        else if (giter->entry.param == ne_idx)
        {
            g_options._ne = true;
        }
    }
}

static std::vector<const char*> to_vector(std::string& grep_options)
{
    std::vector<const char*> ret;
    char* prev = &grep_options[0];
    char* options = prev;

    // Append dummy entry
    ret.push_back(prev + grep_options.size());

    for (; *options; ++options)
    {
        if (strchr(" \t", *options))
        {
            *options = '\0';
            ++options;

            ret.push_back(prev);

            if (!*options)
                break;

            prev = options;
        }
    }

    if (*prev)
        ret.push_back(prev);

    return ret;
}

static std::string env_var(const char* var)
{
    std::string ret;
#ifdef _WIN32
    const DWORD dwSize = ::GetEnvironmentVariableA(var, nullptr, 0);

    if (dwSize)
    {
        // Placate SonarQube (does not like dwSize - 1)
        std::vector<char> buff(dwSize, ' ');

        ::GetEnvironmentVariableA(var, &buff.front(), dwSize);
        ret.assign(&buff.front());
    }
#else
    const char* str = std::getenv(var);

    if (str)
        ret = str;
#endif

    return ret;
}

void show_usage(const std::string& msg = std::string())
{
    std::cerr << msg << usage() << try_help();
    exit(2);
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc == 1)
        {
            show_usage();
        }

        std::vector<config> configs;
        std::vector<std::string> files;
        bool run = true;
        std::string grep_options = env_var("GREP_OPTIONS");
        auto options = to_vector(grep_options);

        read_switches(static_cast<int>(options.size()),
            &options.front(), configs, files);
        parse_colours(env_var("GREP_COLORS"));
        read_switches(argc, argv, configs, files);
        fill_pipeline(std::move(configs));

        // Postponed to allow -r to be processed first as
        // add_pathname() checks g_recursive.
        for (const auto& f : files)
        {
            auto pathnames = split(f.c_str(), ';');

            for (const auto& p : pathnames)
            {
                const char* param = p.data();

                add_pathname(std::string(param, param + p.size()),
                    g_options._pathnames);
            }
        }

        if (g_options._show_filename == show_filename::undefined &&
            g_options._pathnames.size() == 1)
        {
            auto iter = g_options._pathnames.begin();

            if (iter->second._negative.size() +
                iter->second._positive.size() == 1)
            {
                g_options._show_filename = show_filename::no;
            }
        }

        if (g_options._dump_argv)
        {
            std::cout << "argv[] =\n";

            for (int idx = 0; idx != argc; ++idx)
            {
                std::cout << "  " << argv[idx] << output_nl;
            }

            return 0;
        }

        if (g_options._dump != dump::no)
        {
            for (const auto& v : g_pipeline)
            {
                if (static_cast<match_type>(v.index()) == match_type::lexer &&
                    !g_options._force_unicode)
                {
                    const auto& l = std::get<lexer>(v);

                    if (g_options._dump == dump::dot)
                        lexertl::dot::dump(l._sm, lexertl::rules(), std::cout);
                    else
                        lexertl::debug::dump(l._sm, std::cout);
                }
            }

            return 0;
        }

        if (g_options._show_version)
        {
            std::cout << "gram_grep " << g_version_string;
            std::cout << output_nl;
            return 0;
        }

        if (g_pipeline.empty())
        {
            show_usage();
        }

        if (g_options._pathname_only != pathname_only::no &&
            g_options._show_count)
        {
            throw gg_error("Cannot combine -l and --count.");
        }

        if (g_options._perform_output && g_options._pathnames.empty())
            throw gg_error("Cannot combine stdin with -o.");

        if (!g_options._replace.empty() && g_options._modify)
            throw gg_error("Cannot combine --replace with grammar "
                "actions that modify the input.");

        if (!g_options._startup.empty())
        {
            if (::system(g_options._startup.c_str()))
            {
                output_text_nl(std::cerr, is_a_tty(stderr),
                    g_options._wa_text.c_str(),
                    std::format("{}Failed to execute {}.",
                        gg_text(),
                        g_options._startup));
                run = false;
            }
        }

        if (run)
        {
            if (g_options._pathnames.empty())
            {
                std::string cin;

                process_file(std::string(), &cin);
            }
            else
                process();
        }

        if (!g_options._shutdown.empty())
            if (::system(g_options._shutdown.c_str()))
            {
                output_text_nl(std::cerr, is_a_tty(stderr), run ?
                    g_options._ms_text.c_str() :
                    g_options._wa_text.c_str(),
                    std::format("{}Failed to execute {}.",
                        gg_text(),
                        g_options._shutdown));
            }

        if (g_options._summary)
        {
            std::cout << "Matches: " << g_hits << "    Matching files: " <<
                g_files << "    Total files searched: " << g_searched <<
                output_nl;
        }

        return g_hits ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        output_text_nl(std::cerr, is_a_tty(stderr),
            g_options._wa_text.c_str(),
            std::format("{}{}",
                gg_text(),
                e.what()));
        return 1;
    }
}
