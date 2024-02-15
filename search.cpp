#include "pch.h"

#include "gg_error.hpp"
#include <parsertl/search.hpp>
#include "search.hpp"

extern pipeline g_pipeline;
extern bool g_icase;
extern bool g_output;
extern std::regex g_capture_rx;

static void process_action(const parser& p, const char* start,
    const std::map<uint16_t, std::vector<cmd>>::const_iterator& action_iter,
    const std::pair<uint16_t, token::token_vector>& item,
    std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements)
{
    for (const auto& cmd : action_iter->second)
    {
        const token::token_vector& productions = item.second;

        switch (cmd._type)
        {
        case cmd_type::append:
        {
            const auto& c = std::get<match_cmd>(cmd._action);
            std::string temp = productions[productions.size() -
                p._gsm._rules[item.first]._rhs.size() + cmd._param1].str();
            const uint16_t size = c._front + c._back;

            if (c._front == 0 && c._back == 0)
                matches.top() += temp;
            else
            {
                if (c._front >= temp.size() || size > temp.size())
                {
                    std::ostringstream ss;

                    ss << "substr($" << cmd._param1 + 1 << ", " <<
                        c._front << ", " << c._back <<
                        ") out of range for string '" << temp << "'.";
                    throw gg_error(ss.str());
                }

                matches.top() += temp.substr(c._front, temp.size() - size);
            }

            break;
        }
        case cmd_type::assign:
        {
            const auto& c = std::get<match_cmd>(cmd._action);
            std::string temp = productions[productions.size() -
                p._gsm._rules[item.first]._rhs.size() + cmd._param1].str();

            if (c._front == 0 && c._back == 0)
                matches.top() = std::move(temp);
            else
            {
                const uint16_t size = c._front + c._back;

                if (c._front >= temp.size() || size > temp.size())
                {
                    std::ostringstream ss;

                    ss << "substr($" << cmd._param1 + 1 << ", " <<
                        c._front << ", " << c._back <<
                        ") out of range for string '" << temp << "'.";
                    throw gg_error(ss.str());
                }

                matches.top() = temp.substr(c._front, temp.size() - size);
            }

            break;
        }
        case cmd_type::erase:
        {
            if (g_output)
            {
                const auto& param1 = productions[productions.size() -
                    p._gsm._rules[item.first]._rhs.size() + cmd._param1];
                const auto& param2 = productions[productions.size() -
                    p._gsm._rules[item.first]._rhs.size() + cmd._param2];
                const auto index1 =
                    (cmd._second1 ? param1.second : param1.first) - start;
                const auto index2 =
                    (cmd._second2 ? param2.second : param2.first) - start;

                replacements[std::pair(index1, index2 - index1)] =
                    std::string();
            }

            break;
        }
        case cmd_type::insert:
        {
            if (g_output)
            {
                const auto& c = std::get<insert_cmd>(cmd._action);
                const auto& param = productions[productions.size() -
                    p._gsm._rules[item.first]._rhs.size() + cmd._param1];
                const auto index = (cmd._second1 ? param.second : param.first) -
                    start;

                replacements[std::pair(index, 0)] = c._text;
            }

            break;
        }
        case cmd_type::replace:
        {
            if (g_output)
            {
                const auto& c = std::get<replace_cmd>(cmd._action);
                const auto size = productions.size() -
                    p._gsm._rules[item.first]._rhs.size();
                const auto& param1 = productions[size + cmd._param1];
                const auto& param2 = productions[size + cmd._param2];
                const auto index1 =
                    (cmd._second1 ? param1.second : param1.first) - start;
                const auto index2 =
                    (cmd._second2 ? param2.second : param2.first) - start;

                replacements[std::pair(index1, index2 - index1)] = c._text;
            }

            break;
        }
        case cmd_type::replace_all:
            if (g_output)
            {
                const auto& c = std::get<replace_all_cmd>(cmd._action);
                const auto size = productions.size() -
                    p._gsm._rules[item.first]._rhs.size();
                const auto& param = productions[size + cmd._param1];
                const auto index1 = param.first - start;
                const auto index2 = param.second - start;
                auto pair = std::pair(index1, index2 - index1);
                auto iter = replacements.find(pair);
                const std::string text =
                    std::regex_replace(iter == replacements.end() ?
                        std::string(param.first, param.second) : iter->second,
                        c._rx, c._text);

                replacements[pair] = text;
            }

            break;
        default:
            break;
        }
    }
}

static void process_action(const uparser& p, const char* start,
    const std::map<uint16_t, std::vector<cmd>>::const_iterator& action_iter,
    const std::pair<uint16_t,
    parsertl::token<crutf8iterator>::token_vector>& item,
    std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements)
{
    for (const auto& cmd : action_iter->second)
    {
        const parsertl::token<crutf8iterator>::token_vector& productions =
            item.second;

        switch (cmd._type)
        {
        case cmd_type::append:
        {
            const auto& c = std::get<match_cmd>(cmd._action);
            auto& token = productions[productions.size() -
                p._gsm._rules[item.first]._rhs.size() + cmd._param1];
            std::string temp(token.first.get(), token.second.get());
            const uint16_t size = c._front + c._back;

            if (c._front == 0 && c._back == 0)
                matches.top() += temp;
            else
            {
                if (c._front >= temp.size() || size > temp.size())
                {
                    std::ostringstream ss;

                    ss << "substr($" << cmd._param1 + 1 << ", " <<
                        c._front << ", " << c._back <<
                        ") out of range for string '" << temp << "'.";
                    throw gg_error(ss.str());
                }

                matches.top() += temp.substr(c._front, temp.size() - size);
            }

            break;
        }
        case cmd_type::assign:
        {
            const auto& c = std::get<match_cmd>(cmd._action);
            auto& token = productions[productions.size() -
                p._gsm._rules[item.first]._rhs.size() + cmd._param1];
            std::string temp(token.first.get(), token.second.get());

            if (c._front == 0 && c._back == 0)
                matches.top() = std::move(temp);
            else
            {
                const uint16_t size = c._front + c._back;

                if (c._front >= temp.size() || size > temp.size())
                {
                    std::ostringstream ss;

                    ss << "substr($" << cmd._param1 + 1 << ", " <<
                        c._front << ", " << c._back <<
                        ") out of range for string '" << temp << "'.";
                    throw gg_error(ss.str());
                }

                matches.top() = temp.substr(c._front, temp.size() - size);
            }

            break;
        }
        case cmd_type::erase:
        {
            if (g_output)
            {
                const auto& param1 = productions[productions.size() -
                    p._gsm._rules[item.first]._rhs.size() + cmd._param1];
                const auto& param2 = productions[productions.size() -
                    p._gsm._rules[item.first]._rhs.size() + cmd._param2];
                const auto index1 = (cmd._second1 ? param1.second.get() :
                    param1.first.get()) - start;
                const auto index2 = (cmd._second2 ? param2.second.get() :
                    param2.first.get()) - start;

                replacements[std::pair(index1, index2 - index1)] =
                    std::string();
            }

            break;
        }
        case cmd_type::insert:
        {
            if (g_output)
            {
                const auto& c = std::get<insert_cmd>(cmd._action);
                const auto& param = productions[productions.size() -
                    p._gsm._rules[item.first]._rhs.size() + cmd._param1];
                const auto index = (cmd._second1 ? param.second.get() :
                    param.first.get()) - start;

                replacements[std::pair(index, 0)] = c._text;
            }

            break;
        }
        case cmd_type::replace:
        {
            if (g_output)
            {
                const auto& c = std::get<replace_cmd>(cmd._action);
                const auto size = productions.size() -
                    p._gsm._rules[item.first]._rhs.size();
                const auto& param1 = productions[size + cmd._param1];
                const auto& param2 = productions[size + cmd._param2];
                const auto index1 = (cmd._second1 ? param1.second.get() :
                    param1.first.get()) - start;
                const auto index2 = (cmd._second2 ? param2.second.get() :
                    param2.first.get()) - start;

                replacements[std::pair(index1, index2 - index1)] = c._text;
            }

            break;
        }
        case cmd_type::replace_all:
            if (g_output)
            {
                const auto& c = std::get<replace_all_cmd>(cmd._action);
                const auto size = productions.size() -
                    p._gsm._rules[item.first]._rhs.size();
                const auto& param = productions[size + cmd._param1];
                const auto index1 = param.first.get() - start;
                const auto index2 = param.second.get() - start;
                auto pair = std::pair(index1, index2 - index1);
                auto iter = replacements.find(pair);
                const std::string text =
                    std::regex_replace(iter == replacements.end() ?
                        std::string(param.first.get(), param.second.get()) :
                        iter->second,
                        c._rx, c._text);

                replacements[pair] = text;
            }

            break;
        default:
            break;
        }
    }
}

static std::string build_text(const std::string& input, std::vector<std::string>& captures)
{
    std::string output;
    const char* last = input.c_str();
    std::cregex_iterator iter(input.c_str(), input.c_str() + input.size(), g_capture_rx);
    std::cregex_iterator end;

    for (; iter != end; ++iter)
    {
        const std::size_t idx = static_cast<std::size_t>(atoi((*iter)[0].first + 1));

        output.append(last, (*iter)[0].first);
        output += idx >= captures.size() ? std::string() : captures[idx];
        last = (*iter)[0].second;
    }

    output.append(last, input.c_str() + input.size());
    return output;
}

static bool process_text(const text& t, const char* data_first,
    std::vector<match>& ranges, std::vector<std::string>& captures)
{
    const std::string text = build_text(t._text, captures);
    const bool back_dig = text.back() >= '0' && text.back() <= '9';
    const char* first = ranges.back()._first;
    const char* second = ranges.back()._eoi;
    bool success = false;
    bool whole_word = false;

    do
    {
        first = text.empty() ? ranges.back()._eoi :
            g_icase ?
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
                whole_word = success && (!(t._flags & config_flags::whole_word) ||
                    !(first == data_first ||
                        (*(first - 1) >= 'A' && *(first - 1) <= 'Z') ||
                        *(first - 1) == '_' ||
                        (*(first - 1) >= 'a' && *(first - 1) <= 'z'))) &&
                    (!(t._flags & config_flags::whole_word) ||
                        !(second == ranges.front()._eoi ||
                            (*second >= 'A' && *second <= 'Z') ||
                            *second == '_' ||
                            (*second >= 'a' && *second <= 'z') ||
                            (back_dig && *second >= '0' && *second <= '9')));

                if (success && !whole_word)
                {
                    ++first;
                    second = ranges.back()._eoi;
                }
    } while (success && !whole_word);

    success &= whole_word;

    if (success)
    {
        if (t._flags & config_flags::negate)
        {
            if (t._flags & config_flags::negate_all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                ranges.back()._second = first + text.size();

                if (last_start == first)
                {
                    // The match is right at the beginning, so skip.
                    ranges.emplace_back(second, second, second);
                    success = false;
                }
                else
                    ranges.emplace_back(ranges.back()._first, first, first);
            }
        }
        else
        {
            // Store start of match
            ranges.back()._first = first;
            // Store end of match
            ranges.back()._second = second;
            ranges.emplace_back((t._flags & config_flags::extend_search) ?
                second :
                first,
                (t._flags & config_flags::extend_search) ?
                second :
                first,
                (t._flags & config_flags::extend_search) ?
                ranges.back()._eoi :
                second);
        }
    }
    else if (t._flags & config_flags::negate)
    {
        if (ranges.back()._first != ranges.back()._eoi)
        {
            ranges.back()._second = first;
            ranges.emplace_back(ranges.back()._first, first, first);
            success = true;
        }
    }

    if (success)
    {
        captures.clear();
        captures.emplace_back(ranges.back()._first, ranges.back()._eoi);
    }

    return success;
}

static bool process_regex(const regex& r, std::vector<match>& ranges,
    std::vector<std::string>& captures)
{
    std::cregex_iterator iter(ranges.back()._first,
        ranges.back()._eoi, r._rx);
    std::cregex_iterator end;
    bool success = iter != end;

    if (success)
    {
        if (r._flags & config_flags::negate)
        {
            if (r._flags & config_flags::negate_all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                ranges.back()._second = (*iter)[0].second;

                if (last_start == (*iter)[0].first)
                {
                    // The match is right at the beginning, so skip.
                    ranges.emplace_back((*iter)[0].second, (*iter)[0].second,
                        (*iter)[0].second);
                    success = false;
                }
                else
                    ranges.emplace_back(ranges.back()._first, (*iter)[0].first,
                        (*iter)[0].first);
            }
        }
        else
        {
            // Store start of match
            ranges.back()._first = (*iter)[0].first;
            // Store end of match
            ranges.back()._second = (*iter)[0].second;
            ranges.emplace_back((r._flags & config_flags::extend_search) ?
                (*iter)[0].second :
                (*iter)[0].first,
                (r._flags & config_flags::extend_search) ?
                (*iter)[0].second :
                (*iter)[0].first,
                (r._flags & config_flags::extend_search) ?
                ranges.back()._eoi :
                (*iter)[0].second);
        }
    }
    else if (r._flags & config_flags::negate)
    {
        if (ranges.back()._first != ranges.back()._eoi)
        {
            ranges.back()._second = ranges.back()._eoi;
            ranges.emplace_back(ranges.back()._first, ranges.back()._eoi,
                ranges.back()._eoi);
            success = true;
        }
    }

    if (success)
    {
        captures.clear();

        if (r._flags & config_flags::negate)
            captures.emplace_back(ranges.back()._first, ranges.back()._second);
        else
        {
            for (const auto& m : *iter)
            {
                captures.push_back(m.str());
            }
        }
    }

    return success;
}

static bool process_lexer(const lexer& l, std::vector<match>& ranges,
    std::vector<std::string>& captures)
{
    lexertl::criterator iter(ranges.back()._first,
        ranges.back()._eoi, l._sm);
    bool success = iter->first != ranges.back()._eoi;

    if (success)
    {
        if (l._flags & config_flags::negate)
        {
            if (l._flags & config_flags::negate_all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                ranges.back()._second = iter->second;

                if (last_start == iter->first)
                {
                    // The match is right at the beginning, so skip.
                    ranges.emplace_back(iter->second, iter->second,
                        iter->second);
                    success = false;
                }
                else
                    ranges.emplace_back(ranges.back()._first, iter->first,
                        iter->first);
            }
        }
        else
        {
            // Store start of match
            ranges.back()._first = iter->first;
            // Store end of match
            ranges.back()._second = iter->second;
            ranges.emplace_back((l._flags & config_flags::extend_search) ?
                iter->second :
                iter->first,
                (l._flags & config_flags::extend_search) ?
                iter->second :
                iter->first,
                (l._flags & config_flags::extend_search) ?
                ranges.back()._eoi :
                iter->second);
        }
    }
    else if (l._flags & config_flags::negate)
    {
        if (ranges.back()._first != ranges.back()._eoi)
        {
            ranges.back()._second = iter->first;
            ranges.emplace_back(ranges.back()._first, iter->first, iter->first);
            success = true;
        }
    }

    if (success)
    {
        captures.clear();
        captures.emplace_back(ranges.back()._first, ranges.back()._eoi);
    }

    return success;
}

static bool process_lexer(const ulexer& l, std::vector<match>& ranges,
    std::vector<std::string>& captures)
{
    crutf8iterator iter(utf8_iterator(ranges.back()._first, ranges.back()._eoi),
        utf8_iterator(ranges.back()._eoi, ranges.back()._eoi), l._sm);
    bool success =
        iter->first != utf8_iterator(ranges.back()._eoi, ranges.back()._eoi);

    if (success)
    {
        if (l._flags & config_flags::negate)
        {
            if (l._flags & config_flags::negate_all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                ranges.back()._second = iter->second.get();

                if (last_start == iter->first.get())
                {
                    // The match is right at the beginning, so skip.
                    ranges.emplace_back(iter->second.get(), iter->second.get(),
                        iter->second.get());
                    success = false;
                }
                else
                    ranges.emplace_back(ranges.back()._first, iter->first.get(),
                        iter->first.get());
            }
        }
        else
        {
            // Store start of match
            ranges.back()._first = iter->first.get();
            // Store end of match
            ranges.back()._second = iter->second.get();
            ranges.emplace_back((l._flags & config_flags::extend_search) ?
                iter->second.get() :
                iter->first.get(),
                (l._flags & config_flags::extend_search) ?
                iter->second.get() :
                iter->first.get(),
                (l._flags & config_flags::extend_search) ?
                ranges.back()._eoi :
                iter->second.get());
        }
    }
    else if (l._flags & config_flags::negate)
    {
        if (ranges.back()._first != ranges.back()._eoi)
        {
            ranges.back()._second = iter->first.get();
            ranges.emplace_back(ranges.back()._first, iter->first.get(),
                iter->first.get());
            success = true;
        }
    }

    if (success)
    {
        captures.clear();
        captures.emplace_back(ranges.back()._first, ranges.back()._eoi);
    }

    return success;
}

static bool process_parser(const parser& p, const char* start,
    std::vector<match>& ranges, std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements,
    std::vector<std::string>& captures)
{
    using results = std::vector<std::vector<std::pair
        <const char*, const char*>>>;
    lexertl::criterator iter(ranges.back()._first,
        ranges.back()._eoi, p._lsm);
    lexertl::criterator end;
    std::multimap<uint16_t, token::token_vector> prod_map;
    results cap;
    bool success = false;

    if (p._gsm._captures.empty())
        success = parsertl::search(iter, end, p._gsm, &prod_map);
    else
        success = parsertl::search(iter, end, p._gsm, cap);

    if (success)
    {
        if (p._flags & config_flags::negate)
        {
            if (p._flags & config_flags::negate_all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                ranges.back()._second = end->first;

                if (last_start == iter->first)
                {
                    // The match is right at the beginning, so skip.
                    ranges.emplace_back(iter->second, iter->second,
                        iter->second);
                    success = false;
                }
                else
                    ranges.emplace_back(ranges.back()._first, iter->first,
                        iter->first);
            }
        }
        else
        {
            if (!p._actions.empty())
                matches.emplace();

            // Only care about grammar captures with a positive match
            if (p._gsm._captures.empty())
            {
                // Store start of match
                ranges.back()._first = iter->first;
                // Store end of match
                ranges.back()._second = end->first;
                ranges.emplace_back((p._flags & config_flags::extend_search) ?
                    end->first :
                    iter->first,
                    (p._flags & config_flags::extend_search) ?
                    end->first :
                    iter->first,
                    (p._flags & config_flags::extend_search) ?
                    ranges.back()._eoi :
                    end->first);
            }
            else
            {
                // Store start of match
                ranges.back()._first = cap[0][0].first;
                // Store end of match
                ranges.back()._second = cap[0][0].second;

                for (std::size_t idx = 1, size = cap.size();
                    idx < size; ++idx)
                {
                    for (const auto& pair : cap[idx])
                    {
                        ranges.emplace_back((p._flags & config_flags::extend_search) ?
                            pair.second :
                            pair.first,
                            (p._flags & config_flags::extend_search) ?
                            pair.second :
                            pair.first,
                            (p._flags & config_flags::extend_search) ?
                            ranges.back()._eoi :
                            pair.second);
                    }
                }
            }

            if (!p._reduce_set.empty())
            {
                // Success only if a _reduce_set item is found
                success = false;
            }

            for (const auto& item : prod_map)
            {
                const auto action_iter = p._actions.find(item.first);

                if (action_iter != p._actions.end())
                {
                    process_action(p, start, action_iter, item,
                        matches, replacements);
                    ranges.back()._first = ranges.back()._second =
                        matches.top().c_str();
                    ranges.back()._eoi = matches.top().c_str() +
                        matches.top().size();
                }
                else
                {
                    if (p._reduce_set.empty() ||
                        p._reduce_set.contains(item.first))
                    {
                        success = true;

                        if (!matches.empty())
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
    else if (p._flags & config_flags::negate)
    {
        if (ranges.back()._first != ranges.back()._eoi)
        {
            ranges.back()._second = iter->first;
            ranges.emplace_back(ranges.back()._first, iter->first,
                iter->first);
            success = true;
        }
    }

    if (success)
    {
        captures.clear();

        if (p._flags & config_flags::negate)
            captures.emplace_back(ranges.back()._first, ranges.back()._second);
        else
        {
            for (const auto& v : cap)
            {
                if (v.empty())
                    captures.emplace_back();
                else
                    captures.emplace_back(v.front().first, v.front().second);
            }
        }
    }

    return success;
}

static bool process_parser(const uparser& p, const char* start,
    std::vector<match>& ranges, std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements,
    std::vector<std::string>& captures)
{
    using results = std::vector<std::vector<std::pair
        <utf8_iterator, utf8_iterator>>>;
    crutf8iterator iter(utf8_iterator(ranges.back()._first, ranges.back()._eoi),
        utf8_iterator(ranges.back()._eoi, ranges.back()._eoi), p._lsm);
    crutf8iterator end;
    std::multimap<uint16_t,
        parsertl::token<crutf8iterator>::token_vector> prod_map;
    results cap;
    bool success = false;

    if (p._gsm._captures.empty())
        success = parsertl::search(iter, end, p._gsm, &prod_map);
    else
        success = parsertl::search(iter, end, p._gsm, cap);

    if (success)
    {
        if (p._flags & config_flags::negate)
        {
            if (p._flags & config_flags::negate_all)
                success = false;
            else
            {
                const char* last_start = ranges.back()._first;

                ranges.back()._second = end->first.get();

                if (last_start == iter->first.get())
                {
                    // The match is right at the beginning, so skip.
                    ranges.emplace_back(iter->second.get(), iter->second.get(),
                        iter->second.get());
                    success = false;
                }
                else
                    ranges.emplace_back(ranges.back()._first,
                        iter->first.get(), iter->first.get());
            }
        }
        else
        {
            if (!p._actions.empty())
                matches.emplace();

            // Only care about grammar captures with a positive match
            if (p._gsm._captures.empty())
            {
                // Store start of match
                ranges.back()._first = iter->first.get();
                // Store end of match
                ranges.back()._second = end->first.get();
                ranges.emplace_back((p._flags & config_flags::extend_search) ?
                    end->first.get() :
                    iter->first.get(),
                    (p._flags & config_flags::extend_search) ?
                    end->first.get() :
                    iter->first.get(),
                    (p._flags & config_flags::extend_search) ?
                    ranges.back()._eoi :
                    end->first.get());
            }
            else
            {
                // Store start of match
                ranges.back()._first = cap[0][0].first.get();
                // Store end of match
                ranges.back()._second = cap[0][0].second.get();

                for (std::size_t idx = 1, size = cap.size();
                    idx < size; ++idx)
                {
                    for (const auto& pair : cap[idx])
                    {
                        ranges.emplace_back((p._flags & config_flags::extend_search) ?
                            pair.second.get() :
                            pair.first.get(),
                            (p._flags & config_flags::extend_search) ?
                            pair.second.get() :
                            pair.first.get(),
                            (p._flags & config_flags::extend_search) ?
                            ranges.back()._eoi :
                            pair.second.get());
                    }
                }
            }

            if (!p._reduce_set.empty())
            {
                // Success only if a _reduce_set item is found
                success = false;
            }

            for (const auto& item : prod_map)
            {
                const auto action_iter = p._actions.find(item.first);

                if (action_iter != p._actions.end())
                {
                    process_action(p, start, action_iter, item, matches,
                        replacements);
                    ranges.back()._first = ranges.back()._second =
                        matches.top().c_str();
                    ranges.back()._eoi = matches.top().c_str() +
                        matches.top().size();
                }
                else
                {
                    if (p._reduce_set.empty() ||
                        p._reduce_set.contains(item.first))
                    {
                        success = true;

                        if (!matches.empty())
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
    else if (p._flags & config_flags::negate)
    {
        if (ranges.back()._first != ranges.back()._eoi)
        {
            ranges.back()._second = iter->first.get();
            ranges.emplace_back(ranges.back()._first, iter->first.get(),
                iter->first.get());
            success = true;
        }
    }

    if (success)
    {
        captures.clear();

        if (p._flags & config_flags::negate)
            captures.emplace_back(ranges.back()._first, ranges.back()._second);
        else
        {
            for (const auto& v : cap)
            {
                captures.emplace_back(v.front().first.get(),
                    v.front().second.get());
            }
        }
    }

    return success;
}

std::pair<bool, bool> search(std::vector<match>& ranges, const char* data_first,
    std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements,
    std::vector<std::string>& captures)
{
    bool success = false;
    bool negate = false;

    for (std::size_t index = ranges.size() - 1, size = g_pipeline.size();
        index < size; ++index)
    {
        switch (const auto& v = g_pipeline[index]; static_cast<match_type>(v.index()))
        {
        case match_type::text:
        {
            const auto& t = std::get<text>(v);

            success = process_text(t, data_first, ranges, captures);
            negate = (t._flags & config_flags::negate) != 0;
            break;
        }
        case match_type::regex:
        {
            const auto& r = std::get<regex>(v);

            success = process_regex(r, ranges, captures);
            negate = (r._flags & config_flags::negate) != 0;
            break;
        }
        case match_type::lexer:
        {
            const auto& l = std::get<lexer>(v);

            success = process_lexer(l, ranges, captures);
            negate = (l._flags & config_flags::negate) != 0;
            break;
        }
        case match_type::ulexer:
        {
            const auto& l = std::get<ulexer>(v);

            success = process_lexer(l, ranges, captures);
            negate = (l._flags & config_flags::negate) != 0;
            break;
        }
        case match_type::parser:
        {
            const auto& p = std::get<parser>(v);

            success = process_parser(p, data_first, ranges, matches,
                replacements, captures);
            negate = (p._flags & config_flags::negate) != 0;
            break;
        }
        case match_type::uparser:
        {
            const auto& p = std::get<uparser>(v);

            success = process_parser(p, data_first, ranges, matches,
                replacements, captures);
            negate = (p._flags & config_flags::negate) != 0;
            break;
        }
        default:
            break;
        }

        if (!success) break;
    }

    return std::make_pair(success, negate);
}
