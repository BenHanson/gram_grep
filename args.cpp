#include "pch.h"

#include "args.hpp"
#include <format>
#include "gg_error.hpp"
#include <parsertl/iterator.hpp>

extern bool g_byte_count;
extern std::string g_checkout;
extern bool g_colour;
extern condition_map g_conditions;
extern condition_parser g_condition_parser;
extern bool g_dump;
extern bool g_dot;
extern bool g_dump_argv;
extern wildcards g_exclude;
extern wildcards g_exclude_dirs;
extern std::string g_exec;
extern unsigned int g_flags;
extern bool g_force_unicode;
extern bool g_force_write;
extern show_filename g_show_filename;
extern bool g_initial_tab;
extern std::string g_label;
extern bool g_line_buffered;
extern bool g_line_numbers;
extern bool g_line_numbers_parens;
extern std::size_t g_max_count;
extern bool g_null_data;
extern bool g_only_matching;
extern bool g_output;
extern bool g_pathname_only;
extern bool g_pathname_only_negated;
extern std::string g_print;
extern bool g_print_null;
extern bool g_recursive;
extern std::string g_replace;
extern bool g_show_count;
extern std::string g_shutdown;
extern std::string g_startup;
extern bool g_summary;
extern bool g_show_version;
extern bool g_whole_match;
extern std::vector<lexertl::memory_file> g_word_list_files;
extern bool g_writable;

extern void build_condition_parser();
extern std::string unescape(const std::string_view& vw);
extern std::string unescape_str(const char* first, const char* second);

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

static void parse_condition(const char* str)
{
    if (g_condition_parser._gsm.empty())
        build_condition_parser();

    lexertl::citerator liter(str, str + strlen(str), g_condition_parser._lsm);
    parsertl::citerator giter(liter, g_condition_parser._gsm);

    for (; giter->entry.action != parsertl::action::accept &&
        giter->entry.action != parsertl::action::error; ++giter)
    {
        if (giter->entry.param == 2)
        {
            // start: 'regex_search' '(' Index ',' String ')'
            const auto index = giter.dollar(2);
            const auto rx = giter.dollar(4);

            g_conditions[atoi(index.first + 1) & 0xffff] =
                std::regex(unescape_str(rx.first + 1, rx.second - 1));
        }
    }

    if (giter->entry.action == parsertl::action::error)
        throw gg_error(std::format("Failed to parse '{}'", str));
}

static void process_long(int& i, const int argc, const char* const argv[],
    std::vector<config>& configs)
{
    // Skip over "--"
    const char* a = argv[i] + 2;
    const char* equal = strchr(a, '=');
    std::string_view param(a, equal ? equal : a + strlen(a));

    if (param == "basic-regexp")
    {
        if (i + 1 == argc)
            throw gg_error(std::format("Missing regex following -{}.",
                param));

        // Perl style regex
        ++i;
        configs.emplace_back(match_type::regex, argv[i],
            g_flags | config_flags::grep,
            std::move(g_conditions));
        g_flags = 0;
    }
    else if (param == "byte-offset")
        g_byte_count = true;
    else if (param == "color" || param == "colour")
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
    else if (param == "checkout")
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
    else if (param == "config")
    {
        ++i;

        if (i < argc)
        {
            auto pathnames = split(argv[i], ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                configs.emplace_back(match_type::parser,
                    std::string(pn, pn + p.size()), g_flags, g_conditions);
            }

            g_flags = 0;
            g_conditions.clear();
        }
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (param == "count")
        g_show_count = true;
    else if (param == "display-whole-match")
        g_whole_match = true;
    else if (param == "dot")
    {
        g_dump = true;
        g_dot = true;
    }
    else if (param == "dump")
        g_dump = true;
    else if (param == "dump-argv")
        g_dump_argv = true;
    else if (param == "exclude")
    {
        if (!equal)
            throw gg_error(std::format("Missing FILE_PATTERN following -{}.",
                param));

        auto pathnames = split(equal + 1, ';');

        for (const auto& p : pathnames)
        {
            auto pn = p.data();

            if (*pn == '!')
                g_exclude._negative.emplace_back(pn, pn + p.size(),
                    is_windows());
            else
                g_exclude._positive.emplace_back(pn, pn + p.size(),
                    is_windows());
        }
    }
    else if (param == "exclude-dir")
        if (!equal)
            throw gg_error(std::format("Missing PATTERN following -{}.",
                param));
        else
        {
            auto pathnames = split(equal + 1, ';');

            for (const auto& p : pathnames)
            {
                if (p[0] == '!')
                {
                    g_exclude_dirs._negative.emplace_back(std::string(&p[1],
                        std::to_address(p.end())), is_windows());
                }
                else
                {
                    g_exclude_dirs._positive.
                        emplace_back(std::string(p), is_windows());
                }
            }
        }
    else if (param == "exec")
    {
        if (i + 1 == argc)
            throw gg_error(std::format("Missing text following -{}.",
                param));
        ++i;
        g_exec = argv[i];
    }
    else if (param == "extended-regexp")
    {
        if (i + 1 == argc)
            throw gg_error(std::format("Missing regex following -{}.",
                param));

        // Perl style regex
        ++i;
        configs.emplace_back(match_type::regex, argv[i],
            g_flags | config_flags::egrep,
            std::move(g_conditions));
        g_flags = 0;
    }
    else if (param == "extend-search")
        g_flags |= config_flags::extend_search;
    else if (param == "files-with-matches")
        g_pathname_only = true;
    else if (param == "files-without-match")
        g_pathname_only_negated = true;
    else if (param == "fixed-strings")
    {
        // Text
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::text, argv[i],
                g_flags, std::move(g_conditions));
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (param == "flex-regexp")
    {
        // DFA regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::dfa_regex, argv[i], g_flags,
                std::move(g_conditions));
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (param == "force-write")
        g_force_write = true;
    else if (param == "help")
    {
        show_help();
        exit(0);
    }
    else if (param == "if")
    {
        // Condition
        ++i;

        if (i < argc)
            parse_condition(argv[i]);
        else
            throw gg_error(std::format("Missing condition following {}.",
                argv[i - 1]));
    }
    else if (param == "ignore-case")
        g_flags |= config_flags::icase;
    else if (param == "initial-tab")
        g_initial_tab = true;
    else if (param == "invert-match")
        g_flags |= config_flags::negate;
    else if (param == "invert-match-all")
        g_flags |= config_flags::negate | config_flags::all;
    else if (param == "label")
        if (!equal)
            throw gg_error(std::format("Missing NUM following -{}.",
                param));
        else
            g_label = equal + 1;
    else if (param == "line-buffered")
        g_line_buffered = true;
    else if (param == "line-number")
        g_line_numbers = true;
    else if (param == "line-number-parens")
    {
        g_line_numbers = true;
        g_line_numbers_parens = true;
    }
    else if (param == "line-regexp")
        g_flags |= config_flags::bol_eol;
    else if (param == "max-count")
    {
        if (!equal)
            throw gg_error(std::format("Missing NUM following -{}.",
                param));

        std::stringstream ss;

        ss << equal + 1;
        ss >> g_max_count;
    }
    else if (param == "no-filename")
        g_show_filename = show_filename::no;
    else if (param == "null")
        g_print_null = true;
    else if (param == "null-data")
        g_null_data = true;
    else if (param == "only-matching")
        g_only_matching = true;
    else if (param == "perform-output")
        g_output = true;
    else if (param == "perl-regexp")
    {
        // Perl style regex
        ++i;

        if (i < argc)
        {
            configs.emplace_back(match_type::regex, argv[i], g_flags,
                std::move(g_conditions));
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing regex following {}.",
                argv[i - 1]));
    }
    else if (param == "print")
    {
        ++i;

        if (i < argc)
            g_print = unescape(argv[i]);
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (param == "recursive")
        g_recursive = true;
    else if (param == "replace")
    {
        ++i;

        if (i < argc)
            g_replace = unescape(argv[i]);
        else
            throw gg_error(std::format("Missing text following {}.",
                argv[i - 1]));
    }
    else if (param == "return-previous-match")
        g_flags |= config_flags::ret_prev_match;
    else if (param == "shutdown")
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
    else if (param == "startup")
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
    else if (param == "summary")
        g_summary = true;
    else if (param == "version")
        g_show_version = true;
    else if (param == "with-filename")
        g_show_filename = show_filename::yes;
    else if (param == "word-regexp")
        g_flags |= config_flags::whole_word;
    else if (param == "word-list")
    {
        ++i;

        if (i < argc)
        {
            g_word_list_files = std::vector<lexertl::memory_file>(g_word_list_files.size() + 1);
            configs.emplace_back(match_type::word_list, argv[i], g_flags,
                std::move(g_conditions));
            g_flags = 0;
        }
        else
            throw gg_error(std::format("Missing pathname following {}.",
                argv[i - 1]));
    }
    else if (param == "writable")
        g_writable = true;
    else if (param == "utf8")
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
        case 'b':
            g_byte_count = true;
            break;
        case 'c':
            g_show_count = true;
            break;
        case 'E':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing regex following -{}.",
                    *param));

            // DFA regex
            ++i;
            configs.emplace_back(match_type::regex, argv[i],
                g_flags | config_flags::egrep,
                std::move(g_conditions));
            g_flags = 0;
            break;
        case 'F':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing text following -{}.",
                    *param));

            // Text
            ++i;
            configs.emplace_back(match_type::text, argv[i], g_flags,
                std::move(g_conditions));
            g_flags = 0;
            break;
        case 'G':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing regex following -{}.",
                    *param));

            // Perl style regex
            ++i;
            configs.emplace_back(match_type::regex, argv[i],
                g_flags | config_flags::grep,
                std::move(g_conditions));
            g_flags = 0;
            break;
        case 'H':
            g_show_filename = show_filename::yes;
            break;
        case 'h':
            g_show_filename = show_filename::no;
            break;
        case 'i':
            g_flags |= config_flags::icase;
            break;
        case 'L':
            g_pathname_only_negated = true;
            break;
        case 'l':
            g_pathname_only = true;
            break;
        case 'm':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing regex following -{}.",
                    *param));

            ++i;
            g_max_count = atoi(argv[i]);
            break;
        case 'N':
            g_line_numbers = true;
            g_line_numbers_parens = true;
            break;
        case 'n':
            g_line_numbers = true;
            break;
        case 'o':
            g_only_matching = true;
            break;
        case 'P':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing regex following -{}.",
                    *param));

            // Perl style regex
            ++i;
            configs.emplace_back(match_type::regex, argv[i], g_flags,
                std::move(g_conditions));
            g_flags = 0;
            break;
        case 'p':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing text following -{}.",
                    *param));

            ++i;
            g_print = unescape(argv[i]);
            break;
        case 'r':
            g_recursive = true;
            break;
        case 'S':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing pathname following -{}.",
                    *param));

            ++i;
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // /delete /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep /noprompt
            g_shutdown = argv[i];
            break;
        case 's':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing pathname following -{}.",
                    *param));

            ++i;
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // workspace /new /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep
            // /noprompt
            g_startup = argv[i];
            break;
        case 'T':
            g_initial_tab = true;
            break;
        case 'v':
            g_flags |= config_flags::negate;
            break;
        case 'V':
            g_show_version = true;
            break;
        case 'W':
            if (i + 1 == argc)
                throw gg_error(std::format("Missing pathname following -{}.",
                    *param));

            ++i;
            g_word_list_files = std::vector<lexertl::memory_file>(g_word_list_files.size() + 1);
            configs.emplace_back(match_type::word_list, argv[i], g_flags,
                std::move(g_conditions));
            g_flags = 0;
            break;
        case 'w':
            g_flags |= config_flags::whole_word;
            break;
        case 'x':
            g_flags |= config_flags::bol_eol;
            break;
        case 'Z':
            g_print_null = true;
            break;
        case 'z':
            g_null_data = true;
            break;
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
    std::cout << "Regexp selection and interpretation:\n"
        "  -E, --extended-regexp PATTERN\n"
        "    PATTERN is an extended regular expression (ERE)\n"
        "  -F, --fixed-strings PATTERN\n"
        "    PATTERN is a set of newline-separated fixed strings\n"
        "    with support for capture ($n) syntax\n"
        "  -G, --basic-regexp PATTERN\n"
        "    PATTERN is a basic regular expression (BRE)\n"
        "  -P, --perl-regexp PATTERN\n"
        "    PATTERN is a Perl regular expression\n"
        "      --flex-regexp PATTERN\tPATTERN is a flex style regexp\n"
        "  -i, --ignore-case\t\tignore case distinctions\n"
        "  -w, --word-regexp\t\tforce PATTERN to match only whole words\n"
        "  -x, --line-regexp\t\tforce PATTERN to match only whole lines\n"
        "  -z, --null-data\t\ta data line ends in 0 byte, not newline\n"
        "\n"
        "Miscellaneous:\n"
        "  -v, --invert-match\t\tselect non-matching text\n"
        "  -V, --version\t\t\tprint version information and exit\n"
        "      --help\t\t\tdisplay this help and exit\n"
        "\n"
        "Output control:\n"
        "  -m, --max-count=NUM\t\tstop after NUM matches\n"
        "  -b, --byte-offset\t\tprint the byte offset with output lines\n"
        "  -n, --line-number\t\tprint line number with output lines\n"
        "      --line-buffered\t\tflush output on every line\n"
        "  -H, --with-filename\t\tprint the filename for each match\n"
        "  -h, --no-filename\t\tsuppress the prefixing filename on output\n"
        "      --label=LABEL\t\tprint LABEL as filename for standard input\n"
        "  -o, --only-matching\t\tshow only the part of a line matching PATTERN\n"
        "  -r, --recursive\t\trecurse subdirectories\n"
        "      --exclude FILE_PATTERN"
        "    skip files and directories matching FILE_PATTERN\n"
        "      --exclude-dir=PATTERN  directories that match PATTERN will be skipped\n"
        "  -L, --files-without-match\tprint only names of FILEs containing no match\n"
        "  -l, --files-with-matches\tprint only names of FILEs containing matches\n"
        "  -c, --count\t\t\tprint only a count of matches per FILE\n"
        "  -T, --initial-tab\t\tmake tabs line up (if needed)\n"
        "  -Z, --null\t\t\tprint 0 byte after FILE name\n"
        "\n"
        "gram_grep specific switches:\n"
        "      --checkout <cmd>\t\tcheckout command (include $1 for pathname)\n"
        "      --colour, --color\t\tuse markers to highlight the matching strings\n"
        "      --config <config file>\tsearch using config file\n"
        "      --display-whole-match\tdisplay a multiline match\n"
        "      --dump\t\t\tdump DFA regexp\n"
        "      --dump-argv\t\tdump command line arguments\n"
        "      --dump-dot\t\tdump DFA regexp in DOT format\n"
        "      --exec <text>\t\tExecutes the supplied text\n"
        "      --extend-search\t\textend the end of the next match to be the end of the current match\n"
        "      --force-write\t\tif a file is read only, force it to be writable\n"
        "      --if <condition>\t\tmake search conditional\n"
        "      --invert-match-all\tonly match if the search does not match at all\n"
        "  -N, --line-number-parens\tprint line number in parenthesis with output lines\n"
        "      --perform-output\t\toutput changes to matching file\n"
        "  -p, --print <text>\t\tprint text instead of line of match\n"
        "      --replace <text>\t\treplace matching text\n"
        "      --return-previous-match\treturn the previous match instead of the current one\n"
        "  -S, --shutdown <cmd>\t\tcommand to run when exiting\n"
        "  -s, --startup <cmd>\t\tcommand to run at startup\n"
        "      --summary\t\t\tshow match count footer\n"
        "      --utf8\t\t\tin the absence of a BOM assume UTF-8\n"
        "  -W, --word-list <pathname>\tsearch for a word from the supplied word list\n"
        "      --writable\t\tonly process files that are writable\n"
        "  <pathname>...\t\t\tfiles to search (wildcards supported)\n\n"
        "Config file format:\n"
        "  <grammar directives>\n"
        "  %%\n"
        "  <grammar>\n"
        "  %%\n"
        "  <regexp macros>\n"
        "  %%\n"
        "  <regexes>\n"
        "  %%\n\n"
        "Grammar Directives:\n"
        "  %captures (parenthesis in grammars will be treated as captures)\n"
        "  %consume (list tokens that are not to be reported as unused)\n"
        "  %option caseless\n"
        "  %token\n"
        "  %left\n"
        "  %right\n"
        "  %nonassoc\n"
        "  %precedence\n"
        "  %start\n"
        "  %x\n\n"
        "Grammar scripting:\n"
        "  - Note that all strings can contain $n indexes\n\n"
        "Functions returning strings:\n"
        "  format('text', ...); (use {} for format specifiers)\n"
        "  replace_all('text', 'regexp', 'text');\n"
        "  system('text');\n"
        "\n"
        "General functions:\n"
        "  erase($n);\n"
        "  erase($from, $to);\n"
        "  erase($from.second, $to.first);\n"
        "  insert($n, 'text');\n"
        "  insert($n.second, 'text');\n"
        "  match = $n;\n"
        "  match += $n;\n"
        "  match = substr($n, <omit from left>, <omit from right>);\n"
        "  match += substr($n, <omit from left>, <omit from right>);\n"
        "  print('text');\n"
        "  replace($n, 'text');\n"
        "  replace($from, $to, 'text');\n"
        "  replace($from.second, $to.first, 'text');\n"
        "  replace_all($n, 'regexp', 'text');\n"
        "\n"
        "--if Syntax:\n"
        "  regex_search($n, 'regex'){ || regex_search($n, 'regex')}\n"
        "\n"
        "Example:\n"
        "  %token RawString String\n"
        "  %%\n"
        "  list: String { match = substr($1, 1, 1); };\n"
        "  list: RawString { match = substr($1, 3, 2); };\n"
        "  list: list String { match += substr($2, 1, 1); };\n"
        "  list: list RawString { match += substr($2, 3, 2); };\n"
        "  %%\n"
        R"(  ws [ \t\r\n]+)"
        "\n"
        "  %%\n"
        R"(  \"([^"\\\r\n]|\\.)*\"        String)"
        "\n"
        R"(  R\"\((?s:.)*?\)\"            RawString)"
        "\n"
        R"(  '([^'\\\r\n]|\\.)*'          skip())"
        "\n"
        R"(  {ws}|"//".*|"/*"(?s:.)*?"*/" skip())"
        "\n"
        "  %%\n\n"
        "  Note that you can pipeline searches by using multiple switches.\n"
        "  The searches are run in the order they occur on the command line.\n";
}
