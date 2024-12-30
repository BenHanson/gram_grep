#pragma once

#include <charconv>
#include <filesystem>
#include "output.hpp"

extern unsigned int g_flags;
extern options g_options;

extern void parse_condition(const char* str);
extern std::string unescape(const std::string_view& vw);

struct option
{
    enum class type
    {
        regexp,
        misc,
        output,
        context,
        gram_grep
    };

    type _type;
    const char _short;
    const char* _long;
    const char* _param;
    const char* _help;
    void (*_func)(int& i, const bool longp,
        const char* const argv[], std::string_view param,
        std::vector<config>& configs);
};

void check_pattern_set()
{
    if (g_options._pattern_type != pattern_type::none)
        throw gg_error(std::format("{}conflicting matchers specified",
            gg_text()));
}

void colour(int&, const bool, const char* const [], std::string_view value,
    std::vector<config>&)
{
    if (!value.empty() && value != "always" && value != "auto" && value != "never")
    {
        show_help();
        exit(2);
    }

    if (value == "never")
        g_options._colour = false;
    else
    {
#ifdef _WIN32
        HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;

        GetConsoleMode(hOutput, &dwMode);

        if (value.empty() || value == "auto")
            g_options._colour = (dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        else
        {
            SetConsoleMode(hOutput, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            g_options._colour = true;
        }

        // No need to close hOutput
#else
        g_options._colour = true;
#endif
    }
}

void validate_value(int& i, const char* const argv[],
    const bool longp, std::string_view& value)
{
    if (!longp)
    {
        ++i;
        value = argv[i];
    }
}

const option g_option[]
{
    {
        option::type::regexp,
        'E',
        "extended-regexp",
        nullptr,
        "PATTERN is an extended regular expression (ERE)",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            check_pattern_set();
            g_options._pattern_type = pattern_type::extended;
        }
    },
    {
        option::type::regexp,
        'F',
        "fixed-strings",
        nullptr,
        "PATTERN is a string",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            check_pattern_set();
            g_options._pattern_type = pattern_type::fixed;
        }
    },
    {
        option::type::regexp,
        'G',
        "basic-regexp",
        nullptr,
        "PATTERN is a basic regular expression (BRE)",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            check_pattern_set();
            g_options._pattern_type = pattern_type::basic;
        }
    },
    {
        option::type::regexp,
        'P',
        "perl-regexp",
        nullptr,
        "PATTERN is a Perl regular expression",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            check_pattern_set();
            g_options._pattern_type = pattern_type::perl;
        }
    },
    {
        option::type::regexp,
        'i',
        "ignore-case",
        nullptr,
        "ignore case distinctions in patterns and data",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags |= *config_flags::icase;
        }
    },
    {
        option::type::regexp,
        '\0',
        "no-ignore-case",
        nullptr,
        "do not ignore case distinctions (default)",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags &= *config_flags::icase;
        }
    },
    {
        option::type::regexp,
        'w',
        "word-regexp",
        nullptr,
        "match only whole words",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags |= *config_flags::whole_word;
        }
    },
    {
        option::type::regexp,
        'x',
        "line-regexp",
        nullptr,
        "match only whole lines",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags |= *config_flags::bol_eol;
        }
    },
    {
        option::type::misc,
        's',
        "no-messages",
        nullptr,
        "suppress error messages",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._no_messages = true;
        }
    },
    {
        option::type::misc,
        'v',
        "invert-match",
        nullptr,
        "select non-matching text",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags |= *config_flags::negate;
        }
    },
    {
        option::type::misc,
        'V',
        "version",
        nullptr,
        "display version information and exit",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._show_version = true;
        }
    },
    {
        option::type::misc,
        '\0',
        "help",
        nullptr,
        "display this help text and exit",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            show_help();
            exit(0);
        }
    },
    {
        option::type::output,
        'm',
        "max-count",
        "NUM",
        "stop after NUM selected lines",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);

            std::stringstream ss;

            ss << value;
            ss >> g_options._max_count;
        }
    },
    {
        option::type::output,
        'b',
        "byte-offset",
        nullptr,
        "print the byte offset with output lines",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._byte_offset = true;
        }
    },
    {
        option::type::output,
        'n',
        "line-number",
        nullptr,
        "print line number with output lines",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._line_numbers = line_numbers::yes;
        }
    },
    {
        option::type::output,
        '\0',
        "line-buffered",
        nullptr,
        "flush output on every line",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._line_buffered = true;
        }
    },
    {
        option::type::output,
        'H',
        "with-filename",
        nullptr,
        "print file name with output lines",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._show_filename = show_filename::yes;
        }
    },
    {
        option::type::output,
        'h',
        "no-filename",
        nullptr,
        "suppress the file name prefix on output",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._show_filename = show_filename::no;
        }
    },
    {
        option::type::output,
        '\0',
        "label",
        "LABEL",
        "use LABEL as the standard input file name prefix",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            g_options._label = value;
        }
    },
    {
        option::type::output,
        'o',
        "only-matching",
        nullptr,
        "show only nonempty parts of lines that match",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._only_matching = true;
        }
    },
    {
        option::type::output,
        'q',
        "quiet,silent",
        nullptr,
        "suppress all normal output",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._quiet = true;
        }
    },
    {
        option::type::output,
        '\0',
        "binary-files",
        "TYPE",
        "assume that binary files are TYPE;\n"
        "TYPE is 'binary', 'text', or 'without-match'",
        [](int&, const bool, const char* const [], std::string_view value,
            std::vector<config>&)
        {
            if (value == "binary")
                g_options._binary_files = binary_files::binary;
            else if (value == "text")
                g_options._binary_files = binary_files::text;
            else if (value == "without-match")
                g_options._binary_files = binary_files::without_match;
            else
                throw gg_error(std::format("{}unknown binary-files type",
                    gg_text()));
        }
    },
    {
        option::type::output,
        'a',
        "text",
        nullptr,
        "equivalent to --binary-files=text",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._binary_files = binary_files::text;
        }
    },
    {
        option::type::output,
        'I',
        nullptr,
        nullptr,
        "equivalent to --binary-files=without-match",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._binary_files = binary_files::without_match;
        }
    },
    {
        option::type::output,
        'r',
        "recursive",
        nullptr,
        "recurse subdirectories",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._recursive = true;
        }
    },
    {
        option::type::output,
        '\0',
        "exclude",
        "GLOB",
        "skip files that match GLOB",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);

            const std::string str(value);
            auto names = split(str.c_str(), ';');

            for (const auto& name : names)
            {
                auto nm = name.data();

                if (*nm == '!')
                {
                    const std::string pathname(nm, nm + name.size());

                    g_options._exclude._negative.emplace_back(wildcardtl::wildcard
                        { pathname, is_windows() },
                        // If pathname does not include a wildcard
                        // store it as a plain string for error reporting.
                        strpbrk(nm, "*?[") ?
                        std::string() :
                        pathname);
                }
                else
                {
                    const std::string pathname(nm, nm + name.size());

                    g_options._exclude._positive.emplace_back(wildcardtl::wildcard
                        { pathname, is_windows() },
                        // If pathname does not include a wildcard
                        // store it as a plain string for error reporting.
                        strpbrk(nm, "*?[") ?
                        std::string() :
                        pathname);
                }
            }
        }
    },
    {
        option::type::output,
        '\0',
        "exclude-dir",
        "GLOB",
        "skip directories that match GLOB",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);

            const std::string str(value);
            auto pathnames = split(str.c_str(), ';');

            for (auto& p : pathnames)
            {
                namespace fs = std::filesystem;

                while (!p.empty() && p.ends_with(fs::path::preferred_separator))
                    p.remove_suffix(1);

                if (p[0] == '!')
                {
                    const std::string pathname(&p[1], std::to_address(p.end()));

                    g_options._exclude_dirs._negative.emplace_back(wildcardtl::wildcard
                        { pathname, is_windows() },
                        // If pathname does not include a wildcard
                        // store it as a plain string for error reporting.
                        pathname.find_first_of("*?[") == std::string::npos ?
                        pathname :
                        std::string());
                }
                else
                {
                    const std::string pathname(p);

                    g_options._exclude_dirs._positive.emplace_back(wildcardtl::wildcard
                        { pathname, is_windows() },
                        // If pathname does not include a wildcard
                        // store it as a plain string for error reporting.
                        pathname.find_first_of("*?[") == std::string::npos ?
                        pathname :
                        std::string());
                }
            }
        }
    },
    {
        option::type::output,
        'L',
        "files-without-match",
        nullptr,
        "print only names of FILEs with no selected lines",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._pathname_only = pathname_only::negated;
        }
    },
    {
        option::type::output,
        'l',
        "files-with-matches",
        nullptr,
        "print only names of FILEs with selected lines",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._pathname_only = pathname_only::yes;
        }
    },
    {
        option::type::output,
        'c',
        "count",
        nullptr,
        "print only a count of selected lines per FILE",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._show_count = true;
        }
    },
    {
        option::type::output,
        'T',
        "initial-tab",
        nullptr,
        "make tabs line up (if needed)",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._initial_tab = true;
        }
    },
    {
        option::type::output,
        'Z',
        "null",
        nullptr,
        "print 0 byte after FILE name",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._print_null = true;
        }
    },
    {
        option::type::context,
        'B',
        "before-context",
        "NUM",
        "print NUM lines of leading context",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            g_options._hit_separator = true;
            std::from_chars(value.data(), value.data() + value.size(),
                g_options._before_context);
        }
    },
    {
        option::type::context,
        'A',
        "after-context",
        "NUM",
        "print NUM lines of trailing context",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            g_options._hit_separator = true;
            std::from_chars(value.data(), value.data() + value.size(),
                g_options._after_context);
        }
    },
    {
        option::type::context,
        'C',
        "context",
        "NUM",
        "print NUM lines of output context",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            g_options._hit_separator = true;
            std::from_chars(value.data(), value.data() + value.size(),
                g_options._before_context);
            g_options._after_context = g_options._before_context;
        }
    },
    {
        option::type::context,
        '\0',
        "color",
        "[WHEN],",
        nullptr,
        colour
    },
    {
        option::type::context,
        '\0',
        "colour",
        "[WHEN]",
        "use markers to highlight the matching strings;\n"
        "WHEN is 'always', 'never', or 'auto'",
        colour
    },
    {
        option::type::gram_grep,
        '\0',
        "checkout",
        "CMD",
        "checkout command (include $1 for pathname)",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // checkout $1
            // *NOTE* $1 is replaced by the pathname
            g_options._checkout = value;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "config",
        "CONFIG_FILE",
        "search using config file",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>& configs)
        {
            validate_value(i, argv, longp, value);

            const std::string str(value);
            auto pathnames = split(str.c_str(), ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                configs.emplace_back(match_type::parser,
                    std::string(pn, pn + p.size()), g_flags,
                    g_options._conditions);
            }

            g_flags = 0;
            g_options._conditions.clear();
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "display-whole-match",
        nullptr,
        "display a multiline match",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._whole_match = true;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "dump",
        nullptr,
        "dump DFA regexp",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._dump = dump::text;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "dump-argv",
        nullptr,
        "dump command line arguments",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._dump_argv = true;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "dump-dot",
        nullptr,
        "dump DFA regexp in DOT format",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._dump = dump::dot;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "exec",
        "CMD",
        "Executes the supplied command",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            g_options._exec = value;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "extend-search",
        nullptr,
        "extend the end of the next match to be the current match",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags |= *config_flags::extend_search;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "flex-regexp",
        nullptr,
        "PATTERN is a flex style regexp",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            check_pattern_set();
            g_options._pattern_type = pattern_type::flex;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "force-write",
        nullptr,
        "if a file is read only, force it to be writable",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._force_write = true;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "if",
        "CONDITION",
        "make search conditional",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);

            const std::string str(value);

            parse_condition(str.c_str());
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "invert-match-all",
        nullptr,
        "only match if the search does not match at all",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags |= *config_flags::negate | *config_flags::all;
        }
    },
    {
        option::type::gram_grep,
        'N',
        "line-number-parens",
        nullptr,
        "print line number in parenthesis with output lines",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._line_numbers = line_numbers::with_parens;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "perform-output",
        nullptr,
        "output changes to matching file",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            g_options._perform_output = true;
        }
    },
    {
        option::type::gram_grep,
        'p',
        "print",
        "TEXT",
        "print TEXT instead of line of match",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            g_options._print = unescape(value);
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "replace",
        "TEXT",
        "replace match with TEXT",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            g_options._replace = unescape(value);
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "return-previous-match",
        nullptr,
        "return the previous match instead of the current one",
        [](int&, const bool, const char* const [],
            std::string_view, std::vector<config>&)
        {
            // Use the lexertl enum operator
            using namespace lexertl;

            g_flags |= *config_flags::ret_prev_match;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "shutdown",
        "CMD",
        "command to run when exiting",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // /delete /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep /noprompt
            g_options._shutdown = value;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "startup",
        "CMD",
        "command to run at startup",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);
            // "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
            // workspace /new /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep
            // /noprompt
            g_options._startup = value;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "summary",
        nullptr,
        "show match count footer",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._summary = true;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "utf8",
        nullptr,
        "in the absence of a BOM assume UTF-8",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._force_unicode = true;
        }
    },
    {
        option::type::gram_grep,
        'W',
        "word-list",
        "PATHNAME",
        "search for a word from the supplied word list",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>& configs)
        {
            validate_value(i, argv, longp, value);
            g_options._word_list_files =
                std::vector<lexertl::memory_file>
                (g_options._word_list_files.size() + 1);
            configs.emplace_back(match_type::word_list, value, g_flags,
                std::move(g_options._conditions));
            g_flags = 0;
        }
    },
    {
        option::type::gram_grep,
        '\0',
        "writable",
        nullptr,
        "only process files that are writable",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_options._writable = true;
        }
    }
};
