#include "pch.h"

#include "colours.hpp"
#include <format>
#include <lexertl/generator.hpp>
#include <parsertl/generator.hpp>
#include "gg_error.hpp"
#include <parsertl/lookup.hpp>
#include "output.hpp"
#include "types.hpp"

extern options g_options;
extern config_parser g_config_parser;
extern parser* g_curr_parser;
extern uparser* g_curr_uparser;

[[nodiscard]] std::string exec_ret(const std::string& cmd)
{
    std::array<char, 128> buffer{};
    std::string result;
#ifdef _WIN32
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "rb"), &_pclose);
#else
    auto closer = [](std::FILE* fp) { pclose(fp); };
    std::unique_ptr<FILE, decltype(closer)> pipe(popen(cmd.c_str(), "r"), closer);
#endif
    std::size_t size = 0;

    if (!pipe)
    {
        throw gg_error("Failed to open " + cmd);
    }

    result.reserve(1024);

    do
    {
        size = fread(buffer.data(), 1, buffer.size(), pipe.get());
        result.append(buffer.data(), size);
    } while (size);

    return result;
}

std::string actions::exec(cmd* command)
{
    std::string output;
    std::vector<cmd_data> stack;

    _cmd_stack.push_back(command);

    while (!_cmd_stack.empty())
    {
        command = _cmd_stack.back();

        switch (command->_type)
        {
        case cmd::type::format:
        {
            auto ptr = static_cast<format_cmd*>(command);

            _index_stack.insert(_index_stack.end(), ptr->_params.size(),
                stack.size());
            stack.emplace_back(ptr->_type, ptr->_params.size());

            for (auto iter = ptr->_params.rbegin(), end = ptr->_params.rend();
                iter != end; ++iter)
            {
                _cmd_stack.push_back(*iter);
            }

            break;
        }
        case cmd::type::print:
        {
            auto ptr = static_cast<print_cmd*>(command);

            // print() doesn't return anything, so pop
            _cmd_stack.pop_back();
            _cmd_stack.push_back(ptr->_param);
            break;
        }
        case cmd::type::replace_all:
        {
            auto ptr = static_cast<replace_all_cmd*>(command);

            _index_stack.insert(_index_stack.end(), 3, stack.size());
            stack.emplace_back(ptr->_type, 3);
            _cmd_stack.push_back(ptr->_params[2]);
            _cmd_stack.push_back(ptr->_params[1]);
            _cmd_stack.push_back(ptr->_params[0]);
            break;
        }
        case cmd::type::string:
        {
            auto ptr = static_cast<string_cmd*>(command);

            if (stack.empty())
            {
                stack.emplace_back(cmd::type::string, 1);
                stack.back()._params.push_back(ptr->_str);
            }
            else
            {
                stack[_index_stack.back()]._params.push_back(ptr->_str);
                _index_stack.pop_back();
            }

            _cmd_stack.pop_back();
            break;
        }
        case cmd::type::system:
        {
            auto ptr = static_cast<system_cmd*>(command);

            _index_stack.push_back(stack.size());
            stack.emplace_back(ptr->_type, 1);
            _cmd_stack.push_back(ptr->_param);
            break;
        }
        default:
            break;
        }

        while (!stack.empty() && stack.back().ready())
        {
            // Execute command
            output = stack.back().system();

            if (!_index_stack.empty())
            {
                stack[_index_stack.back()]._params.push_back(output);
                _index_stack.pop_back();
            }

            stack.pop_back();

            if (!_cmd_stack.empty())
                _cmd_stack.pop_back();
        }
    }

    return output;
}

std::string cmd_data::system() const
{
    std::string output;

    switch (_type)
    {
    case cmd::type::format:
    {
        auto iter = _params.begin();
        auto end = _params.end();

        output = *iter;
        ++iter;

        for (; iter != end; ++iter)
        {
            const auto fmt_idx = output.find("{}");

            if (fmt_idx != std::string::npos)
                output.replace(fmt_idx, 2, *iter);
        }

        break;
    }
    case cmd::type::replace_all:
        output = std::regex_replace(_params[0], std::regex(_params[1]), _params[2]);
        break;
    case cmd::type::string:
        output = _params.back();
        break;
    case cmd::type::system:
        output = exec_ret(_params.back());
        break;
    default:
        break;
    }

    return output;
}

void config_state::parse(const unsigned int flags,
    const std::string& config_pathname)
{
    lexertl::citerator iter;
    lexertl::citerator end;

    if (flags & config_flags::icase)
    {
        using enum lexertl::regex_flags;

        if (g_options._force_unicode)
            _lurules.flags(*icase | *dot_not_cr_lf);
        else
            _lrules.flags(*icase | *dot_not_cr_lf);
    }

    _mf.open(config_pathname.c_str());

    if (!_mf.data())
    {
        throw gg_error(std::format("Unable to open {}", config_pathname));
    }

    iter = lexertl::citerator(_mf.data(), _mf.data() + _mf.size(),
        g_config_parser._lsm);
    _results.reset(iter->id, g_config_parser._gsm);

    while (_results.entry.action != parsertl::action::error &&
        _results.entry.action != parsertl::action::accept)
    {
        if (_results.entry.action == parsertl::action::reduce)
        {
            auto i = g_config_parser._actions.find(_results.entry.param);

            if (i != g_config_parser._actions.end())
            {
                try
                {
                    i->second(*this, g_config_parser);
                }
                catch (const std::exception& e)
                {
                    const auto idx =
                        _results.production_size(g_config_parser._gsm,
                            _results.entry.param) - 1;
                    const auto& token =
                        _results.dollar(idx, g_config_parser._gsm,
                            _productions);
                    const std::size_t line =
                        1 + std::count(_mf.data(), token.first, '\n');

                    // Column makes no sense here as we are
                    // already at the end of the line
                    throw gg_error(std::format("{}({}): {}",
                        config_pathname,
                        line,
                        e.what()));
                }
            }
        }

        parsertl::lookup(iter, g_config_parser._gsm, _results,
            _productions);
    }

    if (_results.entry.action == parsertl::action::error)
    {
        const std::size_t line =
            1 + std::count(_mf.data(), iter->first, '\n');
        const char endl[] = { '\n' };
        const std::size_t column = iter->first - std::find_end(_mf.data(),
            iter->first, endl, endl + 1);

        throw gg_error(std::format("{}({}:{}): Parse error",
            config_pathname,
            line,
            column));
    }

    _mf.close();

    if (_grules.grammar().empty())
    {
        if (g_options._force_unicode)
            _lurules.push("(?s:.)", lurules::skip());
        else
            _lrules.push("(?s:.)", lexertl::rules::skip());
    }
    else
    {
        std::string warnings;
        parsertl::rules::string_vector terminals;
        const auto& grammar = _grules.grammar();
        const auto& ids = g_options._force_unicode ?
            _lurules.ids() :
            _lrules.ids();
        std::set<std::size_t> used_tokens;
        std::set<std::size_t> used_prec;

        if (g_options._force_unicode)
            parsertl::generator::build(_grules, g_curr_uparser->_gsm,
                &warnings);
        else
            parsertl::generator::build(_grules, g_curr_parser->_gsm,
                &warnings);

        _grules.terminals(terminals);

        if (!warnings.empty())
        {
            if (!g_options._no_messages)
            {
                const std::string msg =
                    std::format("gram_grep: Warnings from config {} : {}",
                        config_pathname, warnings);

                output_text(std::cerr, g_wa_text.c_str(), msg);
            }
        }

        for (const auto& p : grammar)
        {
            // Check for %prec TOKEN
            if (!p._rhs._prec.empty())
            {
                const auto pos = std::ranges::find(terminals, p._rhs._prec);

                if (pos != terminals.end())
                {
                    const size_t idx = pos - terminals.begin();

                    used_prec.insert(idx);
                }
            }

            for (const auto& rhs : p._rhs._symbols)
            {
                if (rhs._type == parsertl::rules::symbol::type::TERMINAL)
                    used_tokens.insert(rhs._id);
            }
        }

        for (std::size_t i = 1, size = terminals.size(); i < size; ++i)
        {
            bool found_id = false;

            for (const auto& curr_ids : ids)
            {
                found_id = std::ranges::find(curr_ids, i) != curr_ids.end() ||
                    used_prec.contains(i);

                if (found_id) break;
            }

            if (!found_id)
            {
                if (!g_options._no_messages)
                {
                    const std::string msg =
                        std::format("gram_grep: Token \"{}\" does not have a "
                            "lexer definiton.", terminals[i]);

                    output_text_nl(std::cerr, g_wa_text.c_str(), msg);
                }
            }

            if (!used_tokens.contains(i) && !used_prec.contains(i) &&
                std::ranges::find(_consume, terminals[i]) == _consume.end())
            {
                if (!g_options._no_messages)
                {
                    const std::string msg =
                        std::format("gram_grep: Token \"{}\" is not used in "
                            "the grammar.", terminals[i]);

                    output_text_nl(std::cerr, g_wa_text.c_str(), msg);
                }
            }
        }
    }

    if (g_options._force_unicode)
    {
        using rules_type = lexertl::basic_rules<char, char32_t>;
        using generator = lexertl::basic_generator<rules_type,
            lexertl::u32state_machine>;

        generator::build(_lurules, g_curr_uparser->_lsm);
    }
    else
        lexertl::generator::build(_lrules, g_curr_parser->_lsm);
}
