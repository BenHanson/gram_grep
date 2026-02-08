#pragma once

#include "types.hpp"

#include <lexertl/state_machine.hpp>
#include <parsertl/rules.hpp>
#include <parsertl/state_machine.hpp>

#include <cstdint>
#include <tuple>

void build_condition_parser();
void build_config_parser();
void build_ret_parser();
std::pair<parsertl::state_machine, lexertl::state_machine> param_parser();

template<typename PARSER>
void push_ret_functions(parsertl::rules& grules, PARSER& parser)
{
    grules.push("ret_function", "perform_capitalise "
        "| perform_format "
        "| perform_replace_all "
        "| perform_system "
        "| perform_tolower "
        "| perform_toupper");
    parser._actions[grules.push("perform_capitalise",
        "capitalise_kwd '(' ret_function ')'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            pop_ret_cmd(state);
        };
    parser._actions[grules.push("capitalise_kwd", "'capitalise'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            push_ret_kwd<capitalise_cmd>(state);
        };
    parser._actions[grules.push("perform_format",
        "format_kwd '(' ret_function format_params ')'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            pop_ret_cmd(state);
        };
    parser._actions[grules.push("format_kwd", "'format'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            push_ret_kwd<format_cmd>(state);
        };
    grules.push("format_params", "%empty | format_params ',' ret_function");
    parser._actions[grules.push("perform_replace_all",
        "replace_all_kwd '(' ret_function ',' ret_function ',' ret_function ')'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            pop_ret_cmd(state);
        };
    parser._actions[grules.push("replace_all_kwd", "'replace_all'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            push_ret_kwd<replace_all_cmd>(state);
        };
    parser._actions[grules.push("perform_system",
        "system_kwd '(' ret_function ')'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            pop_ret_cmd(state);
        };
    parser._actions[grules.push("system_kwd", "'system'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            push_ret_kwd<system_cmd>(state);
        };
    parser._actions[grules.push("perform_tolower",
        "tolower_kwd '(' ret_function ')'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            pop_ret_cmd(state);
        };
    parser._actions[grules.push("tolower_kwd", "'tolower'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            push_ret_kwd<tolower_cmd>(state);
        };
    parser._actions[grules.push("perform_toupper",
        "toupper_kwd '(' ret_function ')'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            pop_ret_cmd(state);
        };
    parser._actions[grules.push("toupper_kwd", "'toupper'")] =
        [](typename PARSER::state& state, const typename PARSER::parser&)
        {
            push_ret_kwd<toupper_cmd>(state);
        };
}
