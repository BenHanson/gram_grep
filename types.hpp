#pragma once

#include <array>
#include <lexertl/iterator.hpp>
#include <memory>
#include <lexertl/memory_file.hpp>
#include <regex>
#include <lexertl/rules.hpp>
#include <parsertl/rules.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/state_machine.hpp>
#include <string_view>
#include <parsertl/token.hpp>
#include <lexertl/utf_iterators.hpp>
#include <variant>
#include <vector>
#include <wildcardtl/wildcard.hpp>

enum config_flags
{
    none = 0,
    icase = 1,
    bol_eol = 2,
    whole_word = 4,
    negate = 8,
    all = 16,
    extend_search = 32,
    ret_prev_match = 64,
    grep = 128,
    egrep = 256
};

enum class show_filename
{
    undefined,
    no,
    yes
};

using condition_map = std::map<uint16_t, std::regex>;

struct wildcards
{
    struct wildcard
    {
        wildcardtl::wildcard _wc;
        std::string _pathname;
    };

    std::vector<wildcard> _positive;
    std::vector<wildcard> _negative;
};

enum class binary_files
{
    binary, text, without_match
};

enum class dump
{
    no, text, dot
};

enum class line_numbers
{
    none, yes, with_parens
};

enum class pathname_only
{
    no, yes, negated
};

enum class pattern_type
{
    none, extended, fixed, basic, perl, flex
};

struct options
{
    binary_files _binary_files = binary_files::binary;
    bool _byte_offset = false;
    std::string _checkout;
    bool _colour = false;
    condition_map _conditions;
    dump _dump = dump::no;
    bool _dump_argv = false;
    wildcards _exclude;
    wildcards _exclude_dirs;
    std::string _exec;
    bool _force_unicode = false;
    bool _force_write = false;
    bool _initial_tab = false;
    std::string _label;
    bool _line_buffered = false;
    line_numbers _line_numbers = line_numbers::none;
    std::size_t _max_count = 0;
    bool _no_messages = false;
    bool _only_matching = false;
    pathname_only _pathname_only = pathname_only::no;
    pattern_type _pattern_type = pattern_type::none;
    bool _perform_output = false;
    std::string _print;
    bool _print_null = false;
    bool _quiet = false;
    bool _recursive = false;
    std::string _replace;
    bool _show_count = false;
    show_filename _show_filename = show_filename::undefined;
    bool _show_version = false;
    std::string _shutdown;
    std::string _startup;
    bool _summary = false;
    bool _whole_match = false;
    std::vector<lexertl::memory_file> _word_list_files;
    bool _writable = false;

    // Colours:
    std::string _sl_text;
    std::string _cx_text; // Not used yet
    bool _rv = false; // Not used yet
    std::string _ms_text = "\x1b[01;31m";
    std::string _mc_text = "\x1b[01;31m"; // Not used yet
    std::string _fn_text = "\x1b[35m";
    std::string _ln_text = "\x1b[32m";
    std::string _bn_text = "\x1b[32m";
    std::string _se_text = "\x1b[36m";
    bool _ne = false;
    std::string _wa_text = "\x1b[38;5;229m";
};

struct match_type_base
{
    unsigned int _flags = config_flags::none;
    condition_map _conditions;

    virtual ~match_type_base() = default;
};

struct text : match_type_base
{
    std::string _text;
};

struct regex : match_type_base
{
    std::regex _rx;
};

struct lexer : match_type_base
{
    lexertl::state_machine _sm;
};

struct ulexer : match_type_base
{
    lexertl::u32state_machine _sm;
};

struct cmd
{
    enum class type
    {
        unknown,
        string,
        assign,
        append,
        erase,
        format,
        insert,
        print,
        replace,
        replace_all,
        replace_all_inplace,
        system
    };

    type _type = type::unknown;
    uint16_t _param1 = 0;
    bool _second1 = false;
    uint16_t _param2 = 0;
    bool _second2 = true;

    cmd(const type type) :
        _type(type)
    {
    }

    cmd(const type type, const uint16_t param) :
        _type(type),
        _param1(param),
        _param2(param)
    {
    }

    cmd(const type type, const uint16_t param, const bool second) :
        _type(type),
        _param1(param),
        _second1(second),
        _param2(param)
    {
    }

    cmd(const type type, const uint16_t param1, const uint16_t param2) :
        _type(type),
        _param1(param1),
        _param2(param2)
    {
    }

    cmd(const type type, const uint16_t param1, const bool second1,
        const uint16_t param2, const bool second2) :
        _type(type),
        _param1(param1),
        _second1(second1),
        _param2(param2),
        _second2(second2)
    {
    }

    virtual ~cmd() = default;

    virtual void push(cmd*)
    {
        // Do nothing by default
    }
};

struct erase_cmd : cmd
{
    explicit erase_cmd(const uint16_t index) :
        cmd(type::erase, index)
    {
    }

    explicit erase_cmd(const uint16_t index1, const uint16_t index2) :
        cmd(type::erase, index1, index2)
    {
    }

    explicit erase_cmd(const uint16_t param1, const bool second1,
        const uint16_t param2, const bool second2) :
        cmd(type::erase, param1, second1, param2, second2)
    {
    }
};

struct system_cmd : cmd
{
    cmd* _param = nullptr;

    explicit system_cmd() :
        cmd(cmd::type::system)
    {
    }

    void push(cmd* command) override
    {
        _param = command;
    }
};

struct format_cmd : cmd
{
    std::vector<cmd*> _params;

    explicit format_cmd() :
        cmd(type::format)
    {
    }

    void push(cmd* command) override
    {
        _params.push_back(command);
    }
};

struct insert_cmd : cmd
{
    cmd* _param = nullptr;

    explicit insert_cmd() :
        cmd(cmd::type::insert)
    {
    }

    void push(cmd* command) override
    {
        _param = command;
    }
};

struct match_cmd : cmd
{
    uint16_t _front = 0;
    uint16_t _back = 0;

    match_cmd(const type type, const uint16_t index) :
        cmd(type, index)
    {
    }
};

struct print_cmd : cmd
{
    cmd* _param = nullptr;

    explicit print_cmd() :
        cmd(cmd::type::print)
    {
    }

    void push(cmd* command) override
    {
        _param = command;
    }
};

struct replace_cmd : cmd
{
    cmd* _param = nullptr;

    replace_cmd() :
        cmd(cmd::type::replace)
    {
    }

    void push(cmd* command) override
    {
        _param = command;
    }
};

struct replace_all_cmd : cmd
{
    std::vector<cmd*> _params;

    replace_all_cmd() :
        cmd(type::replace_all)
    {
    }

    void push(cmd* command) override
    {
        _params.push_back(command);
    }
};

struct replace_all_inplace_cmd : cmd
{
    std::vector<cmd*> _params;

    replace_all_inplace_cmd() :
        cmd(type::replace_all_inplace)
    {
    }

    void push(cmd* command) override
    {
        _params.push_back(command);
    }
};

struct string_cmd : cmd
{
    std::string _str;

    explicit string_cmd(std::string&& str) :
        cmd(type::string),
        _str(std::move(str))
    {
    }
};

struct cmd_data
{
    cmd::type _type = cmd::type::unknown;
    std::size_t _param_count = 0;
    std::vector<std::string> _params;

    cmd_data(const cmd::type type, const std::size_t param_count) :
        _type(type),
        _param_count(param_count)
    {
    }

    bool ready() const
    {
        return _param_count == _params.size();
    }

    std::string system() const;
};

struct actions
{
    std::vector<std::shared_ptr<cmd>> _storage;
    std::vector<cmd*> _commands;
    std::vector<cmd*> _cmd_stack;
    std::vector<std::size_t> _index_stack;

    void emplace(std::shared_ptr<cmd> command)
    {
        _storage.emplace_back(std::move(command));
        _commands.push_back(_storage.back().get());
    }

    void push(std::shared_ptr<cmd> command)
    {
        _storage.emplace_back(std::move(command));
        _cmd_stack.push_back(_storage.back().get());
    }

    cmd* top()
    {
        return _cmd_stack.back();
    }

    void pop()
    {
        _cmd_stack.pop_back();
    }

    std::string exec(cmd* command);
};

struct parser_base : match_type_base
{
    parsertl::state_machine _gsm;
    std::set<uint16_t> _reduce_set;
    std::map<uint16_t, actions> _actions;
};

struct parser : parser_base
{
    lexertl::state_machine _lsm;
};

struct uparser : parser_base
{
    lexertl::u32state_machine _lsm;
};

struct word_list : match_type_base
{
    std::vector<std::string_view> _list;
};

enum class match_type
{
    // Must match order of variant in pipeline (below).
    // Note that dfa_regex always goes on the end
    // as it is converted to lexer or ulexer
    text, regex, lexer, ulexer, parser, uparser, word_list, dfa_regex
};

using pipeline = std::vector<std::variant<text, regex, lexer, ulexer,
    parser, uparser, word_list>>;

struct config
{
    match_type _type;
    std::string _param;
    unsigned int _flags = config_flags::none;
    condition_map _conditions;

    config(const match_type type, const std::string_view& param,
        const unsigned int flags, condition_map conditions) :
        _type(type),
        _param(param),
        _flags(flags),
        _conditions(std::move(conditions))
    {
    }
};

using token = parsertl::token<lexertl::criterator>;

struct config_state
{
    using lurules = lexertl::basic_rules<char, char32_t>;

    parsertl::rules _grules;
    lexertl::rules _lrules;
    lurules _lurules;
    bool _store_tokens = false;
    std::vector<std::string> _consume;
    lexertl::memory_file _mf;
    token::token_vector _productions;
    parsertl::match_results _results;
    std::string _lhs;
    bool _print = false;

    void parse(const unsigned int flags, const std::string& config_pathname);
};

struct condition_parser
{
    parsertl::state_machine _gsm;
    lexertl::state_machine _lsm;
};

struct config_parser;

using config_actions_map = std::map<uint16_t, void(*)(config_state& state,
    const config_parser& parser)>;

struct config_parser
{
    parsertl::state_machine _gsm;
    lexertl::state_machine _lsm;
    config_actions_map _actions;
};

struct match
{
    const char* _first = nullptr;
    const char* _second = nullptr;
    const char* _eoi = nullptr;

    match(const char* first, const char* second, const char* eoi) :
        _first(first),
        _second(second),
        _eoi(eoi)
    {
    }

    std::string_view view() const
    {
        return std::string_view(_first, _eoi);
    }
};

using capture_vector = std::vector<std::vector<std::string_view>>;

using utf8_iterator = lexertl::basic_utf8_in_iterator<const char*, char32_t>;
using utf8results = lexertl::recursive_match_results<utf8_iterator>;
using crutf8iterator =
    lexertl::iterator<utf8_iterator, lexertl::u32state_machine, utf8results>;

[[nodiscard]] std::string exec_ret(const std::string& cmd);