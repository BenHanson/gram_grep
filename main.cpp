// A grep program that allows search by grammar or by lexer spec only
// as well as the conventional way.

#include "pch.h"

#include "args.hpp"
#include "colours.hpp"
#include <lexertl/debug.hpp>
#include <lexertl/dot.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <parsertl/generator.hpp>
#include "gg_error.hpp"
#include <parsertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include "output.hpp"
#include "parser.hpp"
#include <queue>
#include "search.hpp"
#include <type_traits>
#include "version.hpp"

extern std::string build_text(const std::string& input,
    const capture_vector& captures);
extern std::string unescape(const std::string_view& vw);

enum class file_type
{
    ansi, utf8, utf16, utf16_flip
};

namespace fs = std::filesystem;

bool g_byte_count = false;
std::regex g_capture_rx(R"(\$\d+)");
std::string g_checkout;
bool g_colour = false;
condition_map g_conditions;
condition_parser g_condition_parser;
config_parser g_config_parser;
parser* g_curr_parser = nullptr;
uparser* g_curr_uparser = nullptr;
bool g_dot = false;
bool g_dump = false;
bool g_dump_argv = false;
wildcards g_exclude;
wildcards g_exclude_dirs;
std::string g_exec;
std::size_t g_files = 0;
unsigned int g_flags = 0;
bool g_force_unicode = false;
bool g_force_write = false;
show_filename g_show_filename = show_filename::undefined;
std::size_t g_hits = 0;
bool g_initial_tab = false;
std::string g_label;
bool g_line_buffered = false;
bool g_line_numbers = false;
bool g_line_numbers_parens = false;
std::size_t g_max_count = 0;
bool g_modify = false; // Set when grammar has modifying operations
bool g_null_data = false;
bool g_only_matching = false;
bool g_output = false;
bool g_pathname_only = false;
bool g_pathname_only_negated = false;
// maps path to pair.
// pair is wildcards and negated wildcards
std::map<std::string, wildcards> g_pathnames;
pipeline g_pipeline;
std::string g_print;
bool g_print_null = false;
bool g_recursive = false;
std::string g_replace;
bool g_rule_print = false;
std::size_t g_searched = 0;
bool g_show_count = false;
std::string g_startup;
std::string g_shutdown;
bool g_summary = false;
bool g_show_version = false;
bool g_whole_match = false;
std::vector<lexertl::memory_file> g_word_list_files;
bool g_writable = false;

file_type fetch_file_type(const char* data, std::size_t size)
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
    }

    return type;
}

file_type load_file(std::vector<unsigned char>& utf8,
    const char*& data_first, const char*& data_second,
    std::vector<match>& ranges)
{
    const std::size_t size = data_second - data_first;
    file_type type = fetch_file_type(data_first, size);

    switch (type)
    {
    case file_type::utf16:
    {
        using in_iter =
            lexertl::basic_utf16_in_iterator<const uint16_t*, int32_t>;
        using out_iter = lexertl::basic_utf8_out_iterator<in_iter>;
        auto first = std::bit_cast<const uint16_t*>(data_first + 2);
        auto second = std::bit_cast<const uint16_t*>(data_second);
        in_iter in(first, second);
        in_iter in_end(second, second);
        out_iter out(in, in_end);
        out_iter out_end(in_end, in_end);

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

static bool process_matches(const std::vector<match>& ranges,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements,
    std::map<std::pair<std::size_t, std::size_t>,
    std::string>& temp_replacements,
    const bool negate, const capture_vector& captures,
    const char* data_first, const char* data_second,
    lexertl::state_machine& cap_sm, const std::string& pathname,
    std::size_t& hits)
{
    bool finished = false;
    const auto& tuple = ranges.front();
    auto iter = ranges.rbegin();
    auto end = ranges.rend();

    replacements.insert(temp_replacements.begin(), temp_replacements.end());
    temp_replacements.clear();

    // Only allow g_replace if g_modify (grammar actions) not set
    if (g_output && !g_modify)
    {
        const char* first = negate ? iter->_first : iter->_second;

        if (first < tuple._first || first > tuple._second)
        {
            if (g_colour)
                std::cerr << szYellowText;

            std::cerr << "Cannot replace text when source "
                "is not contained in original string.\n";

            if (g_colour)
                std::cerr << szDefaultText;

            return true;
        }
        else
        {
            // Replace with g_replace.
            const char* second = iter->_eoi;

            if (captures.empty())
                replacements[std::make_pair(first - data_first,
                    second - first)] = g_replace;
            else
            {
                std::string replace;
                bool skip = false;

                if (cap_sm.empty())
                {
                    lexertl::rules rules;

                    rules.push(R"(\$\d)", 1);
                    lexertl::generator::build(rules, cap_sm);
                }

                lexertl::citerator i(g_replace.c_str(),
                    g_replace.c_str() + g_replace.size(), cap_sm);
                lexertl::citerator e;

                for (; i != e; ++i)
                {
                    if (i->id == 1)
                    {
                        const std::size_t idx = atoi(i->first + 1);

                        if (idx < captures.size())
                            replace += captures[idx].front();
                        else
                        {
                            if (g_colour)
                                std::cerr << szYellowText;

                            std::cerr << "Capture $" << idx <<
                                " is out of range.\n";

                            if (g_colour)
                                std::cerr << szDefaultText;

                            skip = true;
                        }
                    }
                    else
                        replace.push_back(*i->first);
                }

                if (!skip)
                    replacements[std::make_pair(first - data_first,
                        second - first)] = replace;
            }
        }
    }

    for (; iter != end; ++iter)
    {
        const char* first = iter->_second;

        if (first >= tuple._first && first <= tuple._eoi)
        {
            const char* curr = iter->_first;
            const char* eoi = data_second;

            if (!g_pathname_only_negated && !g_show_count &&
                g_print.empty() && !g_rule_print &&
                !(g_show_filename == show_filename::no && !g_pathname_only))
            {
                if (g_colour)
                    std::cout << g_fn_text;

                if (pathname.empty())
                    std::cout << g_label;
                else
                    std::cout << pathname;

                if (g_colour)
                    std::cout << szDefaultText;

                if (g_print_null)
                    std::cout << '\0';

                if (g_colour)
                    std::cout << g_se_text;

                if (g_line_numbers_parens)
                    std::cout << '(';
                else
                    std::cout << ':';

                if (g_colour)
                    std::cout << szDefaultText;
            }

            if (g_pathname_only)
            {
                std::cout << output_nl;
                finished = true;
                break;
            }
            else if (!g_print.empty())
            {
                std::cout << build_text(g_print, captures);
            }
            else if (!g_exec.empty())
            {
                const std::string cmd = build_text(g_exec, captures);

                std::cout << "Executing: " << cmd << '\n';
                std::cout << exec_ret(cmd);
            }
            else if (!g_pathname_only_negated &&
                !g_show_count && !g_rule_print)
            {
                const auto count = std::count(data_first, curr, '\n');

                if (!g_only_matching && !g_whole_match)
                {
                    if (count == 0)
                        curr = data_first;
                    else
                        for (; *(curr - 1) != '\n'; --curr);
                }

                if (!pathname.empty())
                {
                    if (g_line_numbers)
                    {
                        if (g_colour)
                            std::cout << g_ln_text;

                        std::cout << 1 + count;

                        if (g_colour)
                            std::cout << szDefaultText;

                        if (g_colour)
                            std::cout << g_se_text;

                        if (g_line_numbers_parens)
                            std::cout << ')';

                        std::cout << ':';

                        if (g_colour)
                            std::cout << szDefaultText;
                    }

                    if (g_byte_count)
                    {
                        if (g_colour)
                            std::cout << g_bn_text;

                        std::cout << curr - data_first;

                        if (g_colour)
                            std::cout << g_se_text;

                        std::cout << ':';

                        if (g_colour)
                            std::cout << szDefaultText;
                    }

                    if (g_colour)
                        std::cout << szDefaultText;
                }

                if (g_initial_tab)
                    std::cout << '\t';

                if (g_whole_match)
                {
                    if (g_colour)
                        std::cout << g_ms_text;

                    std::cout << iter->view() << output_nl;

                    if (g_colour)
                        std::cout << szDefaultText;
                }
                else if (g_only_matching)
                {
                    if (g_colour)
                        std::cout << g_ms_text;

                    for (; curr != iter->_eoi &&
                        *curr != '\r' && *curr != '\n'; ++curr)
                    {
                        std::cout << *curr;
                    }

                    if (g_colour)
                        std::cout << szDefaultText;
                }
                else
                {
                    for (; curr != eoi && *curr != '\r' && *curr != '\n'; ++curr)
                    {
                        if (g_colour)
                        {
                            if (curr == iter->_first)
                                std::cout << g_ms_text;
                            else if (curr == iter->_eoi)
                                std::cout << szDefaultText;
                        }

                        std::cout << *curr;
                    }

                    if (g_colour && (*curr == '\r' || *curr == '\n'))
                        std::cout << szDefaultText;
                }
            }

            if (!g_pathname_only_negated && !g_show_count &&
                g_print.empty() && !g_rule_print)
            {
                std::cout << output_nl;
            }

            ++hits;
            break;
        }
    }

    if (hits == g_max_count)
        finished = true;

    return finished;
}

static void perform_output(const std::size_t hits, const std::string& pathname,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements,
    const char* data_first, const char* data_second, lexertl::memory_file& mf,
    const file_type type, const std::size_t size)
{
    const auto perms = fs::status(pathname.c_str()).permissions();

    ++g_files;
    g_hits += hits;

    if ((perms & fs::perms::owner_write) != fs::perms::owner_write)
    {
        // Read-only
        if (!g_checkout.empty())
        {
            std::string checkout = g_checkout;

            if (const std::size_t index = checkout.find("$1");
                index != std::string::npos)
            {
                checkout.erase(index, 2);
                checkout.insert(index, pathname);
            }

            if (::system(checkout.c_str()) != 0)
                throw gg_error(std::format("Failed to execute {}.\n", g_checkout));
        }
        else if (g_force_write)
            fs::permissions(pathname.c_str(), perms | fs::perms::owner_write);
    }

    if (!replacements.empty())
    {
        std::string content(data_first, data_second);

        for (auto iter = replacements.rbegin(), end = replacements.rend();
            iter != end; ++iter)
        {
            content.erase(iter->first.first, iter->first.second);
            content.insert(iter->first.first, iter->second);
        }

        replacements.clear();
        // In case the memory_file is still open
        mf.close();

        if ((fs::status(pathname.c_str()).permissions() &
            fs::perms::owner_write) != fs::perms::owner_write)
        {
            if (g_colour)
                std::cerr << szYellowText;

            std::cerr << pathname << " is read only.\n";

            if (g_colour)
                std::cerr << szDefaultText;
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
    if (g_writable && (fs::status(pathname).permissions() &
        fs::perms::owner_write) == fs::perms::none)
    {
        return;
    }

    lexertl::memory_file mf(pathname.c_str());
    std::vector<unsigned char> utf8;
    const char* data_first = nullptr;
    const char* data_second = nullptr;
    file_type type = file_type::ansi;
    std::size_t hits = 0;
    std::vector<match> ranges;
    static lexertl::state_machine cap_sm;
    capture_vector captures;
    std::stack<std::string> matches;
    std::map<std::pair<std::size_t, std::size_t>, std::string> replacements;
    bool finished = false;

    if (!mf.data() && !cin)
    {
        if (g_colour)
            std::cerr << szYellowText;

        std::cerr << "Error: failed to open " << pathname << ".\n";

        if (g_colour)
            std::cerr << szDefaultText;

        return;
    }

    if (cin)
    {
        std::ostringstream ss;

        ss << std::cin.rdbuf();
        *cin = ss.str();
        data_first = cin->c_str();
        data_second = data_first + cin->size();
    }
    else
    {
        data_first = mf.data();
        data_second = data_first + mf.size();
    }

    type = load_file(utf8, data_first, data_second, ranges);

    if (type == file_type::utf16 || type == file_type::utf16_flip)
        // No need for original data
        mf.close();

    do
    {
        std::map<std::pair<std::size_t, std::size_t>, std::string>
            temp_replacements;
        auto [success, negate] =
            search(ranges, data_first, matches, temp_replacements, captures);

        if (success)
            finished = process_matches(ranges, replacements, temp_replacements,
                negate, captures, data_first, data_second, cap_sm, pathname,
                hits);

        const auto& old = ranges.back();

        ranges.pop_back();

        if (!ranges.empty())
        {
            // Cleardown any stale strings in matches.
            // First makes sure current range is not from the same
            // string (in matches) as the last.
            if (const auto& curr = ranges.back();
                !matches.empty() &&
                (old._first < curr._first || old._first > curr._eoi))
            {
                while (!matches.empty() &&
                    old._first >= matches.top().c_str() &&
                    old._eoi <= matches.top().c_str() + matches.top().size())
                {
                    matches.pop();
                }
            }

            // Start searching from end of last match
            ranges.back()._first = ranges.back()._second;
        }
    } while (!finished && !ranges.empty());

    if (hits)
    {
        perform_output(hits, pathname, replacements, data_first, data_second,
            mf, type, utf8.size());
    }

    if (g_show_count)
    {
        std::cout << pathname << ':' << hits << output_nl;
    }

    if (g_pathname_only_negated && !hits)
    {
        std::cout << pathname << output_nl;
    }

    ++g_searched;
}

static void process_file(const std::string& pathname, const wildcards &wcs)
{
    try
    {
        const char* filename = pathname.c_str() +
            pathname.rfind(fs::path::preferred_separator) + 1;
        bool skip = !g_exclude._negative.empty();

        for (const auto& wc : g_exclude._negative)
        {
            if (!wc.match(filename))
            {
                skip = false;
                break;
            }
        }

        if (!skip)
        {
            for (const auto& wc : g_exclude._positive)
            {
                if (wc.match(filename))
                {
                    skip = true;
                    break;
                }
            }
        }

        if (!skip)
        {
            bool process = !wcs._negative.empty();

            for (const auto& wc : wcs._negative)
            {
                if (!wc.match(pathname))
                {
                    process = false;
                    break;
                }
            }

            if (!process)
            {
                for (const auto& wc : wcs._positive)
                {
                    if (wc.match(pathname))
                    {
                        process = true;
                        break;
                    }
                }
            }

            if (process)
                process_file(pathname);
        }
    }
    catch (const std::exception& e)
    {
        if (g_colour)
            std::cerr << szYellowText;

        std::cerr << e.what() << output_nl;

        if (g_colour)
            std::cerr << szDefaultText;
    }
}

bool include_dir(const std::string& path)
{
    return (g_exclude_dirs._negative.empty() ||
        std::ranges::any_of(g_exclude_dirs._negative,
        [&path](const auto& wc)
        {
            return wc.match(path);
        })) &&
    std::ranges::none_of(g_exclude_dirs._positive,
        [&path](const auto& wc)
        {
            return wc.match(path);
        });
}

static void process()
{
    std::queue<std::pair<std::string, const wildcards*>> queue;
    
    for (const auto& [path, wcs] : g_pathnames)
    {
        queue.emplace(path, &wcs);
    }

    for (; !queue.empty(); queue.pop())
    {
        const auto& [path, wcs] = queue.front();

        for (auto iter = fs::directory_iterator(path,
            fs::directory_options::skip_permission_denied),
            end = fs::directory_iterator(); iter != end; ++iter)
        {
            const auto& p = iter->path();

            // Don't throw if there is a Unicode pathname
            const std::string pathname =
                reinterpret_cast<const char*>(p.u8string().c_str());

            if (fs::is_directory(p))
            {
                if (g_recursive && include_dir(pathname.
                    substr(pathname.rfind(fs::path::preferred_separator) + 1)))
                {
                    queue.emplace(pathname, wcs);
                }
            }
            else 
            {
                if ((g_writable && (fs::status(p).permissions() &
                    fs::perms::owner_write) == fs::perms::none) ||
                    // Skip zero length files
                    fs::file_size(p) == 0)
                {
                    continue;
                }

                process_file(pathname, *wcs);
            }
        }
    }
}

void add_pathname(std::string pn,
    std::map<std::string, wildcards>& map)
{
    const std::size_t wc_idx = pn.find_first_of("*?[");
    const std::size_t sep_idx = pn.rfind(fs::path::preferred_separator,
        wc_idx);
    const bool negate = pn[0] == '!';
    const std::string path = sep_idx == std::string::npos ? "." :
        pn.substr(negate ? 1 : 0, sep_idx + (negate ? 0 : 1));
    auto& wcs = map[path];

    if (g_show_filename == show_filename::undefined && (g_recursive ||
        wc_idx != std::string::npos || negate))
    {
        g_show_filename = show_filename::yes;
    }

    if (sep_idx == std::string::npos)
    {
        if (!((!negate && wc_idx == 0) || (negate && wc_idx == 1)))
        {
            if (g_recursive)
                pn.insert(negate ? 1 : 0, std::string(1, '*') +
                    static_cast<char>(fs::path::preferred_separator));
            else
                pn.insert(negate ? 1 : 0, path +
                    static_cast<char>(fs::path::preferred_separator));
        }
    }
    else if (g_recursive &&
        !((!negate && wc_idx == 0) || (negate && wc_idx == 1)))
    {
        pn = std::string(1, '*') + pn.substr(sep_idx);

        if (negate)
            pn = '!' + pn;
    }

    if (negate)
        wcs._negative.emplace_back(pn, is_windows());
    else
        wcs._positive.emplace_back(pn, is_windows());
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

static void fill_pipeline(std::vector<config>&& configs)
{
    std::size_t word_list_idx = 0;

    // Postponed to allow -i to be processed first.
    for (auto&& config : std::move(configs))
    {
        switch (config._type)
        {
        case match_type::dfa_regex:
        {
            if (g_force_unicode)
            {
                using rules_type = lexertl::basic_rules<char, char32_t>;
                using generator = lexertl::basic_generator<rules_type,
                    lexertl::u32state_machine>;
                rules_type rules;
                ulexer lexer;

                lexer._flags = config._flags;
                lexer._conditions = std::move(config._conditions);

                if (lexer._flags & config_flags::icase)
                    rules.flags(*lexertl::regex_flags::icase |
                        *lexertl::regex_flags::dot_not_cr_lf);

                rules.push(config._param, 1);

                if (!g_dump)
                    rules.push(".{+}[\r\n]", rules_type::skip());

                generator::build(rules, lexer._sm);
                g_pipeline.emplace_back(std::move(lexer));
            }
            else
            {
                lexertl::rules rules;
                lexer lexer;

                lexer._flags = config._flags;
                lexer._conditions = std::move(config._conditions);

                if (lexer._flags & config_flags::icase)
                    rules.flags(*lexertl::regex_flags::icase |
                        *lexertl::regex_flags::dot_not_cr_lf);

                rules.push(config._param, 1);

                if (!g_dump)
                    rules.push(".{+}[\r\n]", lexertl::rules::skip());

                lexertl::generator::build(rules, lexer._sm);
                g_pipeline.emplace_back(std::move(lexer));
            }

            break;
        }
        case match_type::parser:
        {
            if (g_force_unicode)
            {
                uparser parser;
                config_state state;

                parser._flags = config._flags;
                parser._conditions = std::move(config._conditions);
                g_curr_uparser = &parser;

                if (g_config_parser._gsm.empty())
                    build_config_parser();

                state.parse(config._flags, config._param);
                g_rule_print |= state._print;

                if (parser._gsm.empty())
                {
                    ulexer lexer;

                    lexer._flags = parser._flags;
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

                parser._flags = config._flags;
                parser._conditions = std::move(config._conditions);
                g_curr_parser = &parser;

                if (g_config_parser._gsm.empty())
                    build_config_parser();

                state.parse(config._flags, config._param);
                g_rule_print |= state._print;

                if (parser._gsm.empty())
                {
                    lexer lexer;

                    lexer._flags = parser._flags;
                    lexer._sm.swap(parser._lsm);
                    g_pipeline.emplace_back(std::move(lexer));
                }
                else
                    g_pipeline.emplace_back(std::move(parser));
            }

            break;
        }
        case match_type::regex:
        {
            regex regex;
            std::regex::flag_type rx_flags{};

            regex._flags = config._flags;
            regex._conditions = std::move(config._conditions);

            if (regex._flags & config_flags::icase)
                rx_flags |= std::regex_constants::icase;

            if (regex._flags & config_flags::grep)
                rx_flags |= std::regex_constants::grep;
            else if (regex._flags & config_flags::egrep)
                rx_flags |= std::regex_constants::egrep;
            else
                rx_flags |= std::regex_constants::ECMAScript;

            regex._rx.assign(config._param, rx_flags);
            g_pipeline.emplace_back(std::move(regex));
            break;
        }
        case match_type::text:
        {
            text text;

            text._flags = config._flags;
            text._conditions = std::move(config._conditions);
            text._text = config._param;
            g_pipeline.emplace_back(std::move(text));
            break;
        }
        case match_type::word_list:
        {
            word_list words;
            lexertl::memory_file& mf = g_word_list_files[word_list_idx];
            const lexertl::state_machine sm = word_lexer();
            lexertl::citerator iter;

            words._flags = config._flags;
            words._conditions = std::move(config._conditions);
            mf.open(config._param.c_str());

            if (mf.data() == nullptr)
                throw gg_error(std::format("Cannot open {}", config._param));

            iter = lexertl::citerator(mf.data(), mf.data() + mf.size(), sm);

            for (; iter->id != 0; ++iter)
            {
                words._list.push_back(iter->view());
            }

            std::ranges::sort(words._list);
            g_pipeline.emplace_back(std::move(words));
            break;
        }
        default:
            break;
        }
    }
}

void parse_colours(const std::string& colours)
{
    parsertl::rules grules;
    parsertl::state_machine gsm;
    lexertl::rules lrules;
    lexertl::state_machine lsm;

    grules.token("VALUE");
    grules.push("start", "list");
    grules.push("list", "item | list ':' item");

    const uint16_t idx = grules.push("item", "name '=' value");

    grules.push("name",
        "'bn' | 'cx' | 'fn' | 'ln' | 'mc' | 'ms' | 'se' | 'sl'");
    grules.push("value", "%empty | VALUE");
    parsertl::generator::build(grules, gsm);

    lrules.push(":", grules.token_id("':'"));
    lrules.push("=", grules.token_id("'='"));
    lrules.push("bn", grules.token_id("'bn'"));
    lrules.push("cx", grules.token_id("'cx'"));
    lrules.push("fn", grules.token_id("'fn'"));
    lrules.push("ln", grules.token_id("'ln'"));
    lrules.push("mc", grules.token_id("'mc'"));
    lrules.push("ms", grules.token_id("'ms'"));
    lrules.push("se", grules.token_id("'se'"));
    lrules.push("sl", grules.token_id("'sl'"));
    lrules.push(R"(\d{1,3}(;\d{1,3}){0,2})", grules.token_id("VALUE"));
    lexertl::generator::build(lrules, lsm);

    lexertl::citerator liter(colours.c_str(),
        colours.c_str() + colours.size(), lsm);
    parsertl::citerator giter(liter, gsm);

    for (; giter->entry.action != parsertl::action::accept &&
        giter->entry.action != parsertl::action::error; ++giter)
    {
        if (giter->entry.param == idx)
        {
            const auto value = giter.dollar(2).view();

            if (value.empty())
                // Ignore blank value
                continue;

            const auto name = giter.dollar(0).view();

            if (name == "bn")
                g_bn_text = std::format("\x1b[{}m\x1b[K", value);
            /*else if (name == "cx")
                ;*/
            else if (name == "fn")
                g_fn_text = std::format("\x1b[{}m\x1b[K", value);
            else if (name == "ln")
                g_ln_text = std::format("\x1b[{}m\x1b[K", value);
            /*else if (name == "mc")
                ;*/
            else if (name == "ms")
                g_ms_text = std::format("\x1b[{}m\x1b[K", value);
            else if (name == "se")
                g_se_text = std::format("\x1b[{}m\x1b[K", value);
            /*else if (name == "sl")
                ;*/
        }
    }
}

std::vector<const char*> to_vector(std::string& grep_options)
{
    std::vector<const char*> ret;
    char* prev = &grep_options.front();
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

std::string env_var(const char* var)
{
    std::string ret;
#ifdef _WIN32
    const DWORD dwSize = ::GetEnvironmentVariableA(var, nullptr, 0);

    if (dwSize)
    {
        ret.resize(dwSize - 1, ' ');
        ::GetEnvironmentVariableA(var, &ret.front(), dwSize);
    }
#else
    const char* str = std::getenv(var);

    if (str)
        ret = str;
#endif

    return ret;
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc == 1)
        {
            show_help();
            return 1;
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

                add_pathname(std::string(param, param + p.size()), g_pathnames);
            }
        }

        if (g_show_filename == show_filename::undefined &&
            g_pathnames.size() == 1)
        {
            auto iter = g_pathnames.begin();

            if (iter->second._negative.size() +
                iter->second._positive.size() == 1)
            {
                g_show_filename = show_filename::no;
            }
        }

        if (g_dump_argv)
        {
            std::cout << "argv[] =\n";

            for (int idx = 0; idx != argc; ++idx)
            {
                std::cout << "  " << argv[idx] << output_nl;
            }

            return 0;
        }

        if (g_dump)
        {
            for (const auto& v : g_pipeline)
            {
                if (static_cast<match_type>(v.index()) == match_type::lexer &&
                    !g_force_unicode)
                {
                    const auto& l = std::get<lexer>(v);

                    if (g_dot)
                        lexertl::dot::dump(l._sm, lexertl::rules(), std::cout);
                    else
                        lexertl::debug::dump(l._sm, std::cout);
                }
            }

            return 0;
        }

        if (g_show_version)
        {
            std::cout << "gram_grep " << g_version_string;
            std::cout << output_nl;
            return 0;
        }

        if (g_pipeline.empty())
            throw gg_error("No actions have been specified.");

        if (g_pathname_only && g_show_count)
            throw gg_error("Cannot combine -l and --count.");

        if (g_output && g_pathnames.empty())
            throw gg_error("Cannot combine stdin with -o.");

        if (!g_replace.empty() && g_modify)
            throw gg_error("Cannot combine --replace with grammar "
                "actions that modify the input.");

        if (!g_startup.empty())
        {
            if (::system(g_startup.c_str()))
            {
                if (g_colour)
                    std::cerr << szYellowText;

                std::cerr << "Failed to execute " << g_startup << ".\n";

                if (g_colour)
                    std::cerr << szDefaultText;
                run = false;
            }
        }

        if (run)
        {
            if (g_pathnames.empty())
            {
                std::string cin;

                process_file(std::string(), &cin);
            }
            else
                process();
        }

        if (!g_shutdown.empty())
            if (::system(g_shutdown.c_str()))
            {
                if (g_colour)
                    std::cerr << (run ? g_ms_text : szYellowText);

                std::cerr << "Failed to execute " << g_shutdown << ".\n";

                if (g_colour)
                    std::cerr << szDefaultText;
            }

        if (g_summary)
        {
            std::cout << "Matches: " << g_hits << "    Matching files: " <<
                g_files << "    Total files searched: " << g_searched <<
                output_nl;
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        if (g_colour)
            std::cerr << szDarkRedText;

        std::cerr << e.what() << output_nl;

        if (g_colour)
            std::cerr << szDefaultText;

        return 1;
    }
}
