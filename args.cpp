#include "pch.h"

#include "args.hpp"
#include <format>
#include "gg_error.hpp"
#include "parser.hpp"
#include <wildcardtl/wildcard.hpp>

extern std::string g_checkout;
extern bool g_dump;
extern std::pair<std::vector<wildcardtl::wildcard>,
    std::vector<wildcardtl::wildcard>> g_exclude;
extern bool g_force_unicode;
extern bool g_force_write;
extern bool g_icase;
extern bool g_output;
extern bool g_pathname_only;
extern std::string g_print;
extern bool g_recursive;
extern std::string g_replace;
extern bool g_show_hits;
extern std::string g_shutdown;
extern std::string g_startup;
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
#ifdef WIN32
    return true;
#else
    return false;
#endif
}

static void process_long(int &i, const int argc, const char* const argv[],
    std::vector<config>& configs)
{
    // Skip over "--"
    const char* param = argv[i] + 2;

    if (strcmp("--checkout", param) == 0)
    {
        ++i;

        if (i < argc)
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // checkout $1
            // *NOTE* $1 is replaced by the pathname
            g_checkout = argv[i];
        else
            throw gg_error(std::format("Missing pathname "
                "following {}.", param));
    }
    else if (strcmp("--dump", param) == 0)
        g_dump = true;
    else if (strcmp("--exclude", param) == 0)
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
            throw gg_error(std::format("Missing wildcard "
                "following {}.", param));
    }
    else if (strcmp("--extended-regexp", param) == 0)
    {
        // DFA regex
        ++i;

        if (i < argc)
            configs.emplace_back(match_type::dfa_regex, argv[i],
                config_flags::none);
        else
            throw gg_error(std::format("Missing regex following {}.", param));
    }
    else if (strcmp("--file", param) == 0)
    {
        ++i;

        if (i < argc)
        {
            auto pathnames = split(argv[i], ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                configs.emplace_back(match_type::parser,
                    std::string(pn, pn + p.size()),
                    config_flags::none);
            }
        }
        else
            throw gg_error(std::format("Missing pathname following {}.",
                param));
    }
    else if (strcmp("--files-with-matches", param) == 0)
        g_pathname_only = true;
    else if (strcmp("--force-write", param) == 0)
        g_force_write = true;
    else if (strcmp("--help", param) == 0)
    {
        show_help();
        exit(0);
    }
    else if (strcmp("--hits", param) == 0)
        g_show_hits = true;
    else if (strcmp("--ignore-case", param) == 0)
        g_icase = true;
    else if (strcmp("--perform-output", param) == 0)
        g_output = true;
    else if (strcmp("--perl-regexp", param) == 0)
    {
        // Perl style regex
        ++i;

        if (i < argc)
            configs.emplace_back(match_type::regex, argv[i],
                config_flags::none);
        else
            throw gg_error(std::format("Missing regex following {}.",
                param));
    }
    else if (strcmp("--print", param) == 0)
    {
        ++i;

        if (i < argc)
            g_print = unescape(argv[i]);
        else
            throw gg_error(std::format("Missing string following {}.",
                param));
    }
    else if (strcmp("--recursive", param) == 0)
        g_recursive = true;
    else if (strcmp("--replace", param) == 0)
    {
        ++i;

        if (i < argc)
            g_replace = unescape(argv[i]);
        else
            throw gg_error(std::format("Missing text "
                "following {}.", param));
    }
    else if (strcmp("--shutdown", param) == 0)
    {
        ++i;

        if (i < argc)
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // /delete /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep /noprompt
            g_shutdown = argv[i];
        else
            throw gg_error(std::format("Missing pathname "
                "following {}.", param));
    }
    else if (strcmp("--startup", param) == 0)
    {
        ++i;

        if (i < argc)
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // workspace /new /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep
            // /noprompt
            g_startup = argv[i];
        else
            throw gg_error(std::format("Missing pathname "
                "following {}.", param));
    }
    else if (strcmp("--writable", param) == 0)
        g_writable = true;
    else if (strcmp("--utf8", param) == 0)
        g_force_unicode = true;
    else
        throw gg_error(std::format("Unknown switch {}", param));
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
        case 'd':
            g_dump = true;
            break;
        case 'E':
            // DFA regex
            ++i;

            if (i < argc)
            {
                const bool end = *(param + 1) == 'e';

                configs.emplace_back(match_type::dfa_regex, argv[i],
                    end ?
                    config_flags::extend_search :
                    config_flags::none);

                if (end)
                    ++param;
            }
            else
                throw gg_error("Missing regex following -E[e].");

            break;
        case 'e':
            g_force_write = true;
            break;
        case 'f':
            ++i;

            if (i < argc)
            {
                const bool end = *(param + 1) == 'e';
                auto pathnames = split(argv[i], ';');

                for (const auto& p : pathnames)
                {
                    auto pn = p.data();

                    configs.emplace_back(match_type::parser,
                        std::string(pn, pn + p.size()),
                        end ?
                        config_flags::extend_search :
                        config_flags::none);
                }

                if (end)
                    ++param;
            }
            else
                throw gg_error("Missing pathname following -f[e].");

            break;
        case 'h':
            show_help();
            exit(0);
            break;
        case 'i':
            g_icase = true;
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
                const bool end = *(param + 1) == 'e';

                configs.emplace_back(match_type::regex, argv[i],
                    end ?
                    config_flags::extend_search :
                    config_flags::none);

                if (end)
                    ++param;
            }
            else
                throw gg_error("Missing regex following -P[e].");

            break;
        case 'p':
            ++i;

            if (i < argc)
                g_print = unescape(argv[i]);
            else
                throw gg_error("Missing string following -p.");

            break;
        case 'R':
        case 'r':
            g_recursive = true;
            break;
        case 'T':
            // Text
            ++i;

            if (i < argc)
            {
                const bool end = *(param + 1) == 'e';
                const bool whole_word = *(param + 1) == 'w';

                configs.emplace_back(match_type::text, argv[i],
                    end ?
                    config_flags::extend_search :
                    whole_word ?
                    config_flags::whole_word :
                    config_flags::none);

                if (end || whole_word)
                    ++param;
            }
            else
                throw gg_error("Missing regex following -T[e|w].");

            break;
        case 't':
            g_show_hits = true;
            break;
        case 'u':
            g_force_unicode = true;
            break;
        case 'V':
            ++param;

            switch (*param)
            {
            case 'E':
                // DFA regex
                ++i;

                if (i < argc)
                    configs.emplace_back(match_type::dfa_regex, argv[i],
                        config_flags::negate | config_flags::negate_all);
                else
                    throw gg_error("Missing regex following -VE.");

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
                            config_flags::negate | config_flags::negate_all);
                    }
                }
                else
                    throw gg_error("Missing pathname following -Vf.");

                break;
            case 'P':
                // Perl style regex
                ++i;

                if (i < argc)
                    configs.emplace_back(match_type::regex, argv[i],
                        config_flags::negate | config_flags::negate_all);
                else
                    throw gg_error("Missing regex following -VP.");

                break;
            case 'T':
                ++i;

                if (i < argc)
                {
                    const bool whole_word = *(param + 1) == 'w';

                    configs.emplace_back(match_type::text, argv[i],
                        (whole_word ? config_flags::whole_word : 0) |
                        config_flags::negate | config_flags::negate_all);

                    if (whole_word)
                        ++param;
                }
                else
                    throw gg_error("Missing regex following -VT[w].");

                break;
            default:
                throw gg_error(std::format("Unknown switch -V{}", *param));
                break;
            }

            break;
        case 'v':
            ++param;

            switch (*param)
            {
            case 'E':
                // DFA regex
                ++i;

                if (i < argc)
                    configs.emplace_back(match_type::dfa_regex, argv[i],
                        config_flags::negate);
                else
                    throw gg_error("Missing regex following -vE.");

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
                            config_flags::negate);
                    }
                }
                else
                    throw gg_error("Missing pathname following -vf.");

                break;
            case 'P':
                // Perl style regex
                ++i;

                if (i < argc)
                    configs.emplace_back(match_type::regex, argv[i],
                        config_flags::negate);
                else
                    throw gg_error("Missing regex following -vP.");

                break;
            case 'T':
                ++i;

                if (i < argc)
                {
                    const bool whole_word = *(param + 1) == 'w';

                    configs.emplace_back(match_type::text, argv[i],
                        (whole_word ? config_flags::whole_word : 0) |
                        config_flags::negate);

                    if (whole_word)
                        ++param;
                }
                else
                    throw gg_error("Missing regex following -vT[w].");

                break;
            default:
                throw gg_error(std::format("Unknown switch -v{}", *param));
                break;
            }

            break;
        case 'w':
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
                throw gg_error("Missing wildcard "
                    "following -x.");

            break;
        }
        default:
            throw gg_error(std::format("Unknown switch -{}", *param));
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
    std::cout << "-h, --help\t\t\tShows help.\n"
        "--checkout <cmd>\t\tcheckout command (include $1 for pathname).\n"
        "-d, --dump\t\t\tDump DFA regex.\n"
        "-E, --extended-regexp <regex>\tSearch using DFA regex.\n"
        "-Ee <regex>\t\t\tAs -E but continue search following match.\n"
        "-e, --force-write\t\tIf a file is read only, force it to be writable.\n"
        "-f, --file <config file>\tSearch using config file.\n"
        "-fe <config file>\t\tAs -f but continue search following match.\n"
        "-i, --ignore-case\t\tCase insensitive searching.\n"
        "-l, --files-with-matches\tOutput pathname only.\n"
        "-o, --perform-output\t\tOutput changes to matching file.\n"
        "-P, --perl-regexp <regex>\tSearch using std::regex.\n"
        "-Pe <regex>\t\t\tAs -P but continue search following match.\n"
        "-p, --print <text>\t\tPrint text instead of line of match.\n"
        "-r, -R, --recursive\t\tRecurse subdirectories.\n"
        "--replace <text>\t\tReplace matching text.\n"
        "--shutdown <cmd>\t\tCommand to run when exiting.\n"
        "--startup <cmd>\t\t\tCommand to run at startup.\n"
        "-T <text>\t\t\tSearch for plain text with support for capture ($n) syntax.\n"
        "-Tw <text>\t\t\tAs -T but with whole word matching.\n"
        "-t, --hits\t\t\tShow hit count per file.\n"
        "-u, --utf8\t\t\tIn the absence of a BOM assume UTF-8\n"
        "-vE <regex>\t\t\tSearch using DFA regex (negated).\n"
        "-vf <config file>\t\tSearch using config file (negated).\n"
        "-vP <regex>\t\t\tSearch using std::regex (negated).\n"
        "-vT <text>\t\t\tSearch for plain text with support for capture ($n) syntax (negated).\n"
        "-vTw <text>\t\t\tAs -vT but with whole word matching.\n"
        "-VE <regex>\t\t\tSearch using DFA regex (all negated).\n"
        "-Vf <config file>\t\tSearch using config file (all negated).\n"
        "-VP <regex>\t\t\tSearch using std::regex (all negated).\n"
        "-VT <text>\t\t\tSearch for plain text with support for capture ($n) syntax (all negated).\n"
        "-VTw <text>\t\t\tAs -VT but with whole word matching.\n"
        "-w, --writable\t\t\tOnly process files that are writable.\n"
        "-x, --exclude <wildcard>\tExclude pathnames matching wildcard.\n"
        "<pathname>...\t\t\tFiles to search (wildcards supported).\n\n"
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
        "print('text');\n"
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
