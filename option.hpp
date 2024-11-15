#pragma once

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

void colour(int&, const bool, const char* const [], std::string_view,
    std::vector<config>&)
{
    g_options._colour = true;

#ifdef _WIN32
    HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;

    GetConsoleMode(hOutput, &dwMode);

    if (!(dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        SetConsoleMode(hOutput, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // No need to close hOutput
#endif
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

void regexp(int& i, const bool longp, const char* const argv[],
    const match_type type, std::string_view value, std::vector<config>& configs)
{
    validate_value(i, argv, longp, value);
    configs.emplace_back(type, value, g_flags, std::move(g_options._conditions));
    g_flags = 0;
}

const option g_option[]
{
    {
        option::type::regexp,
        'E',
        "extended-regexp",
        "PATTERN",
        "PATTERN is an extended regular expression (ERE)",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>& configs)
        {
            g_flags |= config_flags::egrep;
            regexp(i, longp, argv, match_type::regex, value, configs);
        }
    },
    {
        option::type::regexp,
        'F',
        "fixed-strings",
        "PATTERN",
        "PATTERN is a set of newline-separated fixed strings",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>& configs)
        {
            regexp(i, longp, argv, match_type::text, value, configs);
        }
    },
    {
        option::type::regexp,
        'G',
        "basic-regexp",
        "PATTERN",
        "PATTERN is a basic regular expression (BRE)",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>& configs)
        {
            g_flags |= config_flags::grep;
            regexp(i, longp, argv, match_type::regex, value, configs);
        }
    },
    {
        option::type::regexp,
        'P',
        "perl-regexp",
        "PATTERN",
        "PATTERN is a Perl regular expression",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>& configs)
        {
            regexp(i, longp, argv, match_type::regex, value, configs);
        }
    },
    {
        option::type::regexp,
        '\0',
        "flex-regexp",
        "PATTERN",
        "PATTERN is a flex style regexp",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>& configs)
        {
            regexp(i, longp, argv, match_type::dfa_regex, value, configs);
        }
    },
    {
        option::type::regexp,
        'i',
        "ignore-case",
        nullptr,
        "ignore case distinctions",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_flags |= config_flags::icase;
        }
    },
    {
        option::type::regexp,
        'w',
        "word-regexp",
        nullptr,
        "force PATTERN to match only whole words",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_flags |= config_flags::whole_word;
        }
    },
    {
        option::type::regexp,
        'x',
        "line-regexp",
        nullptr,
        "force PATTERN to match only whole lines",
        [](int&, const bool, const char* const [], std::string_view,
            std::vector<config>&)
        {
            g_flags |= config_flags::bol_eol;
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
            g_flags |= config_flags::negate;
        }
    },
    {
        option::type::misc,
        'V',
        "version",
        nullptr,
        "print version information and exit",
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
        "display this help and exit",
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
        "stop after NUM matches",
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
        "print the filename for each match",
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
        "suppress the prefixing filename on output",
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
        "print LABEL as filename for standard input",
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
        "show only the part of a line matching PATTERN",
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
        "TYPE is `binary', `text', or `without-match'",
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
                throw gg_error("gram_grep: unknown binary-files type");
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
        "FILE_PATTERN",
        "skip files and directories matching FILE_PATTERN",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);

            const std::string str(value);
            auto pathnames = split(str.c_str(), ';');

            for (const auto& p : pathnames)
            {
                auto pn = p.data();

                if (*pn == '!')
                    g_options._exclude._negative.emplace_back(pn, pn + p.size(),
                        is_windows());
                else
                    g_options._exclude._positive.emplace_back(pn, pn + p.size(),
                        is_windows());
            }
        }
    },
    {
        option::type::output,
        '\0',
        "exclude-dir",
        "PATTERN",
        "directories that match PATTERN will be skipped",
        [](int& i, const bool longp, const char* const argv[],
            std::string_view value, std::vector<config>&)
        {
            validate_value(i, argv, longp, value);

            const std::string str(value);
            auto pathnames = split(str.c_str(), ';');

            for (const auto& p : pathnames)
            {
                if (p[0] == '!')
                {
                    g_options._exclude_dirs._negative.
                        emplace_back(std::string(&p[1],
                            std::to_address(p.end())), is_windows());
                }
                else
                {
                    g_options._exclude_dirs._positive.
                        emplace_back(std::string(p), is_windows());
                }
            }
        }
    },
    {
        option::type::output,
        'L',
        "files-without-match",
        nullptr,
        "print only names of FILEs containing no match",
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
        "print only names of FILEs containing matches",
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
        "print only a count of matches per FILE",
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
        '\0',
        "color",
        nullptr,
        nullptr,
        colour
    },
    {
        option::type::context,
        '\0',
        "colour",
        nullptr,
        "use markers to highlight the matching strings",
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
            g_flags |= config_flags::extend_search;
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
            g_flags |= config_flags::negate | config_flags::all;
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
            g_flags |= config_flags::ret_prev_match;
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
