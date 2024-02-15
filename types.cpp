#include "pch.h"

#include <lexertl/generator.hpp>
#include <parsertl/generator.hpp>
#include "gg_error.hpp"
#include <parsertl/lookup.hpp>
#include "types.hpp"

extern config_parser g_config_parser;
extern parser* g_curr_parser;
extern uparser* g_curr_uparser;
extern bool g_icase;
extern bool g_force_unicode;

void config_state::parse(const std::string& config_pathname)
{
    lexertl::citerator iter;
    lexertl::citerator end;

    if (g_icase)
    {
        using enum lexertl::regex_flags;

        if (g_force_unicode)
            _lurules.flags(*icase | *dot_not_cr_lf);
        else
            _lrules.flags(*icase | *dot_not_cr_lf);
    }

    _mf.open(config_pathname.c_str());

    if (!_mf.data())
    {
        std::ostringstream ss;

        ss << "Unable to open " << config_pathname;
        throw gg_error(ss.str());
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
                    std::ostringstream ss;
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
                    ss << config_pathname << '(' << line << "): " << e.what();
                    throw gg_error(ss.str());
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
        std::ostringstream ss;

        ss << config_pathname << '(' << line << ':' << column << "): Parse error";
        throw gg_error(ss.str());
    }

    _mf.close();

    if (_grules.grammar().empty())
    {
        if (g_force_unicode)
            _lurules.push(".{+}[\r\n]", lurules::skip());
        else
            _lrules.push(".{+}[\r\n]", lexertl::rules::skip());
    }
    else
    {
        std::string warnings;
        parsertl::rules::string_vector terminals;
        const auto& grammar = _grules.grammar();
        const auto& ids = g_force_unicode ? _lurules.ids() : _lrules.ids();
        std::set<std::size_t> used_tokens;
        std::set<std::size_t> used_prec;

        if (g_force_unicode)
            parsertl::generator::build(_grules, g_curr_uparser->_gsm,
                &warnings);
        else
            parsertl::generator::build(_grules, g_curr_parser->_gsm,
                &warnings);

        _grules.terminals(terminals);

        if (!warnings.empty())
            std::cerr << "Warnings from config " << config_pathname << " : " <<
            warnings;

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
                std::cerr << "Warning: Token \"" << terminals[i] <<
                "\" does not have a lexer definiton.\n";

            if (!used_tokens.contains(i) && !used_prec.contains(i))
            {
                std::cerr << "Warning: Token \"" << terminals[i] <<
                    "\" is not used in the grammar.\n";
            }
        }
    }

    if (g_force_unicode)
    {
        using rules_type = lexertl::basic_rules<char, char32_t>;
        using generator = lexertl::basic_generator<rules_type,
            lexertl::u32state_machine>;

        generator::build(_lurules, g_curr_uparser->_lsm);
    }
    else
        lexertl::generator::build(_lrules, g_curr_parser->_lsm);
}
