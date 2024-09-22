#include "pch.h"

#include <format>
//#include <parsertl/debug.hpp>
#include <parsertl/generator.hpp>
#include "gg_error.hpp"
#include <lexertl/memory_file.hpp>
#include "types.hpp"

extern config_parser g_config_parser;
extern parser* g_curr_parser;
extern uparser* g_curr_uparser;
extern bool g_force_unicode;
extern bool g_modify;

std::string unescape(const std::string_view& vw)
{
    std::string ret;
    bool escape = false;

    for (const char& c : vw)
    {
        if (c == '\\')
            escape = true;
        else
        {
            if (escape)
            {
                lexertl::detail::basic_re_tokeniser_state
                    <char, uint16_t> state(&c, &vw.back() + 1, 1, 0,
                        std::locale(), nullptr);

                ret += lexertl::detail::basic_re_tokeniser_helper
                    <char, char, uint16_t>::chr(state);
                escape = false;
            }
            else
                ret += c;
        }
    }

    return ret;
}
/*
static std::string unescape_str(const char* first, const char* second)
{
    std::string ret = unescape(std::string_view(first, second));

    for (std::size_t idx = ret.find('\''); idx != std::string::npos;
        idx = ret.find('\'', idx + 1))
    {
        if (ret[idx + 1] == '\'')
            ret.replace(idx, 2, 1, '\'');
    }

    return ret;
}
*/
void build_config_parser()
{
    parsertl::rules grules;
    lexertl::rules lrules;

    grules.token("Charset ExitState Index Integer Literal Macro MacroName "
        "Name NL Number Repeat ScriptString StartState String");

    grules.push("start", "file");
    grules.push("file",
        "directives '%%' grules '%%' rx_macros '%%' rx_rules '%%'");
    grules.push("directives", "%empty "
        "| directives directive");
    grules.push("directive", "NL");

    // Read and set %captures
    g_config_parser._actions[grules.push("directive", "'%captures' NL")] =
        [](config_state& state, const config_parser&)
        {
            state._grules.flags(*parsertl::rule_flags::enable_captures);
        };
    // Read and store %left entries
    g_config_parser._actions[grules.push("directive", "'%left' tokens NL")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string tokens = token.str();

            state._grules.left(tokens.c_str());
        };
    // Read and store %nonassoc entries
    g_config_parser._actions[grules.push("directive",
        "'%nonassoc' tokens NL")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string tokens = token.str();

            state._grules.nonassoc(tokens.c_str());
        };
    // Read and store %precedence entries
    g_config_parser._actions[grules.push("directive",
        "'%precedence' tokens NL")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string tokens = token.str();

            state._grules.precedence(tokens.c_str());
        };
    // Read and store %right entries
    g_config_parser._actions[grules.push("directive", "'%right' tokens NL")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string tokens = token.str();

            state._grules.right(tokens.c_str());
        };
    // Read and store %start
    g_config_parser._actions[grules.push("directive", "'%start' Name NL")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string name = token.str();

            state._grules.start(name.c_str());
        };
    // Read and store %token entries
    g_config_parser._actions[grules.push("directive", "'%token' tokens NL")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string tokens = token.str();

            state._grules.token(tokens.c_str());
        };
    grules.push("tokens", "token "
        "| tokens token");
    grules.push("token", "Literal | Name");
    // Read and store %option caseless
    g_config_parser._actions[grules.push("directive",
        "'%option' 'caseless' NL")] =
        [](config_state& state, const config_parser&)
        {
            if (g_force_unicode)
                state._lurules.flags(state._lurules.flags() |
                    *lexertl::regex_flags::icase);
            else
                state._lrules.flags(state._lrules.flags() |
                    *lexertl::regex_flags::icase);
        };
    // Read and store %x entries
    g_config_parser._actions[grules.push("directive", "'%x' names NL")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& names = state._results.dollar(1, parser._gsm,
                state._productions);
            const char* start = names.first;
            const char* curr = start;

            for (; curr != names.second; ++curr)
            {
                if (*curr == ' ' || *curr == '\t')
                {
                    if (g_force_unicode)
                        state._lurules.push_state(std::string(start, curr).c_str());
                    else
                        state._lrules.push_state(std::string(start, curr).c_str());

                    do
                    {
                        ++curr;
                    } while (curr != names.second &&
                        (*curr == ' ' || *curr == '\t'));

                    start = curr;
                }
            }

            if (start != curr)
            {
                if (g_force_unicode)
                    state._lurules.push_state(std::string(start, curr).c_str());
                else
                    state._lrules.push_state(std::string(start, curr).c_str());
            }
        };
    grules.push("names", "Name "
        "| names Name");

    // Grammar rules
    grules.push("grules", "%empty "
        "| grules grule");

    grules.push("grule", "lhs ':' production ';'");
    g_config_parser._actions[grules.push("lhs", "Name")] =
        [](config_state& state, const config_parser& parser)
        {
            state._lhs = state._results.dollar(0, parser._gsm,
                state._productions).str();
        };
    grules.push("production", "opt_production "
        "| production '|' opt_production");
    g_config_parser._actions[grules.push("opt_production",
        "opt_list opt_prec opt_script")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& item1 = state._results.dollar(0, parser._gsm,
                state._productions);
            const auto& item2 = state._results.dollar(1, parser._gsm,
                state._productions);
            const auto rhs = std::string(item1.first, item2.second);
            const uint16_t prod_index = state._grules.push(state._lhs, rhs);
            auto iter = g_force_unicode ?
                g_curr_uparser->_actions.find(prod_index) :
                g_curr_parser->_actions.find(prod_index);

            if (iter != (g_force_unicode ? g_curr_uparser->_actions.end() :
                g_curr_parser->_actions.end()))
            {
                for (const auto& cmd : iter->second._commands)
                {
                    if (cmd._param1 >=
                        state._grules.grammar().back()._rhs._symbols.size())
                    {
                        throw gg_error(std::format("Index ${} is out of range.",
                            cmd._param1 + 1));
                    }

                    if (cmd._param2 >=
                        state._grules.grammar().back()._rhs._symbols.size())
                    {
                        throw gg_error(std::format("Index ${} is out of range.",
                            cmd._param2 + 1));
                    }

                    if (cmd._param1 == cmd._param2 && cmd._second1 > cmd._second2)
                    {
                        throw gg_error(std::format("Index ${} cannot have "
                            "first following second.",
                            cmd._param2 + 1));
                    }
                }
            }
        };
    grules.push("opt_list", "%empty "
        "| '%empty' "
        "| rhs_list");
    grules.push("rhs_list", "rhs "
        "| rhs_list rhs");
    grules.push("rhs", "Literal "
        "| Name "
        "| '[' production ']' "
        "| rhs '?' "
        "| rhs '*' "
        "| rhs '+' "
        "| '(' production ')'");
    grules.push("opt_prec", "%empty "
        "| '%prec' Literal "
        "| '%prec' Name");
    grules.push("opt_script", "%empty");
    g_config_parser._actions[grules.push("opt_script", "'{' cmd_list '}'")] =
        [](config_state& state, const config_parser& parser)
        {
            if (!parser._gsm._captures.empty())
                throw gg_error("Cannot mix %captures and actions.");

            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);

            if (token.first == token.second)
            {
                const auto size = static_cast<uint16_t>(state._grules.grammar().size());

                // No commands
                if (g_force_unicode)
                    g_curr_uparser->_reduce_set.insert(size);
                else
                    g_curr_parser->_reduce_set.insert(size);
            }
        };
    grules.push("cmd_list", "%empty "
        "| cmd_list single_cmd ';'");
    grules.push("single_cmd", "cmd");

    g_config_parser._actions[grules.push("cmd", "'match' '=' Index")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>(atoi(token.first + 1) - 1);

            if (g_force_unicode)
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::assign, index, match_cmd(0, 0)));
            else
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::assign, index, match_cmd(0, 0)));
        };
    g_config_parser._actions[grules.push("cmd",
        "'match' '=' 'substr' '(' Index ',' Integer ',' Integer ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>
                (atoi(token4.first + 1) - 1);
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const auto& token8 = state._results.dollar(8, parser._gsm,
                state._productions);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::assign, index,
                        match_cmd(static_cast<uint16_t>(atoi(token6.first)),
                        static_cast<uint16_t>(atoi(token8.first)))));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::assign, index,
                        match_cmd(static_cast<uint16_t>(atoi(token6.first)),
                        static_cast<uint16_t>(atoi(token8.first)))));
            }
        };
    g_config_parser._actions[grules.push("cmd", "'match' '+=' Index")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>(atoi(token.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::append, index, match_cmd(0, 0)));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::append, index, match_cmd(0, 0)));
            }
        };
    g_config_parser._actions[grules.push("cmd",
        "'match' '+=' 'substr' '(' Index ',' Integer ',' Integer ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>(atoi(token4.first + 1) - 1);
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const auto& token8 = state._results.dollar(8, parser._gsm,
                state._productions);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::append, index,
                        match_cmd(static_cast<uint16_t>(atoi(token6.first)),
                        static_cast<uint16_t>(atoi(token8.first)))));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::append, index,
                        match_cmd(static_cast<uint16_t>(atoi(token6.first)),
                        static_cast<uint16_t>(atoi(token8.first)))));
            }
        };
    g_config_parser._actions[grules.push("cmd",
        "'print' '(' ret_function ')'")] =
        [](config_state& state, const config_parser&)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::print, print_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::print, print_cmd()));
            }

            state._print = true;
        };

    g_config_parser._actions[grules.push("single_cmd",
        "mod_cmd")] =
        [](config_state&, const config_parser&)
        {
            g_modify = true;
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'erase' '(' Index ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto rule_idx = static_cast<uint16_t>(state._grules.
                grammar().size());
            const auto index = static_cast<uint16_t>(atoi(token.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::erase, index, erase_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::erase, index, erase_cmd()));
            }
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'erase' '(' Index ',' Index ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>(state._grules.
                grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const auto index1 = static_cast<uint16_t>
                (atoi(token2.first + 1) - 1);
            const auto index2 = static_cast<uint16_t>
                (atoi(token4.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::erase, index1, index2, erase_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::erase, index1, index2, erase_cmd()));
            }
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'erase' '(' Index '.' first_second ',' Index '.' first_second ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>(state._grules.
                grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const bool second1 = *state._results.dollar(4, parser._gsm,
                state._productions).first == 's';
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const bool second2 = *state._results.dollar(8, parser._gsm,
                state._productions).first == 's';
            const auto index1 = static_cast<uint16_t>
                (atoi(token2.first + 1) - 1);
            const auto index2 = static_cast<uint16_t>
                (atoi(token6.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::erase, index1, second1, index2,
                        second2, erase_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::erase, index1, second1, index2,
                        second2, erase_cmd()));
            }
        };

    g_config_parser._actions[grules.push("mod_cmd",
        "'insert' '(' Index ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>
                (atoi(token2.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::insert, index, insert_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::insert, index, insert_cmd()));
            }
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'insert' '(' Index '.' 'second' ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>
                (atoi(token2.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::insert, index, true, insert_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::insert, index, true, insert_cmd()));
            }
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'replace' '(' Index ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>(atoi(token2.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace, index, replace_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace, index, replace_cmd()));
            }
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'replace' '(' Index ',' Index ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index1 = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const auto index2 = static_cast<uint16_t>(atoi(token4.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace, index1, index2,
                        replace_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace, index1, index2,
                        replace_cmd()));
            }
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'replace' '(' Index '.' first_second ',' "
        "Index '.' first_second ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index1 = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
            const bool second1 = *state._results.dollar(4, parser._gsm,
                state._productions).first == 's';
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const auto index2 = static_cast<uint16_t>(atoi(token6.first + 1) - 1);
            const bool second2 = *state._results.dollar(8, parser._gsm,
                state._productions).first == 's';

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace, index1, second1, index2,
                        second2, replace_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace, index1, second1, index2,
                        second2, replace_cmd()));
            }
        };
    grules.push("first_second", "'first' | 'second'");
    g_config_parser._actions[grules.push("mod_cmd",
        "'replace_all' '(' Index ',' ret_function ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto index = static_cast<uint16_t>(atoi(token2.first + 1) - 1);

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace_all_inplace,
                        index, replace_all_inplace_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace_all_inplace, index,
                        replace_all_inplace_cmd()));
            }
        };

    g_config_parser._actions[grules.push("ret_function", "ScriptString")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());
            std::string text = state._results.dollar(0, parser._gsm,
                state._productions).substr(1, 1);

            g_curr_parser->_actions[rule_idx].
                _arguments.push_front(std::move(text));
        };
    grules.push("ret_function", "perform_exec "
        "| perform_format "
        "| perform_replace_all");
    g_config_parser._actions[grules.push("perform_exec", "'exec' '(' ret_function ')'")] =
        [](config_state& state, const config_parser&)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::exec, exec_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::exec, exec_cmd()));
            }
        };
    g_config_parser._actions[grules.push("perform_format",
        "'format' '(' ret_function format_params ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());

            if (const auto& token = state._results.dollar(2, parser._gsm,
                state._productions);
                *token.first == '\'' && *(token.second - 1) == '\'')
            {
                static const char szTarget[] = { '{', '}' };
                const char* curr = std::search(token.first, token.second,
                    std::begin(szTarget), std::end(szTarget));
                std::size_t count = 0;

                for (; curr != token.second;
                    curr = std::search(curr + 2, token.second,
                        std::begin(szTarget), std::end(szTarget)))
                {
                    ++count;
                }

                if (count != state._varargs)
                {
                    std::ostringstream ss;

                    ss << "Too ";

                    if (count < state._varargs)
                        ss << "many";
                    else
                        ss << "few";

                    ss << " arguments for the format string";
                    throw gg_error(ss.str());
                }
            }

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::format,
                        format_cmd(state._varargs)));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::format,
                        format_cmd(state._varargs)));
            }

            state._varargs = 0;
        };
    g_config_parser._actions[grules.push("perform_replace_all",
        "'replace_all' '(' ret_function ',' ret_function ',' ret_function ')'")] =
        [](config_state& state, const config_parser&)
        {
            const auto rule_idx = static_cast<uint16_t>
                (state._grules.grammar().size());

            if (g_force_unicode)
            {
                g_curr_uparser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace_all, replace_all_cmd()));
            }
            else
            {
                g_curr_parser->_actions[rule_idx].
                    emplace(cmd(cmd_type::replace_all, replace_all_cmd()));
            }
        };
    grules.push("format_params", "%empty");
    g_config_parser._actions[grules.push("format_params",
        "format_params ',' ret_function")] =
        [](config_state& state, const config_parser&)
        {
            ++state._varargs;
        };

    // Token regex macros
    grules.push("rx_macros", "%empty");
    g_config_parser._actions[grules.push("rx_macros",
        "rx_macros MacroName regex")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string name = token.str();
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();

            if (g_force_unicode)
                state._lurules.insert_macro(name.c_str(), regex.c_str());
            else
                state._lrules.insert_macro(name.c_str(), regex.c_str());
        };

    // Tokens
    grules.push("rx_rules", "%empty");
    g_config_parser._actions[grules.push("rx_rules", "rx_rules regex Number")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = token.str();
            const std::string number = state._results.dollar(2, parser._gsm,
                state._productions).str();

            if (g_force_unicode)
                state._lurules.push(regex,
                    static_cast<uint16_t>(atoi(number.c_str())));
            else
                state._lrules.push(regex,
                    static_cast<uint16_t>(atoi(number.c_str())));
        };
    g_config_parser._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);

            if (g_force_unicode)
                state._lurules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(), regex,
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
            else
                state._lrules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(), regex,
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
        };
    g_config_parser._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState Number")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);
            const std::string number = state._results.dollar(4, parser._gsm,
                state._productions).str();

            if (g_force_unicode)
                state._lurules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, static_cast<uint16_t>(atoi(number.c_str())),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
            else
                state._lrules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, static_cast<uint16_t>(atoi(number.c_str())),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
        };
    g_config_parser._actions[grules.push("rx_rules",
        "rx_rules regex Literal")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = token.str();
            const std::string literal = state._results.dollar(2, parser._gsm,
                state._productions).str();

            if (g_force_unicode)
                state._lurules.push(regex, state._grules.token_id(literal.c_str()));
            else
                state._lrules.push(regex, state._grules.token_id(literal.c_str()));
        };
    g_config_parser._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState Literal")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);
            const std::string literal = state._results.dollar(4, parser._gsm,
                state._productions).str();

            if (g_force_unicode)
                state._lurules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, state._grules.token_id(literal.c_str()),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
            else
                state._lrules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, state._grules.token_id(literal.c_str()),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
        };
    g_config_parser._actions[grules.push("rx_rules", "rx_rules regex Name")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = token.str();
            const std::string name = state._results.dollar(2, parser._gsm,
                state._productions).str();

            if (g_force_unicode)
                state._lurules.push(regex, state._grules.token_id(name.c_str()));
            else
                state._lrules.push(regex, state._grules.token_id(name.c_str()));
        };
    g_config_parser._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState Name")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);
            const std::string name = state._results.dollar(4, parser._gsm,
                state._productions).str();

            if (g_force_unicode)
                state._lurules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, state._grules.token_id(name.c_str()),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
            else
                state._lrules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, state._grules.token_id(name.c_str()),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
        };
    g_config_parser._actions[grules.push("rx_rules",
        "rx_rules regex 'skip()'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = token.str();

            if (g_force_unicode)
                state._lurules.push(regex, config_state::lurules::skip());
            else
                state._lrules.push(regex, lexertl::rules::skip());
        };
    g_config_parser._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState 'skip()'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);

            if (g_force_unicode)
                state._lurules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, config_state::lurules::skip(),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
            else
                state._lrules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, lexertl::rules::skip(),
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
        };

    // Regex
    grules.push("regex", "rx "
        "| '^' rx "
        "| rx '$' "
        "| '^' rx '$'");
    grules.push("rx", "sequence "
        "| rx '|' sequence");
    grules.push("sequence", "item "
        "| sequence item");
    grules.push("item", "atom "
        "| atom repeat");
    grules.push("atom", "Charset "
        "| Macro "
        "| String "
        "| '(' rx ')'");
    grules.push("repeat", "'?' "
        "| '\?\?' "
        "| '*' "
        "| '*?' "
        "| '+' "
        "| '+?' "
        "| Repeat");

    std::string warnings;

    //parsertl::debug::dump(grules, std::cout);
    parsertl::generator::build(grules, g_config_parser._gsm, &warnings);

    if (!warnings.empty())
        std::cerr << "Config parser warnings: " << warnings;

    lrules.push_state("OPTION");
    lrules.push_state("GRULE");
    lrules.push_state("SCRIPT");
    lrules.push_state("MACRO");
    lrules.push_state("REGEX");
    lrules.push_state("RULE");
    lrules.push_state("ID");
    lrules.insert_macro("c_comment", R"("/*"(?s:.)*?"*/")");
    lrules.insert_macro("control_char", "c[@A-Za-z]");
    lrules.insert_macro("hex", "x[0-9A-Fa-f]+");
    lrules.insert_macro("escape", R"(\\([^0-9cx]|\d{1,3}|{hex}|{control_char}))");
    lrules.insert_macro("macro_name", R"([A-Z_a-z][-\w]*)");
    lrules.insert_macro("nl", "\r?\n");
    lrules.insert_macro("posix_name", "alnum|alpha|blank|cntrl|digit|graph|"
        "lower|print|punct|space|upper|xdigit");
    lrules.insert_macro("posix", R"(\[:{posix_name}:\])");
    lrules.insert_macro("spc_tab", "[ \t]+");
    lrules.insert_macro("state_name", R"([A-Z_a-z]\w*)");

    lrules.push("INITIAL,OPTION", "{spc_tab}", lexertl::rules::skip(), ".");
    lrules.push("{nl}", grules.token_id("NL"));
    lrules.push("%captures", grules.token_id("'%captures'"));
    lrules.push("%left", grules.token_id("'%left'"));
    lrules.push("%nonassoc", grules.token_id("'%nonassoc'"));
    lrules.push("%precedence", grules.token_id("'%precedence'"));
    lrules.push("%right", grules.token_id("'%right'"));
    lrules.push("%start", grules.token_id("'%start'"));
    lrules.push("%token", grules.token_id("'%token'"));
    lrules.push("%x", grules.token_id("'%x'"));
    lrules.push("INITIAL", "%option", grules.token_id("'%option'"), "OPTION");
    lrules.push("OPTION", "caseless", grules.token_id("'caseless'"), "INITIAL");
    lrules.push("INITIAL", "%%", grules.token_id("'%%'"), "GRULE");

    lrules.push("GRULE", ":", grules.token_id("':'"), ".");
    lrules.push("GRULE", R"(\[)", grules.token_id("'['"), ".");
    lrules.push("GRULE", R"(\])", grules.token_id("']'"), ".");
    lrules.push("GRULE", R"(\()", grules.token_id("'('"), ".");
    lrules.push("GRULE", R"(\))", grules.token_id("')'"), ".");
    lrules.push("GRULE", R"(\?)", grules.token_id("'?'"), ".");
    lrules.push("GRULE", R"(\*)", grules.token_id("'*'"), ".");
    lrules.push("GRULE", R"(\+)", grules.token_id("'+'"), ".");
    lrules.push("GRULE", R"(\|)", grules.token_id("'|'"), ".");
    lrules.push("GRULE", "%prec", grules.token_id("'%prec'"), ".");
    lrules.push("GRULE", ";", grules.token_id("';'"), ".");
    lrules.push("GRULE", R"(\{)", grules.token_id("'{'"), "SCRIPT");
    lrules.push("SCRIPT", R"(\})", grules.token_id("'}'"), "GRULE");
    lrules.push("SCRIPT", "=", grules.token_id("'='"), ".");
    lrules.push("SCRIPT", ",", grules.token_id("','"), ".");
    lrules.push("SCRIPT", R"(\()", grules.token_id("'('"), ".");
    lrules.push("SCRIPT", R"(\))", grules.token_id("')'"), ".");
    lrules.push("SCRIPT", R"(\.)", grules.token_id("'.'"), ".");
    lrules.push("SCRIPT", ";", grules.token_id("';'"), ".");
    lrules.push("SCRIPT", R"(\+=)", grules.token_id("'+='"), ".");
    lrules.push("SCRIPT", "erase", grules.token_id("'erase'"), ".");
    lrules.push("SCRIPT", "exec", grules.token_id("'exec'"), ".");
    lrules.push("SCRIPT", "first", grules.token_id("'first'"), ".");
    lrules.push("SCRIPT", "format", grules.token_id("'format'"), ".");
    lrules.push("SCRIPT", "insert", grules.token_id("'insert'"), ".");
    lrules.push("SCRIPT", "match", grules.token_id("'match'"), ".");
    lrules.push("SCRIPT", "print", grules.token_id("'print'"), ".");
    lrules.push("SCRIPT", "replace", grules.token_id("'replace'"), ".");
    lrules.push("SCRIPT", "replace_all", grules.token_id("'replace_all'"), ".");
    lrules.push("SCRIPT", "second", grules.token_id("'second'"), ".");
    lrules.push("SCRIPT", "substr", grules.token_id("'substr'"), ".");
    lrules.push("SCRIPT", R"(\d+)", grules.token_id("Integer"), ".");
    lrules.push("SCRIPT", R"(\s+)", lexertl::rules::skip(), ".");
    lrules.push("SCRIPT", R"(\$[1-9]\d*)", grules.token_id("Index"), ".");
    lrules.push("SCRIPT", "'(''|[^'])*'", grules.token_id("ScriptString"), ".");
    lrules.push("GRULE", "{spc_tab}|{nl}", lexertl::rules::skip(), ".");
    lrules.push("GRULE", "%empty", grules.token_id("'%empty'"), ".");
    lrules.push("GRULE", "%%", grules.token_id("'%%'"), "MACRO");
    lrules.push("INITIAL,GRULE,SCRIPT", "{c_comment}", lexertl::rules::skip(), ".");
    // Bison supports single line comments
    lrules.push("INITIAL,GRULE,SCRIPT", R"("//".*)", lexertl::rules::skip(), ".");
    lrules.push("INITIAL,GRULE,ID",
        R"('({escape}|[^\\\r\n'])+'|\"({escape}|[^\\\r\n"])+\")",
        grules.token_id("Literal"), ".");
    lrules.push("INITIAL,GRULE,ID", "[.A-Z_a-z][-.0-9A-Z_a-z]*",
        grules.token_id("Name"), ".");
    lrules.push("ID", R"([1-9]\d*)", grules.token_id("Number"), ".");

    lrules.push("MACRO,RULE", "%%", grules.token_id("'%%'"), "RULE");
    lrules.push("MACRO", "{macro_name}", grules.token_id("MacroName"), "REGEX");
    lrules.push("MACRO", "{c_comment}", lexertl::rules::skip(), ".");
    lrules.push("MACRO,REGEX", "{nl}", lexertl::rules::skip(), "MACRO");

    lrules.push("REGEX", "{spc_tab}", lexertl::rules::skip(), ".");
    lrules.push("RULE", "^{spc_tab}({c_comment}({spc_tab}|{c_comment})*)?",
        lexertl::rules::skip(), ".");
    lrules.push("RULE", R"(^<(\*|{state_name}(,{state_name})*)>)",
        grules.token_id("StartState"), ".");
    lrules.push("REGEX,RULE", R"(\^)", grules.token_id("'^'"), ".");
    lrules.push("REGEX,RULE", R"(\$)", grules.token_id("'$'"), ".");
    lrules.push("REGEX,RULE", R"(\|)", grules.token_id("'|'"), ".");
    lrules.push("REGEX,RULE", R"(\((\?(-?[is])*:)?)", grules.token_id("'('"), ".");
    lrules.push("REGEX,RULE", R"(\))", grules.token_id("')'"), ".");
    lrules.push("REGEX,RULE", R"(\?)", grules.token_id("'?'"), ".");
    lrules.push("REGEX,RULE", R"(\?\?)", grules.token_id("'\?\?'"), ".");
    lrules.push("REGEX,RULE", R"(\*)", grules.token_id("'*'"), ".");
    lrules.push("REGEX,RULE", R"(\*\?)", grules.token_id("'*?'"), ".");
    lrules.push("REGEX,RULE", R"(\+)", grules.token_id("'+'"), ".");
    lrules.push("REGEX,RULE", R"(\+\?)", grules.token_id("'+?'"), ".");
    lrules.push("REGEX,RULE", R"({escape}|(\[^?({escape}|{posix}|[^\\\]])*\])|\S)",
        grules.token_id("Charset"), ".");
    lrules.push("REGEX,RULE", R"(\{{macro_name}\})", grules.token_id("Macro"), ".");
    lrules.push("REGEX,RULE", R"(\{\d+(,(\d+)?)?\}[?]?)",
        grules.token_id("Repeat"), ".");
    lrules.push("REGEX,RULE", R"(\"(\\.|[^\\\r\n"])*\")",
        grules.token_id("String"), ".");

    lrules.push("RULE,ID", "{spc_tab}({c_comment}({spc_tab}|{c_comment})*)?",
        lexertl::rules::skip(), "ID");
    lrules.push("RULE", R"(<([.<]|{state_name}|>{state_name}(:{state_name})?)>)",
        grules.token_id("ExitState"), "ID");
    lrules.push("RULE,ID", "{nl}", lexertl::rules::skip(), "RULE");
    lrules.push("ID", R"(skip\s*\(\s*\))", grules.token_id("'skip()'"), "RULE");
    lexertl::generator::build(lrules, g_config_parser._lsm);
}
