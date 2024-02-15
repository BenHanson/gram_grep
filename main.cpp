// A grep program that allows search by grammar or by lexer spec only
// as well as the conventional way.

#include "pch.h"

#include <lexertl/debug.hpp>
#include <filesystem>
#include <fstream>
#include "gg_error.hpp"
#include <lexertl/memory_file.hpp>
#include "parser.hpp"
#include "search.hpp"
#include <wildcardtl/wildcard.hpp>

enum class file_type
{
    ansi, utf8, utf16, utf16_flip
};

namespace fs = std::filesystem;

struct config
{
    match_type _type;
    std::string _param;
    unsigned int _flags = config_flags::none;

    config(const match_type type, const std::string& param, const unsigned int flags) :
        _type(type),
        _param(param),
        _flags(flags)
    {
    }
};

config_parser g_config_parser;
pipeline g_pipeline;
std::pair<std::vector<wildcardtl::wildcard>,
    std::vector<wildcardtl::wildcard>> g_exclude;
bool g_show_hits = false;
bool g_icase = false;
bool g_dump = false;
bool g_pathname_only = false;
bool g_force_unicode = false;
bool g_modify = false; // Set when grammar has modifying operations
bool g_output = false;
bool g_recursive = false;
std::string g_replace;
std::string g_checkout;
parser* g_curr_parser = nullptr;
uparser* g_curr_uparser = nullptr;
std::map<std::string, std::pair<std::vector<wildcardtl::wildcard>,
    std::vector<wildcardtl::wildcard>>> g_pathnames;
std::size_t g_hits = 0;
std::size_t g_files = 0;
std::size_t g_searched = 0;
bool g_writable = false;
std::string g_startup;
std::string g_shutdown;
bool g_force_write = false;
std::regex g_capture_rx(R"(\$\d+)");

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
    const bool negate, const std::vector<std::string>& captures,
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
            std::cerr << "Cannot replace text when source "
                "is not contained in original string.\n";
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
                            replace += captures[idx];
                        else
                        {
                            std::cerr << "Capture $" << idx <<
                                " is out of range.\n";
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

            if (!g_show_hits)
                std::cout << pathname;

            if (g_pathname_only)
            {
                std::cout << std::endl;
                finished = true;
                break;
            }
            else if (!g_show_hits)
            {
                const auto count = std::count(data_first, curr, '\n');

                if (!pathname.empty())
                    std::cout << '(' << 1 + count << "):";

                if (count == 0)
                    curr = data_first;
                else
                    for (; *(curr - 1) != '\n'; --curr);

                for (; curr != eoi && *curr != '\r' && *curr != '\n'; ++curr)
                {
                    std::cout << *curr;
                }
            }

            if (!g_show_hits)
            {
                // Flush buffer, to give fast feedback to user
                std::cout << std::endl;
            }

            ++hits;
            break;
        }
    }

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
                std::cerr << "Failed to execute " << g_checkout << ".\n";
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
            std::cerr << pathname << " is read only.\n";
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
    std::vector<std::string> captures;
    std::stack<std::string> matches;
    std::map<std::pair<std::size_t, std::size_t>, std::string> replacements;
    bool finished = false;

    if (!mf.data() && !cin)
    {
        std::cerr << "Error: failed to open " << pathname << ".\n";
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

        if (g_show_hits)
            std::cout << pathname << ": " << hits << " hits." << std::endl;
    }

    ++g_searched;
}

static void process_file(const fs::path& path,
    const std::pair<std::vector<wildcardtl::wildcard>,
    std::vector<wildcardtl::wildcard>>& include)
{
    // Skip directories
    if (fs::is_directory(path) ||
        (g_writable && (fs::status(path).permissions() &
            fs::perms::owner_write) == fs::perms::none))
    {
        return;
    }

    const auto pathname = path.string();

    // Skip zero length files
    if (fs::file_size(pathname) == 0)
        return;

    bool skip = !g_exclude.second.empty();

    for (const auto& wc : g_exclude.second)
    {
        if (!wc.match(pathname))
        {
            skip = false;
            break;
        }
    }

    if (!skip)
    {
        for (const auto& wc : g_exclude.first)
        {
            if (wc.match(pathname))
            {
                skip = true;
                break;
            }
        }
    }

    if (!skip)
    {
        bool process = !include.second.empty();

        for (const auto& wc : include.second)
        {
            if (wc.match(pathname))
            {
                process = false;
                break;
            }
        }

        if (!process)
        {
            for (const auto& wc : include.first)
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

static void process()
{
    for (const auto& [pathname, pair] : g_pathnames)
    {
        if (g_recursive)
        {
            for (auto iter = fs::recursive_directory_iterator(pathname,
                fs::directory_options::skip_permission_denied),
                end = fs::recursive_directory_iterator(); iter != end; ++iter)
            {
                process_file(iter->path(), pair);
            }
        }
        else
        {
            for (auto iter = fs::directory_iterator(pathname,
                fs::directory_options::skip_permission_denied),
                end = fs::directory_iterator(); iter != end; ++iter)
            {
                process_file(iter->path(), pair);
            }
        }
    }
}

static void show_help()
{
    std::cout << "--help\t\t\tShows help.\n"
        "-checkout\t\t<checkout command (include $1 for pathname)>.\n"
        "-E <regex>\t\tSearch using DFA regex.\n"
        "-Ee <regex>\t\tAs -E but continue search following match.\n"
        "-exclude <wildcard>\tExclude pathnames matching wildcard.\n"
        "-f <config file>\tSearch using config file.\n"
        "-fe <config file>\tAs -f but continue search following match.\n"
        "-force_write\t\tIf a file is read only, force it to be writable.\n"
        "-hits\t\t\tShow hit count per file.\n"
        "-i\t\t\tCase insensitive searching.\n"
        "-l\t\t\tOutput pathname only.\n"
        "-o\t\t\tOutput changes to matching file.\n"
        "-P <regex>\t\tSearch using std::regex.\n"
        "-Pe <regex>\t\tAs -P but continue search following match.\n"
        "-r, -R, --recursive\tRecurse subdirectories.\n"
        "-replace\t\tReplacement literal text.\n"
        "-shutdown\t\t<command to run when exiting>.\n"
        "-startup\t\t<command to run at startup>.\n"
        "-T <text>\t\tSearch for plain text with support for capture ($n) syntax.\n"
        "-Tw <text>\t\tAs -Tw but with whole word matching.\n"
        "-utf8\t\t\tIn the absence of a BOM assume UTF-8\n"
        "-vE <regex>\t\tSearch using DFA regex (negated).\n"
        "-VE <regex>\t\tSearch using DFA regex (all negated).\n"
        "-vf <config file>\tSearch using config file (negated).\n"
        "-Vf <config file>\tSearch using config file (all negated).\n"
        "-vP <regex>\t\tSearch using std::regex (negated).\n"
        "-VP <regex>\t\tSearch using std::regex (all negated).\n"
        "-vT <text>\t\tSearch for plain text with support for capture ($n) syntax (negated).\n"
        "-vTw <text>\t\tAs -vT but with whole word matching.\n"
        "-VT <text>\t\tSearch for plain text with support for capture ($n) syntax (all negated).\n"
        "-VTw <text>\t\tAs -VT but with whole word matching.\n"
        "-writable\t\tOnly process files that are writable.\n"
        "<pathname>...\t\tFiles to search (wildcards supported).\n\n"
        "Config file format:\n\n"
        "<grammar directives>\n"
        "%%\n"
        "<grammar>\n"
        "%%\n"
        "<regex macros>\n"
        "%%\n"
        "<regexes>\n"
        "%%\n\n"
        "Grammar Directives:\n\n"
        "%captures\n"
        "%option caseless\n"
        "%token\n"
        "%left\n"
        "%right\n"
        "%nonassoc\n"
        "%precedence\n"
        "%start\n"
        "%x\n\n"
        "Grammar scripting:\n\n"
        "erase($n);\n"
        "erase($from, $to);\n"
        "erase($from.second, $to.first);\n"
        "insert($n, 'text');\n"
        "insert($n.second, 'text');\n"
        "match = $n;\n"
        "match += $n;\n"
        "match = substr($n, <omit from left>, <omit from right>);\n"
        "match += substr($n, <omit from left>, <omit from right>);\n"
        "replace($n, 'text');\n"
        "replace($from, $to, 'text');\n"
        "replace($from.second, $to.first, 'text');\n"
        "replace_all($n, 'regex', 'text');\n\n"
        "Example:\n\n"
        "%token RawString String\n"
        "%%\n"
        "list: String { match = substr($1, 1, 1); };\n"
        "list: RawString { match = substr($1, 3, 2); };\n"
        "list: list String { match += substr($2, 1, 1); };\n"
        "list: list RawString { match += substr($2, 3, 2); };\n"
        "%%\n"
        "ws [ \\t\\r\\n]+\n"
        "%%\n"
        R"(\"([^"\\\r\n]|\\.)*\"        String)"
        "\n"
        R"(R\"\((?s:.)*?\)\"            RawString)"
        "\n"
        R"('([^'\\\r\n]|\\.)*'          skip())"
        "\n"
        R"({ws}|"//".*|"/*"(?s:.)*?"*/" skip())"
        "\n"
        "%%\n\n"
        "Note that you can pipeline searches by using multiple switches.\n"
        "The searches are run in the order they occur on the command line.\n";
}

bool is_windows()
{
#ifdef WIN32
    return true;
#else
    return false;
#endif
}

void add_pathname(std::string pn,
    std::map<std::string, std::pair<std::vector<wildcardtl::wildcard>,
    std::vector<wildcardtl::wildcard>>>& map)
{
    const std::size_t wc_idx = pn.find_first_of("*?[");
    const std::size_t sep_idx = pn.rfind(fs::path::preferred_separator,
        wc_idx);
    const bool negate = pn[0] == '!';
    const std::string path = sep_idx == std::string::npos ? "." :
        pn.substr(negate ? 1 : 0, sep_idx + 1);
    auto& pair = map[path];

    if (sep_idx == std::string::npos &&
        !((!negate && wc_idx == 0) || (negate && wc_idx == 1)))
    {
        if (g_recursive)
            pn.insert(negate ? 1 : 0, std::string(1, '*') +
                static_cast<char>(fs::path::preferred_separator));
        else
            pn.insert(negate ? 1 : 0, path +
                static_cast<char>(fs::path::preferred_separator));
    }

    if (negate)
        pair.second.emplace_back(&pn[1], pn.c_str() + pn.size(), is_windows());
    else
        pair.first.emplace_back(pn, is_windows());
}

std::vector<std::string_view> split(const char* str, const char c)
{
    std::vector<std::string_view> ret;
    const char* first = str;
    std::size_t count = 0;

    for (; *str; ++str)
    {
        if (*str == c)
        {
            count = str - first;

            if (count > 0)
                ret.emplace_back(first, count);

            first = str + 1;
        }
    }

    count = str - first;

    if (count > 0)
        ret.emplace_back(first, count);

    return ret;
}

static void read_switches(const int argc, const char* const argv[],
    std::vector<config>& configs, std::vector<std::string>& files)
{
    for (int i = 1; i < argc; ++i)
    {
        const char* param = argv[i];

        if (strcmp("-checkout", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
            {
                // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
                // checkout $1
                // *NOTE* $1 is replaced by the pathname
                g_checkout = param;
            }
            else
                throw gg_error("Missing pathname "
                    "following -checkout.");
        }
        else if (strcmp("-dump", param) == 0)
        {
            g_dump = true;
        }
        else if (strcmp("-E", param) == 0)
        {
            // DFA regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::dfa_regex, param, config_flags::none);
            else
                throw gg_error("Missing regex following -E.");
        }
        else if (strcmp("-Ee", param) == 0)
        {
            // DFA regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::dfa_regex, param,
                    config_flags::extend_search);
            else
                throw gg_error("Missing regex following -Ee.");
        }
        else if (strcmp("-exclude", param) == 0)
        {
            ++i;

            if (i < argc)
            {
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    param = p.data();

                    if (*param == '!')
                        g_exclude.second.
                        emplace_back(&param[1],
                            param + p.size(), is_windows());
                    else
                        g_exclude.first.emplace_back(param, param + p.size(),
                            is_windows());
                }
            }
            else
                throw gg_error("Missing wildcard "
                    "following -exclude.");
        }
        else if (strcmp("-f", param) == 0)
        {
            ++i;

            if (i < argc)
            {
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    param = p.data();
                    configs.emplace_back(match_type::parser,
                        std::string(param, param + p.size()), config_flags::none);
                }
            }
            else
                throw gg_error("Missing pathname following -f.");
        }
        else if (strcmp("-fe", param) == 0)
        {
            ++i;

            if (i < argc)
            {
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    param = p.data();
                    configs.emplace_back(match_type::parser,
                        std::string(param, param + p.size()),
                        config_flags::extend_search);
                }
            }
            else
                throw gg_error("Missing pathname following -fe.");
        }
        else if (strcmp("-force_write", param) == 0)
            g_force_write = true;
        else if (strcmp("--help", param) == 0)
        {
            show_help();
            exit(0);
        }
        else if (strcmp("-hits", param) == 0)
            g_show_hits = true;
        else if (strcmp("-i", param) == 0)
            g_icase = true;
        else if (strcmp("-l", param) == 0)
            g_pathname_only = true;
        else if (strcmp("-o", param) == 0)
            g_output = true;
        else if (strcmp("-P", param) == 0)
        {
            // Perl style regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::regex, param, config_flags::none);
            else
                throw gg_error("Missing regex following -P.");
        }
        else if (strcmp("-Pe", param) == 0)
        {
            // Perl style regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::regex, param,
                    config_flags::extend_search);
            else
                throw gg_error("Missing regex following -Pe.");
        }
        else if (strcmp("-r", param) == 0 || strcmp("-R", param) == 0 ||
            strcmp("--recursive", param) == 0)
        {
            g_recursive = true;
        }
        else if (strcmp("-replace", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
                g_replace = param;
            else
                throw gg_error("Missing text "
                    "following -replacement.");
        }
        else if (strcmp("-shutdown", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
            {
                // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
                // /delete /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep /noprompt
                g_shutdown = param;
            }
            else
                throw gg_error("Missing pathname "
                    "following -shutdown.");
        }
        else if (strcmp("-startup", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
            {
                // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
                // workspace /new /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep
                // /noprompt
                g_startup = param;
            }
            else
                throw gg_error("Missing pathname "
                    "following -startup.");
        }
        else if (strcmp("-T", param) == 0)
        {
            // Text
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::text, param, config_flags::none);
            else
                throw gg_error("Missing regex following -T.");
        }
        else if (strcmp("-Tw", param) == 0)
        {
            // Text
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::text, param, config_flags::whole_word);
            else
                throw gg_error("Missing regex following -T.");
        }
        else if (strcmp("-vE", param) == 0)
        {
            // DFA regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::dfa_regex, param,
                    config_flags::negate);
            else
                throw gg_error("Missing regex following -vE.");
        }
        else if (strcmp("-VE", param) == 0)
        {
            // DFA regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::dfa_regex, param,
                    config_flags::negate | config_flags::negate_all);
            else
                throw gg_error("Missing regex following -VE.");
        }
        else if (strcmp("-vf", param) == 0)
        {
            ++i;

            if (i < argc)
            {
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    param = p.data();
                    configs.emplace_back(match_type::parser,
                        std::string(param, param + p.size()),
                        config_flags::negate);
                }
            }
            else
                throw gg_error("Missing pathname following -vf.");
        }
        else if (strcmp("-Vf", param) == 0)
        {
            ++i;

            if (i < argc)
            {
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    param = p.data();
                    configs.emplace_back(match_type::parser,
                        std::string(param, param + p.size()),
                        config_flags::negate | config_flags::negate_all);
                }
            }
            else
                throw gg_error("Missing pathname following -Vf.");
        }
        else if (strcmp("-vP", param) == 0)
        {
            // Perl style regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::regex, param, config_flags::negate);
            else
                throw gg_error("Missing regex following -vP.");
        }
        else if (strcmp("-VP", param) == 0)
        {
            // Perl style regex
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::regex, param,
                    config_flags::negate | config_flags::negate_all);
            else
                throw gg_error("Missing regex following -VP.");
        }
        else if (strcmp("-vT", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::text, param,
                    config_flags::negate);
            else
                throw gg_error("Missing regex following -vT.");
        }
        else if (strcmp("-vTw", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::text, param,
                    config_flags::whole_word | config_flags::negate);
            else
                throw gg_error("Missing regex following -vT.");
        }
        else if (strcmp("-VT", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::text, param,
                    config_flags::negate | config_flags::negate_all);
            else
                throw gg_error("Missing regex following -VT.");
        }
        else if (strcmp("-VTw", param) == 0)
        {
            ++i;
            param = argv[i];

            if (i < argc)
                configs.emplace_back(match_type::text, param,
                    config_flags::whole_word |
                    config_flags::negate | config_flags::negate_all);
            else
                throw gg_error("Missing regex following -VT.");
        }
        else if (strcmp("-writable", param) == 0)
            g_writable = true;
        else if (strcmp("-utf8", param) == 0)
            g_force_unicode = true;
        else
            files.emplace_back(argv[i]);
    }
}

static void fill_pipeline(const std::vector<config>& configs)
{
    // Postponed to allow -i to be processed first.
    for (const auto& config : configs)
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

                if (g_icase)
                    rules.flags(*lexertl::regex_flags::icase |
                        *lexertl::regex_flags::dot_not_cr_lf);

                rules.push(config._param, 1);
                rules.push(".{+}[\r\n]", rules_type::skip());
                generator::build(rules, lexer._sm);
                g_pipeline.emplace_back(std::move(lexer));
            }
            else
            {
                lexertl::rules rules;
                lexer lexer;

                lexer._flags = config._flags;

                if (g_icase)
                    rules.flags(*lexertl::regex_flags::icase |
                        *lexertl::regex_flags::dot_not_cr_lf);

                rules.push(config._param, 1);
                rules.push(".{+}[\r\n]", lexertl::rules::skip());
                lexertl::generator::build(rules, lexer._sm);
                g_pipeline.emplace_back(std::move(lexer));
            }

            break;
        }
        case match_type::regex:
        {
            regex regex;

            regex._flags = config._flags;
            regex._rx.assign(config._param, g_icase ?
                (std::regex_constants::ECMAScript |
                    std::regex_constants::icase) :
                std::regex_constants::ECMAScript);
            g_pipeline.emplace_back(std::move(regex));
            break;
        }
        case match_type::parser:
        {
            if (g_force_unicode)
            {
                uparser parser;
                config_state state;

                parser._flags = config._flags;
                g_curr_uparser = &parser;

                if (g_config_parser._gsm.empty())
                    build_config_parser();

                state.parse(config._param);

                if (parser._gsm.empty())
                {
                    ulexer lexer;

                    lexer._flags |= parser._flags & config_flags::negate;
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
                g_curr_parser = &parser;

                if (g_config_parser._gsm.empty())
                    build_config_parser();

                state.parse(config._param);

                if (parser._gsm.empty())
                {
                    lexer lexer;

                    lexer._flags |= parser._flags & config_flags::negate;
                    lexer._sm.swap(parser._lsm);
                    g_pipeline.emplace_back(std::move(lexer));
                }
                else
                    g_pipeline.emplace_back(std::move(parser));
            }

            break;
        }
        case match_type::text:
        {
            text text;

            text._flags = config._flags;
            text._text = config._param;
            g_pipeline.emplace_back(std::move(text));
            break;
        }
        default:
            break;
        }
    }
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

        read_switches(argc, argv, configs, files);
        fill_pipeline(configs);

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

        if (g_dump)
        {
            for (const auto& v : g_pipeline)
            {
                if (static_cast<match_type>(v.index()) == match_type::lexer &&
                    !g_force_unicode)
                {
                    const auto& l = std::get<lexer>(v);

                    lexertl::debug::dump(l._sm, std::cout);
                }
            }
        }

        if (g_pipeline.empty())
            throw gg_error("No actions have been specified.");

        if (g_pathname_only && g_show_hits)
            throw gg_error("Cannot combine -l and -hits.");

        if (g_output && g_pathnames.empty())
            throw gg_error("Cannot combine stdin with -o.");

        if (!g_replace.empty() && g_modify)
            throw gg_error("Cannot combine -replace with grammar "
                "actions that modify the input.");

        if (!g_startup.empty())
        {
            if (::system(g_startup.c_str()))
            {
                std::cerr << "Failed to execute " << g_startup << ".\n";
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
                std::cerr << "Failed to execute " << g_shutdown << ".\n";

        if (!g_pathname_only)
            std::cout << "Matches: " << g_hits << "    Matching files: " <<
            g_files << "    Total files searched: " << g_searched << std::endl;

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
