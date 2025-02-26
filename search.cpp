#include "pch.h"

#include "gg_error.hpp"
#include "search.hpp"
#include "types.hpp"

#include <lexertl/iterator.hpp>
#include <boost/regex.hpp>
#include <parsertl/search.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/state_machine.hpp>
#include <parsertl/token.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <stack>
#include <stdlib.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

extern boost::regex g_capture_rx;
extern options g_options;
extern pipeline g_pipeline;

extern lexertl::state_machine word_lexer();
extern std::string unescape(const std::string_view& vw);

using results = std::vector<std::vector<std::pair
    <const char*, const char*>>>;
using prod_map_t = std::vector<std::pair<uint16_t, token::token_vector>>;
using uresults = std::vector<std::vector<std::pair
    <utf8_in_iterator, utf8_in_iterator>>>;
using uprod_map_t = std::vector<std::pair<uint16_t,
    parsertl::token<crutf8iterator>::token_vector>>;

static const char* get_ptr(const char* ptr)
{
    return ptr;
}

static const char* get_ptr(const utf8_in_iterator& iter)
{
    return iter.get();
}

static const char* get_first(const std::pair<const char*, const char*>& pair)
{
    return pair.first;
}

static const char* get_second(const std::pair<const char*, const char*>& pair)
{
    return pair.second;
}

static const char* get_first(const std::pair<utf8_in_iterator,
    utf8_in_iterator>& pair)
{
    return pair.first.get();
}

static const char* get_second(const std::pair<utf8_in_iterator,
    utf8_in_iterator>& pair)
{
    return pair.second.get();
}

static const char* get_first(const lexertl::criterator& iter)
{
    return iter->first;
}

static const char* get_second(const lexertl::criterator& iter)
{
    return iter->second;
}

static const char* get_first(const crutf8iterator& iter)
{
    return iter->first.get();
}

static const char* get_second(const crutf8iterator& iter)
{
    return iter->second.get();
}

static std::size_t production_size(const parsertl::state_machine& sm,
    const std::size_t index_)
{
    return sm._rules[index_]._rhs.size();
}

template<typename token_vector>
const auto& dollar(const uint16_t rule_, const std::size_t index_,
    const parsertl::state_machine& sm_,
    const token_vector& productions)
{
    return productions[productions.size() -
        production_size(sm_, rule_) + index_];
}

static std::string format_item(const std::string& input,
    const std::pair<uint16_t, token::token_vector>& item)
{
    std::string output;
    const char* last = input.c_str();
    boost::cregex_iterator iter(last, last + input.size(),
        g_capture_rx);
    boost::cregex_iterator end;

    for (; iter != end; ++iter)
    {
        const std::size_t idx =
            static_cast<std::size_t>(atoi((*iter)[0].first + 1)) - 1;

        output.append(last, (*iter)[0].first);
        output += idx >= item.second.size() ?
            std::string() :
            item.second[idx].str();
        last = (*iter)[0].second;
    }

    output.append(last, input.c_str() + input.size());
    return unescape(output);
}

static std::string format_item(const std::string& input,
    const std::pair<uint16_t,
    parsertl::token<crutf8iterator>::token_vector>& item)
{
    std::string output;
    const char* last = input.c_str();
    boost::cregex_iterator iter(last, last + input.size(),
        g_capture_rx);
    boost::cregex_iterator end;

    for (; iter != end; ++iter)
    {
        const std::size_t idx =
            static_cast<std::size_t>(atoi((*iter)[0].first + 1)) - 1;

        output.append(last, (*iter)[0].first);
        output += idx >= item.second.size() ?
            std::string() :
            std::string(item.second[idx].first.get(),
                item.second[idx].second.get());
        last = (*iter)[0].second;
    }

    output.append(last, input.c_str() + input.size());
    return unescape(output);
}

template<typename T>
bool conditions_met(const condition_map& conditions, const T& cap_vec)
{
    bool success = conditions.empty();

    for (const auto& [idx, rx] : conditions)
    {
        if (idx >= cap_vec.size())
            throw gg_error(std::format("${} is out of range", idx));

        for (const auto& cap : cap_vec[idx])
        {
            if (boost::regex_search(get_first(cap), get_second(cap), rx))
            {
                success = true;
                break;
            }
        }

        if (success)
            break;
    }

    return success;
}

template<typename parser_t, typename token_vector>
void process_action(const parser_t& p, const char* start,
    const std::map<uint16_t, actions>::iterator& action_iter,
    const std::pair<uint16_t, token_vector>& item,
    std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements)
{
    for (const auto cmd : action_iter->second._commands)
    {
        const token_vector& productions = item.second;

        switch (cmd->_type)
        {
        case cmd::type::append:
        {
            const auto c = static_cast<match_cmd*>(cmd);
            const auto &token = dollar(item.first,
                cmd->_param1, p._gsm, productions);
            const std::string_view temp(get_ptr(token.first),
                get_ptr(token.second));
            const uint16_t size = c->_front + c->_back;

            if (c->_front == 0 && c->_back == 0)
                matches.top() += temp;
            else
            {
                if (c->_front >= temp.size() || size > temp.size())
                {
                    throw gg_error(std::format("substr(${}, {}, {}) out of "
                        "range for string '{}'.",
                        cmd->_param1 + 1,
                        c->_front,
                        c->_back,
                        temp));
                }

                matches.top() += temp.substr(c->_front, temp.size() - size);
            }

            break;
        }
        case cmd::type::assign:
        {
            const auto& c = static_cast<match_cmd*>(cmd);
            const auto& token = dollar(item.first, cmd->_param1, p._gsm,
                productions);
            std::string temp(get_ptr(token.first), get_ptr(token.second));

            if (c->_front == 0 && c->_back == 0)
                matches.top() = std::move(temp);
            else
            {
                const uint16_t size = c->_front + c->_back;

                if (c->_front >= temp.size() || size > temp.size())
                {
                    throw gg_error(std::format("substr(${}, {}, {}) out of "
                        "range for string '{}'.",
                        cmd->_param1 + 1,
                        c->_front,
                        c->_back,
                        temp));
                }

                matches.top() = temp.substr(c->_front, temp.size() - size);
            }

            break;
        }
        case cmd::type::erase:
            if (g_options._perform_output)
            {
                const auto& param1 = dollar(item.first, cmd->_param1, p._gsm,
                    productions);
                const auto& param2 = dollar(item.first, cmd->_param2, p._gsm,
                    productions);
                const auto index1 = (cmd->_second1 ?
                    get_ptr(param1.second) :
                    get_ptr(param1.first)) - start;
                const auto index2 = (cmd->_second2 ?
                    get_ptr(param2.second) :
                    get_ptr(param2.first)) - start;

                replacements[std::pair(index1, index2 - index1)] =
                    std::string();
            }

            break;
        case cmd::type::insert:
            if (g_options._perform_output)
            {
                const auto c = static_cast<insert_cmd*>(cmd);
                const auto& param = dollar(item.first, cmd->_param1, p._gsm,
                    productions);
                const auto index = (cmd->_second1 ?
                    get_ptr(param.second) :
                    get_ptr(param.first)) - start;

                replacements[std::pair(index, 0)] =
                    action_iter->second.exec(c->_param);
            }

            break;
        case cmd::type::print:
            std::cout << format_item(action_iter->second.exec(cmd), item);
            break;
        case cmd::type::replace:
            if (g_options._perform_output)
            {
                const auto c = static_cast<replace_cmd*>(cmd);
                const auto size = productions.size() -
                    production_size(p._gsm, item.first);
                const auto& param1 = productions[size + c->_param1];
                const auto& param2 = productions[size + c->_param2];
                const auto index1 =
                    (c->_second1 ?
                        get_ptr(param1.second) :
                        get_ptr(param1.first)) - start;
                const auto index2 =
                    (c->_second2 ?
                        get_ptr(param2.second) :
                        get_ptr(param2.first)) - start;

                replacements[std::pair(index1, index2 - index1)] =
                    action_iter->second.exec(c->_param);
            }

            break;
        case cmd::type::replace_all_inplace:
            if (g_options._perform_output)
            {
                const auto c = static_cast<replace_all_inplace_cmd*>(cmd);
                const auto size = productions.size() -
                    production_size(p._gsm, item.first);
                const auto& param = productions[size + c->_param1];
                const auto index1 = get_ptr(param.first) - start;
                const auto index2 = get_ptr(param.second) - start;
                auto pair = std::pair(index1, index2 - index1);
                auto iter = replacements.find(pair);
                const boost::regex rx(action_iter->second.exec(c->_params[0]));
                const std::string text =
                    boost::regex_replace(iter == replacements.end() ?
                        std::string(get_ptr(param.first), get_ptr(param.second)) :
                        iter->second,
                        rx, action_iter->second.exec(c->_params[1]));

                replacements[pair] = text;
            }

            break;
        default:
            break;
        }
    }
}

std::string build_text(const std::string& input, const capture_vector& captures)
{
    std::string output;
    const char* last = input.c_str();
    boost::cregex_iterator iter(input.c_str(),
        input.c_str() + input.size(), g_capture_rx);
    boost::cregex_iterator end;

    for (; iter != end; ++iter)
    {
        const auto idx = static_cast<std::size_t>(atoi((*iter)[0].first + 1));

        output.append(last, (*iter)[0].first);
        output += idx >= captures.size() ?
            std::string_view() :
            captures[idx].front();
        last = (*iter)[0].second;
    }

    output.append(last, input.c_str() + input.size());
    return output;
}

static bool is_word_char(const char c)
{
    return (c >= 'A' && c <= 'Z') ||
        c == '_' ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9');
}

static bool is_whole_word(const char* data_first, const char* first,
    const char* second, const char* eoi, const unsigned int flags)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    const char* prev_first = first == data_first ? nullptr : first - 1;
    const char* prev_second = second - 1;

    return !(flags & *config_flags::whole_word) ||
        ((first == data_first ||
            (is_word_char(*prev_first) && !is_word_char(*first)) ||
            (!is_word_char(*prev_first) && is_word_char(*first))) &&
        (second == eoi ||
            (is_word_char(*prev_second) && !is_word_char(*second)) ||
            (!is_word_char(*prev_second) && is_word_char(*second))));
}

static bool is_bol_eol(const char* data_first, const char* first,
    const char* second, const char* eoi, const unsigned int flags)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    const char* prev_first = first == data_first ? nullptr : first - 1;

    return !(flags & *config_flags::bol_eol) ||
        ((first == data_first || *prev_first == '\n') &&
            (second == eoi || *second == '\r' || *second == '\n'));

}

static bool process_text(const text& t, const char* data_first,
    std::vector<match>& ranges, capture_vector& captures)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    const std::string text = build_text(t._text, captures);
    const char* first = ranges.back()._first;
    const char* second = ranges.back()._eoi;
    results cap_vec;
    bool success = false;

    cap_vec.emplace_back();
    cap_vec.back().emplace_back();

    do
    {
        first = text.empty() ? ranges.back()._eoi :
            (t._flags & *config_flags::icase) ?
            std::search(first, second, &text.front(),
                &text.front() + text.size(),
                [](const char lhs, const char rhs)
                {
                    return ::tolower(lhs) == ::tolower(rhs);
                })
            : std::search(first, second, &text.front(),
                &text.front() + text.size());
        second = first + text.size();
        success = first != ranges.back()._eoi;

        if (!success)
            break;

        cap_vec.back().back().first = first;
        cap_vec.back().back().second = second;
        success = is_whole_word(data_first, first, second,
            ranges.front()._eoi, t._flags) &&
            is_bol_eol(data_first, first, second, ranges.front()._eoi,
                t._flags) &&
            conditions_met(t._conditions, cap_vec);

        if (!success)
        {
            ++first;
            second = ranges.back()._eoi;
        }
    } while (!success);

    if (success)
    {
        if (t._flags & *config_flags::negate)
        {
            if (t._flags & *config_flags::all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                if (!(t._flags & *config_flags::ret_prev_match))
                    ranges.back()._second = first + text.size();

                if (last_start == first)
                {
                    if (!(t._flags & *config_flags::ret_prev_match))
                        // The match is right at the beginning, so skip.
                        ranges.emplace_back(second, second, second);

                    success = false;
                }
                else if (!(t._flags & *config_flags::ret_prev_match))
                    ranges.emplace_back(ranges.back()._first, first, first);
            }
        }
        else if (!(t._flags & *config_flags::ret_prev_match))
        {
            // Store start of match
            ranges.back()._first = first;
            // Store end of match
            ranges.back()._second = second;
            ranges.emplace_back((t._flags & *config_flags::extend_search) ?
                second :
                first,
                (t._flags & *config_flags::extend_search) ?
                second :
                first,
                (t._flags & *config_flags::extend_search) ?
                ranges.back()._eoi :
                second);
        }
    }
    else if (t._flags & *config_flags::negate &&
        ranges.back()._first != ranges.back()._eoi)
    {
        if (!(t._flags & *config_flags::ret_prev_match))
        {
            ranges.back()._second = first;
            ranges.emplace_back(ranges.back()._first, first, first);
        }

        success = true;
    }

    if (success && !(t._flags & *config_flags::ret_prev_match))
    {
        captures.clear();
        captures.emplace_back();
        captures.back().emplace_back(ranges.back()._first, ranges.back()._eoi);
    }

    return success;
}

static bool process_regex(const regex& r, const char* data_first,
    std::vector<match>& ranges, capture_vector& captures)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    boost::cregex_iterator iter(ranges.back()._first,
        ranges.back()._eoi, r._rx);
    boost::cregex_iterator end;
    results cap_vec;
    bool success = iter != end;

    cap_vec.emplace_back();
    cap_vec.back().emplace_back(iter == end ?
        boost::csub_match{} :
        (*iter)[0]);

    while (success && (!is_whole_word(data_first,
        (*iter)[0].first, (*iter)[0].second, ranges.front()._eoi, r._flags) ||
        !is_bol_eol(data_first, (*iter)[0].first, (*iter)[0].second,
            ranges.front()._eoi, r._flags) ||
        !conditions_met(r._conditions, cap_vec)))
    {
        iter = boost::cregex_iterator((*iter)[0].second, ranges.back()._eoi, r._rx);
        success = iter != end;

        if (!success)
            break;

        cap_vec.back().back() = (*iter)[0];
    }

    if (success)
    {
        if (r._flags & *config_flags::negate)
        {
            if (r._flags & *config_flags::all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                if (!(r._flags & *config_flags::ret_prev_match))
                    ranges.back()._second = (*iter)[0].second;

                if (last_start == (*iter)[0].first)
                {
                    if (!(r._flags & *config_flags::ret_prev_match))
                        // The match is right at the beginning, so skip.
                        ranges.emplace_back((*iter)[0].second, (*iter)[0].second,
                            (*iter)[0].second);

                    success = false;
                }
                else if(!(r._flags & *config_flags::ret_prev_match))
                    ranges.emplace_back(ranges.back()._first, (*iter)[0].first,
                        (*iter)[0].first);
            }
        }
        else if(!(r._flags & *config_flags::ret_prev_match))
        {
            // Store start of match
            ranges.back()._first = (*iter)[0].first;
            // Store end of match
            ranges.back()._second = (*iter)[0].second;
            ranges.emplace_back((r._flags & *config_flags::extend_search) ?
                (*iter)[0].second :
                (*iter)[0].first,
                (r._flags & *config_flags::extend_search) ?
                (*iter)[0].second :
                (*iter)[0].first,
                (r._flags & *config_flags::extend_search) ?
                ranges.back()._eoi :
                (*iter)[0].second);
        }
    }
    else if (r._flags & *config_flags::negate &&
        ranges.back()._first != ranges.back()._eoi)
    {
        if (!(r._flags & *config_flags::ret_prev_match))
        {
            ranges.back()._second = ranges.back()._eoi;
            ranges.emplace_back(ranges.back()._first, ranges.back()._eoi,
                ranges.back()._eoi);
        }

        success = true;
    }

    if (success && !(r._flags & *config_flags::ret_prev_match))
    {
        captures.clear();

        if (r._flags & *config_flags::negate)
        {
            captures.emplace_back();
            captures.back().emplace_back(ranges.back()._first,
                ranges.back()._second);
        }
        else
        {
            for (const auto& m : *iter)
            {
                captures.emplace_back();
                captures.back().emplace_back(m.first, m.second);
            }
        }
    }

    return success;
}

static std::pair<bool, lexertl::criterator> lexer_search(const lexer& l,
    const char* data_first, std::vector<match>& ranges)
{
    lexertl::criterator iter(ranges.back()._first,
        ranges.back()._eoi, l._sm);
    results cap_vec;
    bool success = iter->first != ranges.back()._eoi;

    cap_vec.emplace_back();
    cap_vec.back().emplace_back(iter->first, iter->second);

    while (success && (!is_whole_word(data_first,
        iter->first, iter->second, ranges.front()._eoi, l._flags) ||
        !is_bol_eol(data_first, iter->first, iter->second, ranges.front()._eoi,
            l._flags) ||
        !conditions_met(l._conditions, cap_vec)))
    {
        iter = lexertl::criterator(iter->second, iter->eoi, l._sm);
        success = iter->first != ranges.back()._eoi;

        if (!success)
            break;

        cap_vec.back().back().first = iter->first;
        cap_vec.back().back().second = iter->second;
    }

    return std::make_pair(success, std::move(iter));
}

static std::pair<bool, crutf8iterator> lexer_search(const ulexer& l,
    const char* data_first, std::vector<match>& ranges)
{
    crutf8iterator iter(utf8_in_iterator(ranges.back()._first, ranges.back()._eoi),
        utf8_in_iterator(ranges.back()._eoi, ranges.back()._eoi), l._sm);
    results cap_vec;
    bool success =
        iter->first != utf8_in_iterator(ranges.back()._eoi, ranges.back()._eoi);

    cap_vec.emplace_back();
    cap_vec.back().emplace_back(iter->first.get(), iter->second.get());

    while (success && (!is_whole_word(data_first, iter->first.get(),
        iter->second.get(), ranges.front()._eoi, l._flags) ||
        !is_bol_eol(data_first, iter->first.get(), iter->second.get(),
            ranges.front()._eoi, l._flags) ||
        !conditions_met(l._conditions, cap_vec)))
    {
        iter = crutf8iterator(utf8_in_iterator(iter->second.get(), iter->eoi.get()),
            utf8_in_iterator(iter->eoi.get(), iter->eoi.get()), l._sm);
        success = iter->first != utf8_in_iterator(ranges.back()._eoi,
            ranges.back()._eoi);

        if (!success)
            break;

        cap_vec.back().back().first = iter->first.get();
        cap_vec.back().back().second = iter->second.get();
    }

    return std::make_pair(success, std::move(iter));
}

template<typename lexer_t>
bool process_lexer(const lexer_t& l, const char* data_first,
    std::vector<match>& ranges, capture_vector& captures)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    auto [success, iter] = lexer_search(l, data_first, ranges);

    if (success)
    {
        if (l._flags & *config_flags::negate)
        {
            if (l._flags & *config_flags::all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                if (!(l._flags & *config_flags::ret_prev_match))
                    ranges.back()._second = get_second(iter);

                if (last_start == get_first(iter))
                {
                    if (!(l._flags & *config_flags::ret_prev_match))
                        // The match is right at the beginning, so skip.
                        ranges.emplace_back(get_second(iter),
                            get_second(iter), get_second(iter));

                    success = false;
                }
                else if (!(l._flags & *config_flags::ret_prev_match))
                    ranges.emplace_back(ranges.back()._first,
                        get_first(iter), get_first(iter));
            }
        }
        else if (!(l._flags & *config_flags::ret_prev_match))
        {
            // Store start of match
            ranges.back()._first = get_first(iter);
            // Store end of match
            ranges.back()._second = get_second(iter);
            ranges.emplace_back((l._flags &
                *config_flags::extend_search) ?
                get_second(iter) :
                get_first(iter),
                (l._flags & *config_flags::extend_search) ?
                get_second(iter) :
                get_first(iter),
                (l._flags & *config_flags::extend_search) ?
                ranges.back()._eoi :
                get_second(iter));
        }
    }
    else if (l._flags & *config_flags::negate &&
        ranges.back()._first != ranges.back()._eoi)
    {
        if (!(l._flags & *config_flags::ret_prev_match))
        {
            ranges.back()._second = get_first(iter);
            ranges.emplace_back(ranges.back()._first,
                get_first(iter), get_first(iter));
        }

        success = true;
    }

    if (success && !(l._flags & *config_flags::ret_prev_match))
    {
        captures.clear();
        captures.emplace_back();
        captures.back().emplace_back(ranges.back()._first, ranges.back()._eoi);
    }

    return success;
}

static std::tuple<lexertl::criterator, lexertl::criterator, prod_map_t, results>
get_iterators(const parser& p, const std::vector<match>& ranges)
{
    lexertl::criterator iter(ranges.back()._first,
        ranges.back()._eoi, p._lsm);

    return std::make_tuple(std::move(iter), lexertl::criterator(),
        prod_map_t(), results());
}

static std::tuple<crutf8iterator, crutf8iterator, uprod_map_t, uresults>
get_iterators(const uparser& p, const std::vector<match>& ranges)
{
    crutf8iterator iter(utf8_in_iterator(ranges.back()._first, ranges.back()._eoi),
        utf8_in_iterator(ranges.back()._eoi, ranges.back()._eoi), p._lsm);

    return std::make_tuple(std::move(iter), crutf8iterator(),
        uprod_map_t(), uresults());
}

template<typename parser_t>
bool process_parser(parser_t& p, const char* data_first,
    std::vector<match>& ranges, std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements,
    capture_vector& captures)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    auto [iter, end, prod_map, cap_vec] = get_iterators(p, ranges);
    const bool has_captures = !p._gsm._captures.empty();
    bool success = false;

    do
    {
        if (has_captures)
            success = parsertl::search(iter, end, p._gsm, cap_vec);
        else
            success = parsertl::search(iter, end, p._gsm, &prod_map);

        if (!success)
            break;

        success = is_whole_word(data_first, get_first(iter), get_first(end),
            ranges.front()._eoi, p._flags) &&
            is_bol_eol(data_first, get_first(iter), get_first(end),
                ranges.front()._eoi, p._flags) &&
            conditions_met(p._conditions, cap_vec);

        if (!success)
            iter = end;
    } while (!success);

    if (success)
    {
        if (p._flags & *config_flags::negate)
        {
            if (p._flags & *config_flags::all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                if (!(p._flags & *config_flags::ret_prev_match))
                    ranges.back()._second = get_first(end);

                if (last_start == get_first(iter))
                {
                    if (!(p._flags & *config_flags::ret_prev_match))
                        // The match is right at the beginning, so skip.
                        ranges.emplace_back(get_second(iter),
                            get_second(iter), get_second(iter));

                    success = false;
                }
                else if (!(p._flags & *config_flags::ret_prev_match))
                    ranges.emplace_back(ranges.back()._first,
                        get_first(iter), get_first(iter));
            }
        }
        else
        {
            if (!p._actions.empty())
                matches.emplace();

            if (!(p._flags & *config_flags::ret_prev_match))
            {
                // Store start of match
                ranges.back()._first = get_first(iter);
                // Store end of match
                ranges.back()._second = get_first(end);
                ranges.emplace_back((p._flags &
                    *config_flags::extend_search) ?
                    get_first(end) :
                    get_first(iter),
                    (p._flags & *config_flags::extend_search) ?
                    get_first(end) :
                    get_first(iter),
                    (p._flags & *config_flags::extend_search) ?
                    ranges.back()._eoi :
                    get_first(end));
            }

            if (!p._reduce_set.empty())
            {
                // Success only if a _reduce_set item is found
                success = false;
            }

            for (const auto& item : prod_map)
            {
                auto action_iter = p._actions.find(item.first);

                if (action_iter != p._actions.end())
                {
                    process_action(p, data_first, action_iter, item, matches,
                        replacements);

                    if (!(p._flags & *config_flags::ret_prev_match))
                    {
                        ranges.back()._first = ranges.back()._second =
                            matches.top().c_str();
                        ranges.back()._eoi = matches.top().c_str() +
                            matches.top().size();
                    }
                }
                else
                {
                    if (p._reduce_set.empty() ||
                        p._reduce_set.contains(item.first))
                    {
                        success = true;

                        if (!matches.empty())
                        {
                            if (!(p._flags & *config_flags::ret_prev_match))
                            {
                                ranges.back()._first = ranges.back()._second =
                                    matches.top().c_str();
                                ranges.back()._eoi = matches.top().c_str() +
                                    matches.top().size();
                            }
                        }
                    }
                }
            }
        }
    }
    else if ((p._flags & *config_flags::negate) &&
        ranges.back()._first != ranges.back()._eoi)
    {
        if (!(p._flags & *config_flags::ret_prev_match))
        {
            ranges.back()._second = get_first(iter);
            ranges.emplace_back(ranges.back()._first,
                get_first(iter), get_first(iter));
        }

        success = true;
    }

    if (success && !(p._flags & *config_flags::ret_prev_match))
    {
        captures.clear();

        if (p._flags & *config_flags::negate)
        {
            captures.emplace_back();
            captures.back().emplace_back(ranges.back()._first,
                ranges.back()._second);
        }
        else
        {
            for (const auto& v : cap_vec)
            {
                captures.emplace_back();

                if (v.empty())
                {
                    captures.back().emplace_back();
                }
                else
                {
                    for (const auto& cap : v)
                    {
                        captures.back().emplace_back(get_first(cap),
                            get_second(cap));
                    }
                }
            }
        }
    }

    return success;
}

static bool process_word_list(const word_list& w, const char* data_first,
    std::vector<match>& ranges, capture_vector& captures)
{
    // Use the lexertl enum operator
    using namespace lexertl;
    std::string_view text;
    const lexertl::state_machine sm = word_lexer();
    lexertl::citerator iter(ranges.back()._first, ranges.back()._eoi, sm);
    const char* first = ranges.back()._first;
    const char* second = ranges.back()._eoi;
    results cap_vec;
    bool success = false;

    cap_vec.emplace_back();
    cap_vec.back().emplace_back();

    for (; iter->id != 0; ++iter)
    {
        text = iter->view();

        if (w._flags & *config_flags::icase)
        {
            auto list_iter = std::find_if(w._list.begin(), w._list.end(),
                [&text](const std::string_view& rhs)
                {
                    if (text.size() != rhs.size())
                        return false;
                    else
                    {
                        for (std::size_t idx = 0, size = text.size(); idx < size; ++idx)
                        {
                            if (::tolower(text[idx]) != ::tolower(rhs[idx]))
                                return false;
                        }

                        return true;
                    }
                });

            success = list_iter != w._list.end();
        }
        else
        {
            auto list_iter = std::ranges::lower_bound(w._list, text);

            success = list_iter != w._list.end() && *list_iter == text;
        }

        first = iter->first;
        second = iter->second;

        if (success)
        {
            cap_vec.back().back().first = first;
            cap_vec.back().back().second = second;
            success = is_whole_word(data_first, first, second,
                ranges.front()._eoi, w._flags) &&
                is_bol_eol(data_first, first, second, ranges.front()._eoi,
                    w._flags) &&
                conditions_met(w._conditions, cap_vec);

            if (success)
                break;
        }
    }

    if (!success)
    {
        first = second = ranges.back()._eoi;
    }

    if (success)
    {
        if (w._flags & *config_flags::negate)
        {
            if (w._flags & *config_flags::all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                if (!(w._flags & *config_flags::ret_prev_match))
                    ranges.back()._second = first + text.size();

                if (last_start == first)
                {
                    if (!(w._flags & *config_flags::ret_prev_match))
                        // The match is right at the beginning, so skip.
                        ranges.emplace_back(second, second, second);

                    success = false;
                }
                else if (!(w._flags & *config_flags::ret_prev_match))
                    ranges.emplace_back(ranges.back()._first, first, first);
            }
        }
        else if (!(w._flags & *config_flags::ret_prev_match))
        {
            // Store start of match
            ranges.back()._first = first;
            // Store end of match
            ranges.back()._second = second;
            ranges.emplace_back((w._flags & *config_flags::extend_search) ?
                second :
                first,
                (w._flags & *config_flags::extend_search) ?
                second :
                first,
                (w._flags & *config_flags::extend_search) ?
                ranges.back()._eoi :
                second);
        }
    }
    else if (w._flags & *config_flags::negate &&
        ranges.back()._first != ranges.back()._eoi)
    {
        if (!(w._flags & *config_flags::ret_prev_match))
        {
            ranges.back()._second = first;
            ranges.emplace_back(ranges.back()._first, first, first);
        }

        success = true;
    }

    if (success && !(w._flags & *config_flags::ret_prev_match))
    {
        captures.clear();
        captures.emplace_back();
        captures.back().emplace_back(ranges.back()._first, ranges.back()._eoi);
    }

    return success;
}

bool search(match_data& data,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements)
{
    bool success = false;

    data._negate = false;

    for (std::size_t index = data._ranges.size() - 1, size = g_pipeline.size();
        index < size; ++index)
    {
        // Use the lexertl enum operator
        using namespace lexertl;

        switch (auto& v = g_pipeline[index]; static_cast<match_type>(v.index()))
        {
        case match_type::text:
        {
            const auto& t = std::get<text>(v);

            success = process_text(t, data._first, data._ranges, data._captures);
            data._negate = (t._flags & *config_flags::negate) != 0;
            break;
        }
        case match_type::regex:
        {
            const auto& r = std::get<regex>(v);

            success = process_regex(r, data._first, data._ranges, data._captures);
            data._negate = (r._flags & *config_flags::negate) != 0;
            break;
        }
        case match_type::lexer:
        {
            const auto& l = std::get<lexer>(v);

            success = process_lexer(l, data._first, data._ranges, data._captures);
            data._negate = (l._flags & *config_flags::negate) != 0;
            break;
        }
        case match_type::ulexer:
        {
            const auto& l = std::get<ulexer>(v);

            success = process_lexer(l, data._first, data._ranges, data._captures);
            data._negate = (l._flags & *config_flags::negate) != 0;
            break;
        }
        case match_type::parser:
        {
            // Not const as the parser holds state
            // that needs to be mutable (unlike other types)
            auto& p = std::get<parser>(v);

            success = process_parser(p, data._first, data._ranges, data._matches,
                replacements, data._captures);
            data._negate = (p._flags & *config_flags::negate) != 0;
            break;
        }
        case match_type::uparser:
        {
            // Not const as the uparser holds state
            // that needs to be mutable (unlike other types)
            auto& p = std::get<uparser>(v);

            success = process_parser(p, data._first, data._ranges, data._matches,
                replacements, data._captures);
            data._negate = (p._flags & *config_flags::negate) != 0;
            break;
        }
        case match_type::word_list:
        {
            const auto& words = std::get<word_list>(v);

            success = process_word_list(words, data._first, data._ranges,
                data._captures);
            data._negate = (words._flags & *config_flags::negate) != 0;
            break;
        }
        default:
            break;
        }

        if (!success) break;
    }

    return success;
}
