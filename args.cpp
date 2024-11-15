#include "pch.h"

#include "args.hpp"
#include <format>
#include "gg_error.hpp"
#include <parsertl/iterator.hpp>
#include "option.hpp"

extern void build_condition_parser();
extern std::string unescape(const std::string_view& vw);
extern std::string unescape_str(const char* first, const char* second);
extern void show_usage();

extern options g_options;
extern condition_parser g_condition_parser;
unsigned int g_flags = 0;

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

void parse_condition(const char* str)
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

            g_options._conditions[atoi(index.first + 1) & 0xffff] =
                std::regex(unescape_str(rx.first + 1, rx.second - 1));
        }
    }

    if (giter->entry.action == parsertl::action::error)
        throw gg_error(std::format("Failed to parse '{}'", str));
}

static void process_long(int& i, const char* const argv[],
    std::vector<config>& configs)
{
    // Skip over "--"
    const char* a = argv[i] + 2;
    const char* equal = strchr(a, '=');
    const std::string_view param(a, equal ? equal : a + strlen(a));
    const std::string_view value(equal ? equal + 1 : "");
    auto iter = std::ranges::find_if(g_option,
        [param](const auto& c)
        {
            std::vector<std::string_view> switches;

            if (c._long)
                switches = split(c._long, ',');

            return std::ranges::find(switches, param) != switches.end();
        });

    if (iter != std::end(g_option))
    {
        if (iter->_param && value.empty())
        {
            std::cerr << std::format("gram_grep: option `{}' "
                "requires an argument\n", argv[i]);
            show_usage();
            exit(2);
        }

        if (!value.empty() && !iter->_param)
        {
            throw gg_error(std::format("gram_grep: option `{}' "
                "doesn't accept an argument\n"
                "Try `gram_grep --help' for more information.",
                param));
        }

        iter->_func(i, true, argv, value, configs);
    }
    else
    {
        throw gg_error(std::format("Unknown switch {}", argv[i]));
    }
}

static void process_short(int& i, const int argc, const char* const argv[],
    std::vector<config>& configs)
{
    // Skip over '-'
    const char* param = argv[i] + 1;

    while (*param)
    {
        auto iter = std::find_if(std::begin(g_option), std::end(g_option),
            [param](const auto& c)
            {
                return *param == c._short;
            });

        if (iter != std::end(g_option))
        {
            if (iter->_param && i + 1 == argc)
            {
                std::cerr << std::format("gram_grep: option requires an "
                    "argument -- {}\n", argv[i][1]);
                show_usage();
                exit(2);
            }

            iter->_func(i, false, argv, std::string_view(), configs);
        }
        else
        {
            throw gg_error(std::format("Unknown switch -{}", *param));
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
                process_long(i, argv, configs);
            else
                process_short(i, argc, argv, configs);
        else
            files.emplace_back(argv[i]);
    }
}

void show_option(const option& opt)
{
    constexpr std::size_t max_len = 31;
    std::vector<std::string_view> long_options;
    std::ostringstream ss;

    if (opt._long)
        long_options = split(opt._long, ',');

    ss << "  ";

    if (opt._short)
    {
        ss << '-' << opt._short;
        
        if (opt._long)
            ss << ", ";
    }
    else
        ss << "    ";

    for (std::size_t idx = 0, size = long_options.size(); idx < size; ++idx)
    {
        if (idx)
            ss << ", ";

        ss << "--" << long_options[idx];
    }

    if (opt._param)
        ss << '=' << opt._param;

    if (const auto sz = ss.view().size();
        sz < max_len)
    {
        ss << std::string(max_len - sz, ' ');
    }
    else
        ss << "  ";

    std::cout << ss.view();
    
    if (opt._help)
    {
        const std::vector<std::string_view> help = split(opt._help, '\n');

        for (std::size_t idx = 0, size = help.size(); idx < size; ++idx)
        {
            if (idx)
                std::cout << '\n' << std::string(max_len, ' ');

            std::cout << help[idx];
        }
    }

    std::cout << '\n';
}

void show_help()
{
    auto iter = std::begin(g_option);
    auto end = std::end(g_option);

    std::cout << "Regexp selection and interpretation:\n";

    for (; iter->_type == option::type::regexp; ++iter)
    {
        show_option(*iter);
    }

    std::cout << '\n' << "Miscellaneous:\n";

    for (; iter->_type == option::type::misc; ++iter)
    {
        show_option(*iter);
    }

    std::cout << '\n' << "Output control:\n";

    for (; iter->_type == option::type::output; ++iter)
    {
        show_option(*iter);
    }

    std::cout << '\n' << "Context control:\n";

    for (; iter->_type == option::type::context; ++iter)
    {
        show_option(*iter);
    }

    std::cout << '\n' << "gram_grep specific switches:\n";

    for (; iter != end; ++iter)
    {
        show_option(*iter);
    }

    std::cout << '\n';
    std::cout << "Config file format:\n"
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
