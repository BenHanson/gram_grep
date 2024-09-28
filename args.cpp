#include "pch.h"

#include "args.hpp"
#include <format>
#include "gg_error.hpp"
#include <wildcardtl/wildcard.hpp>

extern std::string g_checkout;
extern bool g_colour;
extern bool g_dump;
extern bool g_dot;
extern bool g_dump_argv;
extern std::pair<std::vector<wildcardtl::wildcard>,
    std::vector<wildcardtl::wildcard>> g_exclude;
extern unsigned int g_flags;
extern bool g_force_unicode;
extern bool g_force_write;
extern bool g_output;
extern bool g_pathname_only;
extern std::string g_print;
extern bool g_recursive;
extern std::string g_replace;
extern bool g_show_hits;
extern std::string g_shutdown;
extern std::string g_startup;
extern bool g_whole_match;
extern bool g_writable;

extern std::string unescape(const std::string_view& vw);

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

bool is_windows()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

static void process_long(int& i, const int argc, const char* const argv[],
    std::vector<config>& configs)
{
    // Skip over "--"
    const char* param = argv[i] + 2;

    if (strcmp("color", param) == 0 || strcmp("colour", param) == 0)
    {
        g_colour = true;

#ifdef _WIN32
        HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;

        GetConsoleMode(hOutput, &dwMode);

        if (!(dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            SetConsoleMode(hOutput, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        // No need to close hOutput
#endif
    }
    else if (strcmp("checkout", param) == 0)
    {
        ++i;

        if (i < argc)
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // checkout $1
            // *NOTE* $1 is replaced by the pathname
            g_checkout = argv[i];
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (strcmp("display-whole-match", param) == 0)
        g_whole_match = true;
    else if (strcmp("dot", param) == 0)
    {
        g_dump = true;
        g_dot = true;
    }
    else if (strcmp("dump", param) == 0)
        g_dump = true;
    else if (strcmp("dump-argv", param) == 0)
        g_dump_argv = true;
    else if (strcmp("exclude", param) == 0)
    {
        ++i;

        if (i < argc)
        {
            auto pathnames = split(argv[i], ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                if (*pn == '!')
                    g_exclude.second.emplace_back(pn, pn + p.size(),
                        is_windows());
                else
                    g_exclude.first.emplace_back(pn, pn + p.size(),
                        is_windows());
            }
        }
        else
            throw gg_error(std::format("Missing wildcard following {}.",
                argv[i - 1]));
    }
    else if (strcmp("extended-regexp", param) == 0)
    {
        // DFA regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::dfa_regex, argv[i], g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (strcmp("extend-search", param) == 0)
        g_flags |= config_flags::extend_search;
    else if (strcmp("file", param) == 0)
    {
        ++i;

        if (i < argc)
        {
            auto pathnames = split(argv[i], ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                configs.emplace_back(match_type::parser,
                    std::string(pn, pn + p.size()), g_flags);
            }

            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (strcmp("files-with-matches", param) == 0)
        g_pathname_only = true;
    else if (strcmp("fixed-strings", param) == 0)
    {
        // Text
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::text, argv[i], g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (strcmp("force-write", param) == 0)
        g_force_write = true;
    else if (strcmp("help", param) == 0)
    {
        show_help();
        exit(0);
    }
    else if (strcmp("hits", param) == 0)
        g_show_hits = true;
    else if (strcmp("ignore-case", param) == 0)
        g_flags |= config_flags::icase;
    else if (strcmp("invert-extended-regexp", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search "
                "with {}", argv[i]));

        // DFA regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::dfa_regex, argv[i],
                config_flags::negate | g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (strcmp("invert-file", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search with {}",
                argv[i]));

        ++i;

        if (i < argc)
        {
            auto pathnames = split(argv[i], ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                configs.emplace_back(match_type::parser,
                    std::string(pn, pn + p.size()),
                    config_flags::negate | g_flags);
            }

            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (strcmp("invert-perl-regexp", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search "
                "with {}", argv[i]));

        // Perl style regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::regex, argv[i],
                config_flags::negate | g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (strcmp("invert-text", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search "
                "with {}", argv[i]));

        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::text, argv[i],
                config_flags::negate | g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (strcmp("invert-all-extended-regexp", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search "
                "with {}", argv[i]));

        // DFA regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::dfa_regex, argv[i],
                config_flags::negate | config_flags::all | g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (strcmp("invert-all-file", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search "
                "with {}", argv[i]));

        ++i;

        if (i < argc)
        {
            auto pathnames = split(argv[i], ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                configs.emplace_back(match_type::parser,
                    std::string(pn, pn + p.size()),
                    config_flags::negate | config_flags::all | g_flags);
            }

            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (strcmp("invert-all-perl-regexp", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search "
                "with {}", argv[i]));

        // Perl style regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::regex, argv[i],
                config_flags::negate | config_flags::all | g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (strcmp("invert-all-text", param) == 0)
    {
        if (g_flags & config_flags::extend_search)
            throw gg_error(std::format("Cannot mix --extend-search with {}",
                argv[i]));

        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::text, argv[i],
                config_flags::negate | config_flags::all | g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (strcmp("perform-output", param) == 0)
        g_output = true;
    else if (strcmp("perl-regexp", param) == 0)
    {
        // Perl style regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::regex, argv[i], g_flags);
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (strcmp("print", param) == 0)
    {
        ++i;

        if (i < argc)
            g_print = unescape(argv[i]);
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (strcmp("recursive", param) == 0)
        g_recursive = true;
    else if (strcmp("replace", param) == 0)
    {
        ++i;

        if (i < argc)
            g_replace = unescape(argv[i]);
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (strcmp("return-previous-match", param) == 0)
        g_flags |= config_flags::ret_prev_match;
    else if (strcmp("shutdown", param) == 0)
    {
        ++i;

        if (i < argc)
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // /delete /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep /noprompt
            g_shutdown = argv[i];
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (strcmp("startup", param) == 0)
    {
        ++i;

        if (i < argc)
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // workspace /new /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep
            // /noprompt
            g_startup = argv[i];
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (strcmp("whole-word", param) == 0)
        g_flags |= config_flags::whole_word;
    else if (strcmp("writable", param) == 0)
        g_writable = true;
    else if (strcmp("utf8", param) == 0)
        g_force_unicode = true;
    else
        throw gg_error(std::format("Unknown switch {}", argv[i]));
}

static void process_short(int& i, const int argc, const char* const argv[],
    std::vector<config>& configs)
{
    // Skip over '-'
    const char* param = argv[i] + 1;

    while (*param)
    {
        switch (*param)
        {
        case 'a':
            ++i;

            if (i < argc)
                g_replace = unescape(argv[i]);
            else
                throw gg_error(std::format("Missing text following {}.",
                    argv[i - 1]));

            break;
        case 'c':
            ++i;

            if (i < argc)
                // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
                // checkout $1
                // *NOTE* $1 is replaced by the pathname
                g_checkout = argv[i];
            else
                throw gg_error(std::format("Missing pathname following {}.",
                    argv[i - 1]));

            break;
        case 'd':
            g_dump = true;
            break;
        case 'D':
            g_dump = true;
            g_dot = true;
            break;
        case 'E':
            // DFA regex
            ++i;

            if (i < argc)
            {
                configs.emplace_back(match_type::dfa_regex, argv[i], g_flags);
                g_flags = 0;
            }
            else
                throw gg_error(std::format("Missing regex following {}.",
                    argv[i - 1]));

            break;
        case 'e':
            g_force_write = true;
            break;
        case 'f':
            ++i;

            if (i < argc)
            {
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    auto pn = p.data();

                    configs.emplace_back(match_type::parser,
                        std::string(pn, pn + p.size()), g_flags);
                }

                g_flags = 0;
            }
            else
                throw gg_error(std::format("Missing pathname following {}.",
                    argv[i - 1]));

            break;
        case 'F':
            // Text
            ++i;

            if (i < argc)
            {
                configs.emplace_back(match_type::text, argv[i], g_flags);
                g_flags = 0;
            }
            else
                throw gg_error(std::format("Missing text following {}.",
                    argv[i - 1]));

            break;
        case 'h':
            show_help();
            exit(0);
            break;
        case 'i':
            g_flags |= config_flags::icase;
            break;
        case 'l':
            g_pathname_only = true;
            break;
        case 'o':
            g_output = true;
            break;
        case 'P':
            // Perl style regex
            ++i;

            if (i < argc)
            {
                configs.emplace_back(match_type::regex, argv[i], g_flags);
                g_flags = 0;
            }
            else
                throw gg_error(std::format("Missing regex following {}.",
                    argv[i - 1]));

            break;
        case 'p':
            ++i;

            if (i < argc)
                g_print = unescape(argv[i]);
            else
                throw gg_error(std::format("Missing text following {}.",
                    argv[i - 1]));

            break;
        case 'R':
        case 'r':
            g_recursive = true;
            break;
        case 'S':
            ++i;

            if (i < argc)
                // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
                // /delete /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep /noprompt
                g_shutdown = argv[i];
            else
                throw gg_error(std::format("Missing pathname following {}.",
                    argv[i - 1]));

            break;
        case 's':
            ++i;

            if (i < argc)
                // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
                // workspace /new /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep
                // /noprompt
                g_startup = argv[i];
            else
                throw gg_error(std::format("Missing pathname following {}.",
                    argv[i - 1]));

            break;
        case 't':
            g_show_hits = true;
            break;
        case 'u':
            g_force_unicode = true;
            break;
        case 'V':
            ++param;

            if (g_flags & config_flags::extend_search)
                throw gg_error(std::format("Cannot mix --extend-search with {}",
                    argv[i]));

            switch (*param)
            {
            case 'E':
                // DFA regex
                ++i;

                if (i < argc)
                {
                    configs.emplace_back(match_type::dfa_regex, argv[i],
                        config_flags::negate | config_flags::all | g_flags);
                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing regex following {}.",
                        argv[i - 1]));

                break;
            case 'f':
                ++i;

                if (i < argc)
                {
                    auto pathnames = split(argv[i], ';');

                    for (const auto& p : pathnames)
                    {
                        auto pn = p.data();

                        configs.emplace_back(match_type::parser,
                            std::string(pn, pn + p.size()),
                            config_flags::negate | config_flags::all | g_flags);
                    }

                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing pathname following {}.",
                        argv[i - 1]));

                break;
            case 'F':
                ++i;

                if (i < argc)
                {
                    configs.emplace_back(match_type::text, argv[i],
                        config_flags::negate | config_flags::all | g_flags);
                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing text following {}.",
                        argv[i - 1]));

                break;
            case 'P':
                // Perl style regex
                ++i;

                if (i < argc)
                {
                    configs.emplace_back(match_type::regex, argv[i],
                        config_flags::negate | config_flags::all | g_flags);
                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing regex following {}.",
                        argv[i - 1]));

                break;
            default:
                throw gg_error(std::format("Unknown switch {}", argv[i]));
                break;
            }

            break;
        case 'v':
            ++param;

            if (g_flags & config_flags::extend_search)
                throw gg_error(std::format("Cannot mix --extend-search with {}",
                    argv[i]));

            switch (*param)
            {
            case 'E':
                // DFA regex
                ++i;

                if (i < argc)
                {
                    configs.emplace_back(match_type::dfa_regex, argv[i],
                        config_flags::negate | g_flags);
                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing regex following {}.",
                        argv[i - 1]));

                break;
            case 'f':
                ++i;

                if (i < argc)
                {
                    auto pathnames = split(argv[i], ';');

                    for (const auto& p : pathnames)
                    {
                        auto pn = p.data();

                        configs.emplace_back(match_type::parser,
                            std::string(pn, pn + p.size()),
                            config_flags::negate | g_flags);
                    }

                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing pathname following {}.",
                        argv[i - 1]));

                break;
            case 'F':
                ++i;

                if (i < argc)
                {
                    configs.emplace_back(match_type::text, argv[i],
                        config_flags::negate | g_flags);
                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing text following {}.",
                        argv[i - 1]));

                break;
            case 'P':
                // Perl style regex
                ++i;

                if (i < argc)
                {
                    configs.emplace_back(match_type::regex, argv[i],
                        config_flags::negate | g_flags);
                    g_flags = 0;
                }
                else
                    throw gg_error(std::format("Missing regex following {}.",
                        argv[i - 1]));

                break;
            default:
                throw gg_error(std::format("Unknown switch {}",
                    argv[i]));
                break;
            }

            break;
        case 'w':
            g_flags |= config_flags::whole_word;
            break;
        case 'W':
            g_writable = true;
            break;
        case 'x':
        {
            ++i;

            if (i < argc)
            {
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    auto pn = p.data();

                    if (*pn == '!')
                        g_exclude.second.emplace_back(pn, pn + p.size(),
                            is_windows());
                    else
                        g_exclude.first.emplace_back(pn, pn + p.size(),
                            is_windows());
                }
            }
            else
                throw gg_error(std::format("Missing wildcard following {}.",
                    argv[i - 1]));

            break;
        }
        default:
            throw gg_error(std::format("Unknown switch {}", argv[i]));
            break;
        }

        ++param;
    }
}

void read_switches(const int argc, const char* const argv[],
    std::vector<config>& configs, std::vector<std::string>& files)
{
    for (int i = 1; i < argc; ++i)
    {
        const char* param = argv[i];

        if (*param == '-')
            if (param[1] == '-')
                process_long(i, argc, argv, configs);
            else
                process_short(i, argc, argv, configs);
        else
            files.emplace_back(argv[i]);
    }
}

void show_help()
{
    std::cout << "-h, --help\t\t\t\t\tShows help.\n"
        "-c, --checkout <cmd>\t\t\t\tCheckout command (include $1 for pathname).\n"
        "    --colour, --color\t\t\t\tUse markers to highlight the matching strings.\n"
        "    --display-whole-match\t\t\tDisplay a multiline match.\n"
        "-D, --dot\t\t\t\t\tDump DFA regexp in DOT format.\n"
        "-d, --dump\t\t\t\t\tDump DFA regexp.\n"
        "    --dump-argv\t\t\t\t\tDump command line arguments.\n"
        "-x, --exclude <wildcard>\t\t\tExclude pathnames matching wildcard.\n"
        "    --extend-search\t\t\t\tExtend the end of the next match to be the end of the current match.\n"
        "-E, --extended-regexp <regexp>\t\t\tSearch using DFA regexp.\n"
        "-f, --file <config file>\t\t\tSearch using config file.\n"
        "-l, --files-with-matches\t\t\tOutput pathname only.\n"
        "-F, --fixed-strings <text>\t\t\tSearch for plain text with support for capture ($n) "
        "syntax.\n"
        "-e, --force-write\t\t\t\tIf a file is read only, force it to be writable.\n"
        "-t, --hits\t\t\t\t\tShow hit count per file.\n"
        "-i, --ignore-case\t\t\t\tCase insensitive searching.\n"
        "-VE, --invert-all-extended-regexp <regexp>\tSearch using DFA regexp (all negated).\n"
        "-Vf, --invert-all-file <config file>\t\tSearch using config file (all negated).\n"
        "-VP, --invert-all-perl-regexp <regexp>\t\tSearch using std::regex (all negated).\n"
        "-VF, --invert-all-text <text>\t\t\tSearch for plain text with support for capture "
        "($n) syntax (all negated).\n"
        "-vE, --invert-extended-regexp <regexp>\t\tSearch using DFA regexp (negated).\n"
        "-vf, --invert-file <config file>\t\tSearch using config file (negated).\n"
        "-vP, --invert-perl-regexp <regexp>\t\tSearch using std::regex (negated).\n"
        "-vF, --invert-text <text>\t\t\tSearch for plain text with support for "
        "capture ($n) syntax (negated).\n"
        "-o, --perform-output\t\t\t\tOutput changes to matching file.\n"
        "-P, --perl-regexp <regexp>\t\t\tSearch using std::regex.\n"
        "-p, --print <text>\t\t\t\tPrint text instead of line of match.\n"
        "-r, -R, --recursive\t\t\t\tRecurse subdirectories.\n"
        "-a, --replace <text>\t\t\t\tReplace matching text.\n"
        "    --return-previous-match\t\t\tReturn the previous match instead of the current one.\n"
        "-S, --shutdown <cmd>\t\t\t\tCommand to run when exiting.\n"
        "-s, --startup <cmd>\t\t\t\tCommand to run at startup.\n"
        "-u, --utf8\t\t\t\t\tIn the absence of a BOM assume UTF-8.\n"
        "-w, --whole-word\t\t\t\tForce match to match only whole words.\n"
        "-W, --writable\t\t\t\t\tOnly process files that are writable.\n"
        "<pathname>...\t\t\t\t\tFiles to search (wildcards supported).\n\n"
        "Config file format:\n\n"
        "<grammar directives>\n"
        "%%\n"
        "<grammar>\n"
        "%%\n"
        "<regexp macros>\n"
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
        "Grammar scripting:\n"
        "- Note that all strings can contain $n indexes\n\n"
        "Functions returning strings:\n\n"
        "exec('text');\n"
        "format('text', ...);\n"
        "replace_all('text', 'regexp', 'text');\n\n"
        "General functions:\n\n"
        "erase($n);\n"
        "erase($from, $to);\n"
        "erase($from.second, $to.first);\n"
        "insert($n, 'text');\n"
        "insert($n.second, 'text');\n"
        "match = $n;\n"
        "match += $n;\n"
        "match = substr($n, <omit from left>, <omit from right>);\n"
        "match += substr($n, <omit from left>, <omit from right>);\n"
        "print('text');\n"
        "replace($n, 'text');\n"
        "replace($from, $to, 'text');\n"
        "replace($from.second, $to.first, 'text');\n"
        "replace_all($n, 'regexp', 'text');\n\n"
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
