#pragma once

#include <lexertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include <regex>
#include <lexertl/rules.hpp>
#include <parsertl/rules.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/state_machine.hpp>
#include <parsertl/token.hpp>
#include <lexertl/utf_iterators.hpp>
#include <variant>

enum config_flags
{
    none = 0,
    whole_word = 1,
    negate = 2,
    negate_all = 4,
    extend_search = 8
};

struct base
{
    unsigned int _flags = config_flags::none;

    virtual ~base() = default;
};

struct text : base
{
    std::string _text;
};

struct regex : base
{
    std::regex _rx;
};

struct lexer : base
{
    lexertl::state_machine _sm;
};

struct ulexer : base
{
    lexertl::u32state_machine _sm;
};

struct erase_cmd
{
};

struct insert_cmd
{
    std::string _text;

    explicit insert_cmd(const std::string& text) :
        _text(text)
    {
    }
};

struct match_cmd
{
    uint16_t _front;
    uint16_t _back;

    match_cmd(const uint16_t front,
        const uint16_t back) noexcept :
        _front(front),
        _back(back)
    {
    }
};

struct print_cmd
{
    std::string _text;

    explicit print_cmd(const std::string& text) :
        _text(text)
    {
    }
};

struct replace_cmd
{
    std::string _text;

    explicit replace_cmd(const std::string& text) :
        _text(text)
    {
    }
};

struct replace_all_cmd
{
    std::regex _rx;
    std::string _text;

    replace_all_cmd(const std::string& rx, const std::string& text) :
        _rx(rx),
        _text(text)
    {
    }
};

enum class cmd_type
{
    unknown, assign, append, erase, insert, print, replace, replace_all
};

struct cmd
{
    using action = std::variant<erase_cmd, insert_cmd, match_cmd, print_cmd,
        replace_cmd, replace_all_cmd>;
    cmd_type _type = cmd_type::unknown;
    uint16_t _param1 = 0;
    bool _second1 = false;
    uint16_t _param2 = 0;
    bool _second2 = true;
    action _action;

    cmd(const cmd_type type, const action& action) :
        _type(type),
        _action(action)
    {
    }

    cmd(const cmd_type type, const uint16_t param, const action& action) :
        _type(type),
        _param1(param),
        _param2(param),
        _action(action)
    {
    }

    cmd(const cmd_type type, const uint16_t param, const bool second,
        const action& action) :
        _type(type),
        _param1(param),
        _second1(second),
        _param2(param),
        _action(action)
    {
    }

    cmd(const cmd_type type, const uint16_t param1, const uint16_t param2,
        const action& action) :
        _type(type),
        _param1(param1),
        _param2(param2),
        _action(action)
    {
    }

    cmd(const cmd_type type, const uint16_t param1, const bool second1,
        const uint16_t param2, const bool second2, const action& action) :
        _type(type),
        _param1(param1),
        _second1(second1),
        _param2(param2),
        _second2(second2),
        _action(action)
    {
    }
};

struct parser_base : base
{
    parsertl::state_machine _gsm;
    std::set<uint16_t> _reduce_set;
    std::map<uint16_t, std::vector<cmd>> _actions;
};

struct parser : parser_base
{
    lexertl::state_machine _lsm;
};

struct uparser : parser_base
{
    lexertl::u32state_machine _lsm;
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
};

enum class match_type
{
    // Must match order of variant in g_pipeline
    // Note that dfa_regex always goes on the end
    // as it is converted to lexer or ulexer
    text, regex, lexer, ulexer, parser, uparser, dfa_regex
};

using token = parsertl::token<lexertl::criterator>;

struct config_state
{
    using lurules = lexertl::basic_rules<char, char32_t>;

    parsertl::rules _grules;
    lexertl::rules _lrules;
    lurules _lurules;
    lexertl::memory_file _mf;
    token::token_vector _productions;
    parsertl::match_results _results;
    std::string _lhs;
    bool _print = false;

    void parse(const std::string& config_pathname);
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

using pipeline = std::vector<std::variant<text, regex, lexer, ulexer, parser, uparser>>;

using utf8_iterator = lexertl::basic_utf8_in_iterator<const char*, char32_t>;
using utf8results = lexertl::recursive_match_results<utf8_iterator>;
using crutf8iterator =
    lexertl::iterator<utf8_iterator, lexertl::u32state_machine, utf8results>;
