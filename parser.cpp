#include "pch.h"

#include <format>
//#include <parsertl/debug.hpp>
#include <parsertl/generator.hpp>
#include "gg_error.hpp"
#include "output.hpp"
#include "types.hpp"

extern options g_options;
extern condition_parser g_condition_parser;
extern config_parser g_config_parser;
extern parser* g_curr_parser;
extern uparser* g_curr_uparser;
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

std::string dedup_apostrophes(std::string str)
{
    for (std::size_t idx = str.find('\''); idx != std::string::npos;
        idx = str.find('\'', idx + 1))
    {
        if (str[idx + 1] == '\'')
            str.replace(idx, 2, 1, '\'');
    }

    return str;
}

std::string unescape_str(const std::string& str)
{
    return dedup_apostrophes(unescape(str));
}

template<typename T>
void push_ret_kwd(const config_state& state)
{
    const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
    auto command = std::make_shared<T>();
    actions* ptr = nullptr;

    if (g_options._force_unicode)
    {
        ptr = &g_curr_uparser->_actions[rule_idx];
    }
    else
    {
        ptr = &g_curr_parser->_actions[rule_idx];
    }

    ptr->_cmd_stack.back()->push(command.get());
    ptr->push(std::move(command));
}

template<typename T>
void push_kwd(const config_state& state)
{
    const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
    auto command = std::make_shared<T>();
    actions* ptr = nullptr;

    if (g_options._force_unicode)
    {
        ptr = &g_curr_uparser->_actions[rule_idx];
    }
    else
    {
        ptr = &g_curr_parser->_actions[rule_idx];
    }

    // Final command, emplace
    ptr->emplace(std::move(command));
    // Temporary copy for ret_functions to connect to
    ptr->_cmd_stack.push_back(ptr->_commands.back());
}

static void pop_ret_cmd(const config_state& state)
{
    const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
    actions* ptr = nullptr;

    if (g_options._force_unicode)
    {
        ptr = &g_curr_uparser->_actions[rule_idx];
    }
    else
    {
        ptr = &g_curr_parser->_actions[rule_idx];
    }

    ptr->_cmd_stack.pop_back();
}

void build_condition_parser()
{
    parsertl::rules grules;
    lexertl::rules lrules;

    grules.token("Index String");
    grules.push("start", "cmd | start '||' cmd");
    grules.push("cmd", "'regex_search' '(' Index ',' String ')'");
    parsertl::generator::build(grules, g_condition_parser._gsm);

    lrules.push(R"(\()", grules.token_id("'('"));
    lrules.push(R"(\))", grules.token_id("')'"));
    lrules.push(",", grules.token_id("','"));
    lrules.push(R"(\|\|)", grules.token_id("'||'"));
    lrules.push(R"($(0|[1-9]\d*))", grules.token_id("Index"));
    lrules.push("regex_search", grules.token_id("'regex_search'"));
    lrules.push("'([^']|'')*'", grules.token_id("String"));
    lrules.push(R"(\s+)", lexertl::rules::skip());
    lexertl::generator::build(lrules, g_condition_parser._lsm);
}

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
    g_config_parser._actions[grules.push("directive", "consume tokens NL")] =
        [](config_state& state, const config_parser&)
        {
            state._store_tokens = false;
        };
    g_config_parser._actions[grules.push("consume", "'%consume'")] =
        [](config_state& state, const config_parser&)
        {
            state._store_tokens = true;
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
    g_config_parser._actions[grules.push("tokens", "token")] =
        [](config_state& state, const config_parser& parser)
        {
            if (state._store_tokens)
            {
                const auto& token = state._results.dollar(0, parser._gsm,
                    state._productions);

                state._consume.push_back(token.str());
            }
        };
    g_config_parser._actions[grules.push("tokens", "tokens token")] =
        [](config_state& state, const config_parser& parser)
        {
            if (state._store_tokens)
            {
                const auto& token = state._results.dollar(1, parser._gsm,
                    state._productions);

                state._consume.push_back(token.str());
            }
        };
    grules.push("token", "Literal | Name");
    // Read and store %option caseless
    g_config_parser._actions[grules.push("directive",
        "'%option' 'caseless' NL")] =
        [](config_state& state, const config_parser&)
        {
            if (g_options._force_unicode)
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
                    if (g_options._force_unicode)
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
                if (g_options._force_unicode)
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
            auto iter = g_options._force_unicode ?
                g_curr_uparser->_actions.find(prod_index) :
                g_curr_parser->_actions.find(prod_index);

            if (iter != (g_options._force_unicode ? g_curr_uparser->_actions.end() :
                g_curr_parser->_actions.end()))
            {
                for (const auto& cmd : iter->second._commands)
                {
                    if (cmd->_param1 >=
                        state._grules.grammar().back()._rhs._symbols.size())
                    {
                        throw gg_error(std::format("Index ${} is out of range.",
                            cmd->_param1 + 1));
                    }

                    if (cmd->_param2 >=
                        state._grules.grammar().back()._rhs._symbols.size())
                    {
                        throw gg_error(std::format("Index ${} is out of range.",
                            cmd->_param2 + 1));
                    }

                    if (cmd->_param1 == cmd->_param2 &&
                        cmd->_second1 > cmd->_second2)
                    {
                        throw gg_error(std::format("Index ${} cannot have "
                            "first following second.",
                            cmd->_param2 + 1));
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
        "| '[' opt_list ']' "
        "| rhs '?' "
        "| rhs '*' "
        "| rhs '+' "
        "| '(' opt_list ')'");
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
                const uint16_t size = state._grules.grammar().size() & 0xffff;

                // No commands
                if (g_options._force_unicode)
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
            const uint16_t rule_idx = (state._grules.grammar().size()) & 0xffff;
            const auto& token = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token.first + 1) - 1) & 0xffff;
            auto command = std::make_shared<match_cmd>
                (cmd::type::assign, index);
            actions* ptr = nullptr;

            command->_front = atoi(token.first) & 0xffff;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(std::move(command));
        };
    g_config_parser._actions[grules.push("cmd",
        "'match' '=' 'substr' '(' Index ',' Integer ',' Integer ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = (state._grules.grammar().size()) & 0xffff;
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token4.first + 1) - 1) & 0xffff;
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const auto& token8 = state._results.dollar(8, parser._gsm,
                state._productions);
            auto command = std::make_shared<match_cmd>(cmd::type::assign, index);
            actions* ptr = nullptr;

            command->_front = atoi(token6.first) & 0xffff;
            command->_back = atoi(token8.first) & 0xffff;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(std::move(command));
        };
    g_config_parser._actions[grules.push("cmd", "'match' '+=' Index")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = (state._grules.grammar().size()) & 0xffff;
            const auto& token = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token.first + 1) - 1) & 0xffff;
            auto command = std::make_shared<match_cmd>(cmd::type::append, index);
            actions* ptr = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(std::move(command));
        };
    g_config_parser._actions[grules.push("cmd",
        "'match' '+=' 'substr' '(' Index ',' Integer ',' Integer ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token4.first + 1) - 1) & 0xffff;
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const auto& token8 = state._results.dollar(8, parser._gsm,
                state._productions);
            auto command = std::make_shared<match_cmd>(cmd::type::append, index);
            actions* ptr = nullptr;

            command->_front = atoi(token6.first) & 0xffff;
            command->_back = atoi(token8.first) & 0xffff;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(std::move(command));
        };
    g_config_parser._actions[grules.push("cmd",
        "print_kwd '(' ret_function ')'")] =
        [](config_state& state, const config_parser&)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            actions* ptr = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->_cmd_stack.pop_back();
            state._print = true;
        };
    g_config_parser._actions[grules.push("print_kwd", "'print'")] =
        [](config_state& state, const config_parser&)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            auto command = std::make_shared<print_cmd>();
            actions* ptr = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(command);
            ptr->_cmd_stack.push_back(command.get());
        };

    g_config_parser._actions[grules.push("single_cmd", "mod_cmd")] =
        [](config_state&, const config_parser&)
        {
            g_modify = true;
        };
    g_config_parser._actions[grules.push("mod_cmd", "'erase' '(' Index ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const uint16_t index = (atoi(token.first + 1) - 1) & 0xffff;
            auto command = std::make_shared<erase_cmd>(index);
            actions* ptr = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(std::move(command));
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'erase' '(' Index ',' Index ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const uint16_t index1 = (atoi(token2.first + 1) - 1) & 0xffff;
            const uint16_t index2 = (atoi(token4.first + 1) - 1) & 0xffff;
            auto command = std::make_shared<erase_cmd>(index1, index2);
            actions* ptr = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(std::move(command));
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "'erase' '(' Index '.' first_second ',' Index '.' first_second ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const bool second1 = *state._results.dollar(4, parser._gsm,
                state._productions).first == 's';
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const bool second2 = *state._results.dollar(8, parser._gsm,
                state._productions).first == 's';
            const uint16_t index1 = (atoi(token2.first + 1) - 1) & 0xffff;
            const uint16_t index2 = (atoi(token6.first + 1) - 1) & 0xffff;
            auto command = std::make_shared<erase_cmd>
                (index1, second1, index2, second2);
            actions* ptr = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->emplace(std::move(command));
        };

    g_config_parser._actions[grules.push("mod_cmd",
        "insert_kwd '(' Index ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token2.first + 1) - 1) & 0xffff;
            actions* ptr = nullptr;
            cmd* command = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            command = ptr->_commands.back();
            command->_param1 = index;
            // Pop copy of command
            ptr->_cmd_stack.pop_back();
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "insert_kwd '(' Index '.' 'second' ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token2.first + 1) - 1) & 0xffff;
            actions* ptr = nullptr;
            cmd* command = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            command = ptr->_commands.back();
            command->_param1 = index;
            command->_second1 = true;
            // Pop copy of command
            ptr->_cmd_stack.pop_back();
        };
    g_config_parser._actions[grules.push("insert_kwd", "'insert'")] =
        [](config_state& state, const config_parser&)
        {
            push_kwd<insert_cmd>(state);
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "replace_kwd '(' Index ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token2.first + 1) - 1) & 0xffff;
            actions* ptr = nullptr;
            cmd* command = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            command = ptr->_commands.back();
            command->_param1 = index;
            // Pop copy of command
            ptr->_cmd_stack.pop_back();
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "replace_kwd '(' Index ',' Index ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index1 = (atoi(token2.first + 1) - 1) & 0xffff;
            const auto& token4 = state._results.dollar(4, parser._gsm,
                state._productions);
            const uint16_t index2 = (atoi(token4.first + 1) - 1) & 0xffff;
            actions* ptr = nullptr;
            cmd* command = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            command = ptr->_commands.back();
            command->_param1 = index1;
            command->_param2 = index2;
            // Pop copy of command
            ptr->_cmd_stack.pop_back();
        };
    g_config_parser._actions[grules.push("mod_cmd",
        "replace_kwd '(' Index '.' first_second ',' "
        "Index '.' first_second ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index1 = (atoi(token2.first + 1) - 1) & 0xffff;
            const bool second1 = *state._results.dollar(4, parser._gsm,
                state._productions).first == 's';
            const auto& token6 = state._results.dollar(6, parser._gsm,
                state._productions);
            const uint16_t index2 = (atoi(token6.first + 1) - 1) & 0xffff;
            const bool second2 = *state._results.dollar(8, parser._gsm,
                state._productions).first == 's';
            actions* ptr = nullptr;
            cmd* command = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            command = ptr->_commands.back();
            command->_param1 = index1;
            command->_second1 = second1;
            command->_param2 = index2;
            command->_second2 = second2;
            // Pop copy of command
            ptr->_cmd_stack.pop_back();
        };
    g_config_parser._actions[grules.push("replace_kwd", "'replace'")] =
        [](config_state& state, const config_parser&)
        {
            push_kwd<replace_cmd>(state);
        };

    grules.push("first_second", "'first' | 'second'");
    g_config_parser._actions[grules.push("mod_cmd",
        "replace_all_inplace_kwd '(' Index ',' ret_function ',' ret_function ')'")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& token2 = state._results.dollar(2, parser._gsm,
                state._productions);
            const uint16_t index = (atoi(token2.first + 1) - 1) & 0xffff;
            actions* ptr = nullptr;
            cmd* command = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            command = ptr->_commands.back();
            command->_param1 = index;
            // Pop copy of command
            ptr->_cmd_stack.pop_back();
        };
    g_config_parser._actions[grules.push("replace_all_inplace_kwd", "'replace_all'")] =
        [](config_state& state, const config_parser&)
        {
            push_kwd<replace_all_inplace_cmd>(state);
        };

    g_config_parser._actions[grules.push("ret_function", "ScriptString")] =
        [](config_state& state, const config_parser& parser)
        {
            const uint16_t rule_idx = state._grules.grammar().size() & 0xffff;
            const auto& text = state._results.dollar(0, parser._gsm,
                state._productions);
            actions* ptr = nullptr;

            if (g_options._force_unicode)
            {
                ptr = &g_curr_uparser->_actions[rule_idx];
            }
            else
            {
                ptr = &g_curr_parser->_actions[rule_idx];
            }

            ptr->_storage.push_back(std::make_shared<string_cmd>
                (state._print ?
                    unescape_str(text.substr(1, 1)) :
                    dedup_apostrophes(text.substr(1, 1))));
            ptr->_cmd_stack.back()->push(ptr->_storage.back().get());
        };
    grules.push("ret_function", "perform_system "
        "| perform_format "
        "| perform_replace_all");
    g_config_parser._actions[grules.push("perform_system",
        "system_kwd '(' ret_function ')'")] =
        [](config_state& state, const config_parser&)
        {
            pop_ret_cmd(state);
        };
    g_config_parser._actions[grules.push("system_kwd", "'system'")] =
        [](config_state& state, const config_parser&)
        {
            push_ret_kwd<system_cmd>(state);
        };
    g_config_parser._actions[grules.push("perform_format",
        "format_kwd '(' ret_function format_params ')'")] =
        [](config_state& state, const config_parser&)
        {
            pop_ret_cmd(state);
        };
    g_config_parser._actions[grules.push("format_kwd", "'format'")] =
        [](config_state& state, const config_parser&)
        {
            push_ret_kwd<format_cmd>(state);
        };
    g_config_parser._actions[grules.push("perform_replace_all",
        "replace_all_kwd '(' ret_function ',' ret_function ',' ret_function ')'")] =
        [](config_state& state, const config_parser&)
        {
            pop_ret_cmd(state);
        };
    g_config_parser._actions[grules.push("replace_all_kwd", "'replace_all'")] =
        [](config_state& state, const config_parser&)
        {
            push_ret_kwd<replace_all_cmd>(state);
        };

    grules.push("format_params", "%empty | format_params ',' ret_function");

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

            if (g_options._force_unicode)
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

            if (g_options._force_unicode)
                state._lurules.push(regex, atoi(number.c_str()) & 0xffff);
            else
                state._lrules.push(regex, atoi(number.c_str()) & 0xffff);
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

            if (g_options._force_unicode)
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

            if (g_options._force_unicode)
                state._lurules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, atoi(number.c_str()) & 0xffff,
                    std::string(exit_state.first + 1,
                        exit_state.second - 1).c_str());
            else
                state._lrules.push(std::string(start_state.first + 1,
                    start_state.second - 1).c_str(),
                    regex, atoi(number.c_str()) & 0xffff,
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

            if (g_options._force_unicode)
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

            if (g_options._force_unicode)
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

            if (g_options._force_unicode)
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

            if (g_options._force_unicode)
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

            if (g_options._force_unicode)
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

            if (g_options._force_unicode)
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
    {
        output_text_nl(std::cerr, is_a_tty(stderr),
            g_options._ms_text.c_str(),
            std::format("{}Config parser warnings: {}",
                gg_text(),
                warnings));
    }

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
    lrules.push("%consume", grules.token_id("'%consume'"));
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
    lrules.push("SCRIPT", "first", grules.token_id("'first'"), ".");
    lrules.push("SCRIPT", "format", grules.token_id("'format'"), ".");
    lrules.push("SCRIPT", "insert", grules.token_id("'insert'"), ".");
    lrules.push("SCRIPT", "match", grules.token_id("'match'"), ".");
    lrules.push("SCRIPT", "print", grules.token_id("'print'"), ".");
    lrules.push("SCRIPT", "replace", grules.token_id("'replace'"), ".");
    lrules.push("SCRIPT", "replace_all", grules.token_id("'replace_all'"), ".");
    lrules.push("SCRIPT", "second", grules.token_id("'second'"), ".");
    lrules.push("SCRIPT", "substr", grules.token_id("'substr'"), ".");
    lrules.push("SCRIPT", "system", grules.token_id("'system'"), ".");
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
