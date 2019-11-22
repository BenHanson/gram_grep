// A grep program that allows search by grammar or by lexer spec only
// as well as the conventional way.

#include "stdafx.h"

//#include "../parsertl14/include/parsertl/debug.hpp"
#include <filesystem>
#include <fstream>
#include "../parsertl14/include/parsertl/generator.hpp"
#include "../parsertl14/include/parsertl/lookup.hpp"
#include <iostream>
#include "../lexertl14/include/lexertl/iterator.hpp"
#include "../parsertl14/include/parsertl/match_results.hpp"
#include "../lexertl14/include/lexertl/memory_file.hpp"
#include <regex>
#include "../parsertl14/include/parsertl/search.hpp"
#include <variant>
#include "../wildcardtl/include/wildcardtl/wildcard.hpp"

enum class match_type
{
	// Must match order of variant below
	parser, lexer, regex, dfa_regex
};

struct base
{
	bool _negate = false;
	bool _all = false;

	virtual ~base() = default;
};

enum class cmd_type
{
	unknown, assign, append, erase, insert, replace, replace_all
};

struct erase_cmd
{
};

struct insert_cmd
{
	std::string _text;

	insert_cmd(const std::string &text) :
		_text(text)
	{
	}
};

struct match_cmd
{
	uint16_t _front;
	uint16_t _back;

	match_cmd(const uint16_t front,
		const uint16_t back) :
		_front(front),
		_back(back)
	{
	}
};

struct replace_cmd
{
	std::string _text;

	replace_cmd(const std::string &text) :
		_text(text)
	{
	}
};

struct replace_all_cmd
{
	std::regex _rx;
	std::string _text;

	replace_all_cmd(const std::string &rx, const std::string &text) :
		_rx(rx),
		_text(text)
	{
	}
};

struct cmd
{
	using action = std::variant<erase_cmd, insert_cmd, match_cmd, replace_cmd,
		replace_all_cmd>;
	cmd_type _type = cmd_type::unknown;
	uint16_t _param1;
	bool _second1 = false;
	uint16_t _param2;
	bool _second2 = true;
	action _action;

	cmd(const cmd_type type, const uint16_t param, const action &action) :
		_type(type),
		_param1(param),
		_param2(param),
		_action(action)
	{
	}

	cmd(const cmd_type type, const uint16_t param, const bool second,
		const action &action) :
		_type(type),
		_param1(param),
		_second1(second),
		_param2(param),
		_action(action)
	{
	}

	cmd(const cmd_type type, const uint16_t param1, const uint16_t param2,
		const action &action) :
		_type(type),
		_param1(param1),
		_param2(param2),
		_action(action)
	{
	}

	cmd(const cmd_type type, const uint16_t param1, const bool second1,
		const uint16_t param2, const bool second2, const action &action) :
		_type(type),
		_param1(param1),
		_second1(second1),
		_param2(param2),
		_second2(second2),
		_action(action)
	{
	}
};

struct parser : base
{
	parsertl::state_machine _gsm;
	lexertl::state_machine _lsm;
	std::set<uint16_t> _reduce_set;
	std::map<uint16_t, std::vector<cmd>> _actions;
};

struct lexer : base
{
	lexertl::state_machine _sm;
};

struct regex : base
{
	std::regex _rx;
};

struct config_state;
struct config_parser;

using config_actions_map = std::map<uint16_t, void(*)(config_state &state,
	config_parser &parser)>;

struct config
{
	match_type _type;
	std::string _param;
	bool _negate;
	bool _all;

	config(const match_type type, const std::string &param, const bool negate,
		const bool all) :
		_type(type),
		_param(param),
		_negate(negate),
		_all(all)
	{
	}
};

struct config_parser
{
	parsertl::state_machine _gsm;
	lexertl::state_machine _lsm;
	config_actions_map _actions;
};

struct match
{
	const char *_first;
	const char *_second;
	const char *_eoi;

	match(const char *first, const char *second, const char *eoi) :
		_first(first),
		_second(second),
		_eoi(eoi)
	{
	}
};

config_parser g_config_parser;
std::vector<std::variant<parser, lexer, regex>> g_pipeline;
std::vector<wildcardtl::wildcard> g_exclude;
bool g_icase = false;
bool g_modify = false; // Set when grammar has modifying operations
bool g_output = false;
bool g_recursive = false;
std::string g_replace;
std::string g_checkout;
parser *g_curr_parser = nullptr;
std::vector<std::string> g_pathnames;
std::size_t g_hits = 0;
std::size_t g_files = 0;
std::size_t g_searched = 0;

using token = parsertl::token<lexertl::citerator>;

struct config_state
{
	parsertl::rules _grules;
	lexertl::rules _lrules;
	lexertl::memory_file _mf;
	token::token_vector _productions;
	parsertl::match_results _results;

	void parse(const std::string &config_pathname)
	{
		lexertl::citerator iter;
		lexertl::citerator end;

		if (g_icase)
		{
			_lrules.flags(lexertl::icase | lexertl::dot_not_cr_lf);
		}

		_mf.open(config_pathname.c_str());

		if (!_mf.data())
		{
			std::ostringstream ss;

			ss << "Unable to open " << config_pathname << '\n';
			throw std::runtime_error(ss.str());
		}

		iter = lexertl::citerator(_mf.data(), _mf.data() + _mf.size(),
			g_config_parser._lsm);
		_results.reset(iter->id, g_config_parser._gsm);

		while (_results.entry.action != parsertl::error &&
			_results.entry.action != parsertl::accept)
		{
			if (_results.entry.action == parsertl::reduce)
			{
				auto i = g_config_parser._actions.find(_results.entry.param);

				if (i != g_config_parser._actions.end())
				{
					try
					{
						i->second(*this, g_config_parser);
					}
					catch (const std::exception &e)
					{
						std::ostringstream ss;
						const auto idx = _results.production_size(g_config_parser.
							_gsm, _results.entry.param) - 1;
						const auto &token = _results.dollar(g_config_parser._gsm,
							idx, _productions);

						ss << e.what() << "\nLine " << 1 + std::count(_mf.data(),
							token.first, '\n');
						throw std::runtime_error(ss.str());
					}
				}
			}

			parsertl::lookup(g_config_parser._gsm, iter, _results,
				_productions);
		}

		if (_results.entry.action == parsertl::error)
		{
			const std::size_t line =
				1 + std::count(_mf.data(), iter->first, '\n');
			const char endl[] = { '\n' };
			const std::size_t column = iter->first - std::find_end(_mf.data(),
				iter->first, endl, endl + 1);
			std::ostringstream ss;

			ss << "Parse error in " << config_pathname << " at line " <<
				line << ", column " << column << '\n';
			throw std::runtime_error(ss.str());
		}

		_mf.close();

		if (_grules.grammar().empty())
		{
			_lrules.push(".{+}[\r\n]", _lrules.skip());
		}
		else
		{
			std::string warnings;
			parsertl::rules::string_vector terminals;
			const auto &grammar = _grules.grammar();
			const auto &ids = _lrules.ids();
			std::set<std::size_t> used_tokens;

			parsertl::generator::build(_grules, g_curr_parser->_gsm,
				&warnings);
			_grules.terminals(terminals);

			if (!warnings.empty())
			{
				std::cerr << "Warnings from config " << config_pathname <<
					" : " << warnings;
			}

			for (const auto &p : grammar)
			{
				for (const auto &rhs : p._rhs.first)
				{
					if (rhs._type == parsertl::rules::symbol::TERMINAL)
					{
						used_tokens.insert(rhs._id);
					}
				}
			}

			for (std::size_t i = 1, size = terminals.size(); i < size; ++i)
			{
				if (std::find(ids[0].begin(), ids[0].end(), i) == ids[0].end())
				{
					std::cerr << "Warning: Token \"" << terminals[i] <<
						"\" does not have a lexer definiton.\n";
				}

				if (std::find(used_tokens.begin(), used_tokens.end(), i) ==
					used_tokens.end())
				{
					std::cerr << "Warning: Token \"" << terminals[i] <<
						"\" is not used in the grammar.\n";
				}
			}
		}

		lexertl::generator::build(_lrules, g_curr_parser->_lsm);
	}
};

void process_action(const parser &p, const char *start,
	const std::map<uint16_t, std::vector<cmd>>::const_iterator &action_iter,
	const std::pair<uint16_t, token::token_vector> &item,
	std::stack<std::string> &matches,
	std::map<std::pair<std::size_t, std::size_t>, std::string> &replacements)
{
	for (const auto &cmd : action_iter->second)
	{
		const token::token_vector &productions = item.second;

		switch (cmd._type)
		{
		case cmd_type::append:
		{
			const auto &c = std::get<match_cmd>(cmd._action);
			std::string temp = productions[productions.size() -
				p._gsm._rules[item.first].second.size() + cmd._param1].str();
			const uint16_t size = c._front + c._back;

			if (c._front == 0 && c._back == 0)
			{
				matches.top() += temp;
			}
			else
			{
				if (c._front >= temp.size() || size > temp.size())
				{
					std::ostringstream ss;

					ss << "substr($" << cmd._param1 + 1 << ", " <<
						c._front << ", " << c._back <<
						") out of range for string '" << temp << "'.";
					throw std::runtime_error(ss.str());
				}

				matches.top() += temp.substr(c._front, temp.size() - size);
			}

			break;
		}
		case cmd_type::assign:
		{
			const auto &c = std::get<match_cmd>(cmd._action);
			std::string temp = productions[productions.size() -
				p._gsm._rules[item.first].second.size() + cmd._param1].str();

			if (c._front == 0 && c._back == 0)
			{
				matches.top() = std::move(temp);
			}
			else
			{
				const uint16_t size = c._front + c._back;

				if (c._front >= temp.size() || size > temp.size())
				{
					std::ostringstream ss;

					ss << "substr($" << cmd._param1 + 1 << ", " <<
						c._front << ", " << c._back <<
						") out of range for string '" << temp << "'.";
					throw std::runtime_error(ss.str());
				}

				matches.top() = temp.substr(c._front, temp.size() - size);
			}

			break;
		}
		case cmd_type::erase:
		{
			if (g_output)
			{
				const auto &param1 = productions[productions.size() -
					p._gsm._rules[item.first].second.size() + cmd._param1];
				const auto &param2 = productions[productions.size() -
					p._gsm._rules[item.first].second.size() + cmd._param2];
				const auto index1 = (cmd._second1 ? param1.second : param1.first) -
					start;
				const auto index2 = (cmd._second2 ? param2.second : param2.first) -
					start;

				replacements[std::pair(index1, index2 - index1)] = std::string();
			}

			break;
		}
		case cmd_type::insert:
		{
			if (g_output)
			{
				const auto &c = std::get<insert_cmd>(cmd._action);
				const auto &param = productions[productions.size() -
					p._gsm._rules[item.first].second.size() + cmd._param1];
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
				const auto &c = std::get<replace_cmd>(cmd._action);
				const auto size = productions.size() -
					p._gsm._rules[item.first].second.size();
				const auto &param1 = productions[size + cmd._param1];
				const auto &param2 = productions[size + cmd._param2];
				const auto index1 = (cmd._second1 ? param1.second : param1.first) -
					start;
				const auto index2 = (cmd._second2 ? param2.second : param2.first) -
					start;

				replacements[std::pair(index1, index2 - index1)] = c._text;
			}

			break;
		}
		case cmd_type::replace_all:
			if (g_output)
			{
				const auto &c = std::get<replace_all_cmd>(cmd._action);
				const auto size = productions.size() -
					p._gsm._rules[item.first].second.size();
				const auto &param = productions[size + cmd._param1];
				const auto index1 = param.first - start;
				const auto index2 = param.second - start;
				auto pair = std::pair(index1, index2 - index1);
				auto iter = replacements.find(pair);
				const std::string text = std::regex_replace(iter == replacements.end() ?
					std::string(param.first, param.second) : iter->second,
					c._rx, c._text);

				replacements[pair] = text;
			}

			break;
		}
	}
}

bool process_parser(const parser &p, const char *start,
	std::vector<match> &ranges, std::stack<std::string> &matches,
	std::map<std::pair<std::size_t, std::size_t>, std::string> &replacements)
{
	lexertl::citerator iter(ranges.back()._first,
		ranges.back()._eoi, p._lsm);
	lexertl::citerator end;
	std::multimap<uint16_t, token::token_vector> prod_map;
	bool success = parsertl::search(p._gsm, iter, end, &prod_map);

	if (success)
	{
		if (p._negate)
		{
			if (p._all)
			{
				success = false;
			}
			else
			{
				const char *last_start = ranges.back()._first;

				ranges.back()._second = iter->second;

				if (last_start == iter->first)
				{
					// The match is right at the beginning,
					// so skip.
					ranges.push_back(match(iter->second,
						iter->second, iter->second));
					success = false;
				}
				else
				{
					ranges.push_back(match(ranges.back()._first,
						iter->first, iter->first));
				}
			}
		}
		else
		{
			if (!p._actions.empty())
			{
				matches.push(std::string());
			}

			// Store start of match
			ranges.back()._first = iter->first;
			// Store end of match
			ranges.back()._second = end->first;
			ranges.push_back(match(iter->first, end->first,
				end->first));

			if (!p._reduce_set.empty())
			{
				// Success only if a _reduce_set item is found
				success = false;
			}

			for (const auto &item : prod_map)
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
						p._reduce_set.find(item.first) != p._reduce_set.end())
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
	else if (p._negate)
	{
		if (ranges.back()._first != ranges.back()._eoi)
		{
			ranges.back()._second = iter->first;
			ranges.push_back(match(ranges.back()._first,
				iter->first, iter->first));
			success = true;
		}
	}

	return success;
}

bool process_lexer(const lexer &l, std::vector<match> &ranges)
{
	lexertl::citerator iter(ranges.back()._first,
		ranges.back()._eoi, l._sm);
	bool success = iter->first != ranges.back()._eoi;

	if (success)
	{
		if (l._negate)
		{
			if (l._all)
			{
				success = false;
			}
			else
			{
				const char *last_start = ranges.back()._first;

				ranges.back()._second = iter->second;

				if (last_start == iter->first)
				{
					// The match is right at the beginning,
					// so skip.
					ranges.push_back(match(iter->second,
						iter->second, iter->second));
					success = false;
				}
				else
				{
					ranges.push_back(match(ranges.back()._first,
						iter->first, iter->first));
				}
			}
		}
		else
		{
			// Store start of match
			ranges.back()._first = iter->first;
			// Store end of match
			ranges.back()._second = iter->second;
			ranges.push_back(match(iter->first, iter->first,
				iter->second));
		}
	}
	else if (l._negate)
	{
		if (ranges.back()._first != ranges.back()._eoi)
		{
			ranges.back()._second = iter->first;
			ranges.push_back(match(ranges.back()._first,
				iter->first, iter->first));
			success = true;
		}
	}

	return success;
}

bool process_regex(const regex &r, std::vector<match> &ranges)
{
	std::cregex_iterator iter(ranges.back()._first,
		ranges.back()._eoi, r._rx);
	std::cregex_iterator end;
	bool success = iter != end;

	if (success)
	{
		if (r._negate)
		{
			if (r._all)
			{
				success = false;
			}
			else
			{
				const char *last_start = ranges.back()._first;

				ranges.back()._second = (*iter)[0].second;

				if (last_start == (*iter)[0].first)
				{
					// The match is right at the beginning,
					// so skip.
					ranges.push_back(match((*iter)[0].second,
						(*iter)[0].second, (*iter)[0].second));
					success = false;
				}
				else
				{
					ranges.push_back(match(ranges.back()._first,
						(*iter)[0].first, (*iter)[0].first));
				}
			}
		}
		else
		{
			// Store start of match
			ranges.back()._first = (*iter)[0].first;
			// Store end of match
			ranges.back()._second = (*iter)[0].second;
			ranges.push_back(match((*iter)[0].first,
				(*iter)[0].first, (*iter)[0].second));
		}
	}
	else if (r._negate)
	{
		if (ranges.back()._first != ranges.back()._eoi)
		{
			ranges.back()._second = ranges.back()._eoi;
			ranges.push_back(match(ranges.back()._first,
				ranges.back()._eoi, ranges.back()._eoi));
			success = true;
		}
	}

	return success;
}

void process_file(const std::string &pathname)
{
	lexertl::memory_file mf(pathname.c_str());
	std::size_t hits = 0;
	std::vector<match> ranges;
	std::stack<std::string> matches;
	std::map<std::pair<std::size_t, std::size_t>, std::string> replacements;

	if (!mf.data())
	{
		std::cerr << "Error: failed to open " << pathname << ".\n";
		return;
	}

	ranges.push_back(match(mf.data(), mf.data(), mf.data() + mf.size()));

	do
	{
		bool success = false;
		std::map<std::pair<std::size_t, std::size_t>, std::string> temp_replacements;

		for (std::size_t index = ranges.size() - 1, size = g_pipeline.size();
			index < size; ++index)
		{
			const auto &v = g_pipeline[index];

			switch (static_cast<match_type>(v.index()))
			{
				case match_type::parser:
				{
					const auto &p = std::get<parser>(v);

					success = process_parser(p, mf.data(), ranges, matches,
						temp_replacements);
					break;
				}
				case match_type::lexer:
				{
					const auto &l = std::get<lexer>(v);

					success = process_lexer(l, ranges);
					break;
				}
				case match_type::regex:
				{
					const auto &r = std::get<regex>(v);

					success = process_regex(r, ranges);
					break;
				}
			}

			if (!success) break;
		}

		if (success)
		{
			const auto &tuple = ranges.front();
			auto iter = ranges.rbegin();
			auto end = ranges.rend();

			replacements.insert(temp_replacements.begin(), temp_replacements.end());
			temp_replacements.clear();

			// Only allow g_replace if g_modify (grammar actions) not set
			if (g_output && !g_modify)
			{
				const char *first = iter->_second;

				if (first < tuple._first || first > tuple._second)
				{
					std::cerr << "Cannot replace text when source "
						"is not contained in original string.\n";
					return;
				}
				else
				{
					// Replace with g_replace.
					const char *second = iter->_eoi;

					replacements[std::make_pair(first - mf.data(),
						second - first)] = g_replace;
				}
			}

			for (; iter != end; ++iter)
			{
				const char *first = iter->_second;

				if (first >= tuple._first && first <= tuple._eoi)
				{
					const char *curr = iter->_first;
					const auto count = std::count(mf.data(), curr, '\n');
					const char *eoi = mf.data() + mf.size();

					std::cout << pathname << '(' << 1 + count << "):";

					if (count == 0)
					{
						curr = mf.data();
					}
					else
					{
						for (; *(curr - 1) != '\n'; --curr);
					}

					for (; curr != eoi && *curr != '\r' && *curr != '\n'; ++curr)
					{
						std::cout << *curr;
					}

					// Flush buffer, to give fast feedback to user
					std::cout << std::endl;
					++hits;
					break;
				}
			}
		}

		const auto old = ranges.back();

		ranges.pop_back();

		if (!ranges.empty())
		{
			const auto curr = ranges.back();

			// Cleardown any stale strings in matches.
			// First makes sure current range is not from the same
			// string (in matches) as the last.
			if (!matches.empty() && (old._first < curr._first ||
				old._first > curr._eoi))
			{
				while (!matches.empty() && old._first >= matches.top().c_str() &&
					old._eoi <= matches.top().c_str() + matches.top().size())
				{
					matches.pop();
				}
			}

			// Start searching from end of last match
			ranges.back()._first = ranges.back()._second;
		}
	} while (!ranges.empty());

	if (hits)
	{
		namespace fs = std::filesystem;

		++g_files;
		g_hits += hits;

		if ((fs::status(pathname.c_str()).permissions() &
			fs::perms::owner_write) != fs::perms::owner_write)
		{
			// Read-only
			if (!g_checkout.empty())
			{
				std::string checkout = g_checkout;
				const std::size_t index = checkout.find("$1");

				if (index != std::string::npos)
				{
					checkout.erase(index, 2);
					checkout.insert(index, pathname);
				}

				if (::system(checkout.c_str()) != 0)
				{
					std::cerr << "Failed to execute " << g_checkout << '\n';
				}
			}
		}

		if (!replacements.empty())
		{
			std::string content(mf.data(), mf.data() + mf.size());

			for (auto iter = replacements.rbegin(), end = replacements.rend();
				iter != end; ++iter)
			{
				content.erase(iter->first.first, iter->first.second);
				content.insert(iter->first.first, iter->second);
			}

			replacements.clear();
			mf.close();

			if ((fs::status(pathname.c_str()).permissions() &
				fs::perms::owner_write) != fs::perms::owner_write)
			{
				std::cerr << pathname << " is read only.\n";
			}
			else
			{
				std::ofstream os(pathname.c_str(), std::ofstream::binary);

				os.exceptions(std::ofstream::badbit);
				os.write(content.c_str(), content.size());
				os.close();
			}
		}
	}

	++g_searched;
}

void process()
{
	namespace fs = std::filesystem;

	for (const auto &pn : g_pathnames)
	{
		const std::size_t index = pn.rfind(fs::path::preferred_separator);
		const std::string path = index == std::string::npos ? "." :
			pn.substr(0, index + 1);
		const std::string fn = index == std::string::npos ?
			pn : pn.substr(path.size());
		wildcardtl::wildcard pathname_wc;

		pathname_wc.assign(fn, true);

		if (g_recursive)
		{
			for (auto iter = fs::recursive_directory_iterator(path),
				end = fs::recursive_directory_iterator(); iter != end; ++iter)
			{
				auto pathname = iter->path();
				const auto &filename = pathname.filename().string();
				bool skip = false;

				if (!g_exclude.empty())
				{
					for (const auto &wc : g_exclude)
					{
						if (wc.match(pathname.string()))
						{
							skip = true;
							break;
						}
					}
				}

				if (!skip && pathname_wc.match(filename) && fs::file_size(iter->path()) != 0)
				{
					process_file(iter->path().string());
				}
			}
		}
		else
		{
			for (auto iter = fs::directory_iterator(path),
				end = fs::directory_iterator(); iter != end; ++iter)
			{
				const auto &pathname = iter->path();
				const auto filename = pathname.filename().string();
				bool skip = false;

				if (!g_exclude.empty())
				{
					for (const auto &wc : g_exclude)
					{
						if (wc.match(pathname.string()))
						{
							skip = true;
							break;
						}
					}
				}

				if (!skip && pathname_wc.match(filename) && fs::file_size(pathname) != 0)
				{
					process_file(pathname.string());
				}
			}
		}
	}
}

std::string unescape_str(const char *first, const char *second)
{
	std::string ret;

	for (; first != second; ++first)
	{
		ret += *first;

		if (*first == '\'' && (first + 1) != second)
			++first;
	}

	return ret;
}

void build_config_parser()
{
	parsertl::rules grules;
	lexertl::rules lrules;

	grules.token("Charset Index Integer Literal Macro MacroName Name NL "
		"Number Repeat ScriptString String");

	grules.push("start", "file");
	grules.push("file",
		"directives '%%' grules '%%' rx_macros '%%' rx_rules '%%'");
	grules.push("directives", "%empty "
		"| directives directive ");
	grules.push("directive", "NL");

	// Read and stored %token entries
	g_config_parser._actions[grules.push("directive", "'%token' tokens NL")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string tokens = token.str();

		state._grules.token(tokens.c_str());
	};
	// Read and stored %left entries
	g_config_parser._actions[grules.push("directive", "'%left' tokens NL")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string tokens = token.str();

		state._grules.left(tokens.c_str());
	};
	// Read and stored %right entries
	g_config_parser._actions[grules.push("directive", "'%right' tokens NL")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string tokens = token.str();

		state._grules.right(tokens.c_str());
	};
	// Read and stored %nonassoc entries
	g_config_parser._actions[grules.push("directive",
		"'%nonassoc' tokens NL")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string tokens = token.str();

		state._grules.nonassoc(tokens.c_str());
	};
	// Read and stored %precedence entries
	g_config_parser._actions[grules.push("directive",
		"'%precedence' tokens NL")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string tokens = token.str();

		state._grules.precedence(tokens.c_str());
	};
	// Read and stored %start
	g_config_parser._actions[grules.push("directive", "'%start' Name NL")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string name = token.str();

		state._grules.start(name.c_str());
	};
	grules.push("tokens", "token "
		"| tokens token");
	grules.push("token", "Literal | Name");

	grules.push("grules", "%empty "
		"| grules grule");
	g_config_parser._actions[grules.push("grule",
		"Name ':' production opt_script ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &lhs = state._results.dollar(parser._gsm, 0,
			state._productions);
		const auto &prod = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t prod_index = state._grules.push(lhs.str(), prod.str());
		auto iter = g_curr_parser->_actions.find(prod_index);

		if (iter != g_curr_parser->_actions.end())
		{
			for (const auto &cmd : iter->second)
			{
				if (cmd._param1 >= state._grules.grammar().back()._rhs.first.size())
				{
					std::ostringstream ss;

					ss << "Index $" << cmd._param1 + 1 << " out of range.";
					throw std::runtime_error(ss.str());
				}

				if (cmd._param2 >= state._grules.grammar().back()._rhs.first.size())
				{
					std::ostringstream ss;

					ss << "Index $" << cmd._param2 + 1 << " out of range.";
					throw std::runtime_error(ss.str());
				}

				if (cmd._param1 == cmd._param2 && cmd._second1 > cmd._second2)
				{
					std::ostringstream ss;

					ss << "Index $" << cmd._param2 + 1 << " cannot have first following second.";
					throw std::runtime_error(ss.str());
				}
			}
		}
	};
	grules.push("production", "opt_list "
		"| production '|' opt_list");
	grules.push("opt_list", "%empty "
		"| '%empty' "
		"| rhs_list opt_prec");
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
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);

		if (token.first == token.second)
		{
			const uint16_t size = static_cast<uint16_t>(state._grules.
				grammar().size());

			// No commands
			g_curr_parser->_reduce_set.insert(size);
		}
	};
	grules.push("cmd_list", "%empty "
		"| cmd_list single_cmd");
	grules.push("single_cmd", "cmd");
	g_config_parser._actions[grules.push("single_cmd",
		"mod_cmd")] =
		[](config_state &, config_parser &)
	{
		g_modify = true;
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'erase' '(' Index ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const uint16_t index = static_cast<uint16_t>(atoi(token.first + 1) - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::erase,
			index, erase_cmd()));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'erase' '(' Index ',' Index ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const auto &token4 = state._results.dollar(parser._gsm, 4,
			state._productions);
		const uint16_t index1 = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const uint16_t index2 = static_cast<uint16_t>(atoi(token4.first + 1) - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::erase,
			index1, index2, erase_cmd()));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'erase' '(' Index '.' first_second ',' Index '.' first_second ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const bool second1 = *state._results.dollar(parser._gsm, 4,
			state._productions).first == 's';
		const auto &token6 = state._results.dollar(parser._gsm, 6,
			state._productions);
		const bool second2 = *state._results.dollar(parser._gsm, 8,
			state._productions).first == 's';
		const uint16_t index1 = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const uint16_t index2 = static_cast<uint16_t>(atoi(token6.first + 1) - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::erase,
			index1, second1, index2, second2, erase_cmd()));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'insert' '(' Index ',' ScriptString ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const auto &token4 = state._results.dollar(parser._gsm, 4,
			state._productions);
		const std::string text = unescape_str(token4.first + 1, token4.second - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::insert,
			index, insert_cmd(text)));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'insert' '(' Index '.' 'second' ',' ScriptString ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const auto &token6 = state._results.dollar(parser._gsm, 6,
			state._productions);
		const std::string text = unescape_str(token6.first + 1, token6.second - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::insert,
			index, true, insert_cmd(text)));
	};
	g_config_parser._actions[grules.push("cmd", "'match' '=' Index ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token.first + 1) - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::assign,
			index, match_cmd(0, 0)));
	};
	g_config_parser._actions[grules.push("cmd",
		"'match' '=' 'substr' '(' Index ',' Integer ',' Integer ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token4 = state._results.dollar(parser._gsm, 4,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token4.first + 1) - 1);
		const auto &token6 = state._results.dollar(parser._gsm, 6,
			state._productions);
		const auto &token8 = state._results.dollar(parser._gsm, 8,
			state._productions);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::assign,
			index, match_cmd(static_cast<uint16_t>(atoi(token6.first)),
				static_cast<uint16_t>(atoi(token8.first)))));
	};
	g_config_parser._actions[grules.push("cmd", "'match' '+=' Index ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token.first + 1) - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::append,
			index, match_cmd(0, 0)));
	};
	g_config_parser._actions[grules.push("cmd",
		"'match' '+=' 'substr' '(' Index ',' Integer ',' Integer ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token4 = state._results.dollar(parser._gsm, 4,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token4.first + 1) - 1);
		const auto &token6 = state._results.dollar(parser._gsm, 6,
			state._productions);
		const auto &token8 = state._results.dollar(parser._gsm, 8,
			state._productions);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::append,
			index, match_cmd(static_cast<uint16_t>(atoi(token6.first)),
				static_cast<uint16_t>(atoi(token8.first)))));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'replace' '(' Index ',' ScriptString ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const auto &token4 = state._results.dollar(parser._gsm, 4,
			state._productions);
		const std::string text = unescape_str(token4.first + 1, token4.second - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::replace,
			index, replace_cmd(text)));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'replace' '(' Index ',' Index ',' ScriptString ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index1 = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const auto &token4 = state._results.dollar(parser._gsm, 4,
			state._productions);
		const uint16_t index2 = static_cast<uint16_t>(atoi(token4.first + 1) - 1);
		const auto &token6 = state._results.dollar(parser._gsm, 6,
			state._productions);
		const std::string text = unescape_str(token6.first + 1, token6.second - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::replace,
			index1, index2, replace_cmd(text)));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'replace' '(' Index '.' first_second ',' Index '.' first_second ',' ScriptString ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index1 = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const bool second1 = *state._results.dollar(parser._gsm, 4,
			state._productions).first == 's';
		const auto &token6 = state._results.dollar(parser._gsm, 6,
			state._productions);
		const uint16_t index2 = static_cast<uint16_t>(atoi(token6.first + 1) - 1);
		const bool second2 = *state._results.dollar(parser._gsm, 8,
			state._productions).first == 's';
		const auto &token10 = state._results.dollar(parser._gsm, 10,
			state._productions);
		const std::string text = unescape_str(token10.first + 1, token10.second - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::replace,
			index1, second1, index2, second2, replace_cmd(text)));
	};
	g_config_parser._actions[grules.push("mod_cmd",
		"'replace_all' '(' Index ',' ScriptString ',' ScriptString ')' ';'")] =
		[](config_state &state, config_parser &parser)
	{
		const uint16_t rule_idx = static_cast<uint16_t>(state._grules.
			grammar().size());
		const auto &token2 = state._results.dollar(parser._gsm, 2,
			state._productions);
		const uint16_t index = static_cast<uint16_t>(atoi(token2.first + 1) - 1);
		const auto &token4 = state._results.dollar(parser._gsm, 4,
			state._productions);
		const std::string text1 = unescape_str(token4.first + 1, token4.second - 1);
		const auto &token6 = state._results.dollar(parser._gsm, 6,
			state._productions);
		const std::string text2 = unescape_str(token6.first + 1, token6.second - 1);

		g_curr_parser->_actions[rule_idx].emplace_back(cmd(cmd_type::replace_all,
			index, replace_all_cmd(text1, text2)));
	};
	grules.push("first_second", "'first' | 'second'");
	grules.push("rx_macros", "%empty");
	g_config_parser._actions[grules.push("rx_macros", "rx_macros MacroName regex")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string name = token.str();
		const std::string regex = state._results.dollar(parser._gsm, 2,
			state._productions).str();

		state._lrules.insert_macro(name.c_str(), regex.c_str());
	};
	grules.push("rx_rules", "%empty");
	g_config_parser._actions[grules.push("rx_rules", "rx_rules regex Number")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string regex = token.str();
		const std::string number = state._results.dollar(parser._gsm, 2,
			state._productions).str();

		state._lrules.push(regex, static_cast<uint16_t>(atoi(number.c_str())));
	};
	g_config_parser._actions[grules.push("rx_rules", "rx_rules regex Literal")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string regex = token.str();
		const std::string literal = state._results.dollar(parser._gsm, 2,
			state._productions).str();

		state._lrules.push(regex,
			state._grules.token_id(literal.c_str()));
	};
	g_config_parser._actions[grules.push("rx_rules", "rx_rules regex Name")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string regex = token.str();
		const std::string name = state._results.dollar(parser._gsm, 2,
			state._productions).str();

		state._lrules.push(regex, state._grules.token_id(name.c_str()));
	};
	g_config_parser._actions[grules.push("rx_rules", "rx_rules regex 'skip()'")] =
		[](config_state &state, config_parser &parser)
	{
		const auto &token = state._results.dollar(parser._gsm, 1,
			state._productions);
		const std::string regex = token.str();

		state._lrules.push(regex, state._lrules.skip());
	};

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

//	parsertl::debug::dump(grules, std::cout);
	parsertl::generator::build(grules, g_config_parser._gsm, &warnings);

	if (!warnings.empty())
	{
		std::cerr << "Config parser warnings: " << warnings;
	}

	lrules.push_state("GRULE");
	lrules.push_state("SCRIPT");
	lrules.push_state("MACRO");
	lrules.push_state("REGEX");
	lrules.push_state("RULE");
	lrules.push_state("ID");
	lrules.insert_macro("posix_name", "alnum|alpha|blank|cntrl|digit|graph|"
		"lower|print|punct|space|upper|xdigit");
	lrules.insert_macro("posix", "\\[:{posix_name}:\\]");
	lrules.insert_macro("escape", "\\\\(.|x[0-9A-Fa-f]+|c[@a-zA-Z])");
	lrules.insert_macro("comment", "[/][*].{+}[\r\n]*?[*][/]");

	lrules.push("[ \t]+", lrules.skip());
	lrules.push("\n|\r\n", grules.token_id("NL"));
	lrules.push("%token", grules.token_id("'%token'"));
	lrules.push("%left", grules.token_id("'%left'"));
	lrules.push("%right", grules.token_id("'%right'"));
	lrules.push("%nonassoc", grules.token_id("'%nonassoc'"));
	lrules.push("%precedence", grules.token_id("'%precedence'"));
	lrules.push("%start", grules.token_id("'%start'"));
	lrules.push("INITIAL", "%%", grules.token_id("'%%'"), "GRULE");

	lrules.push("GRULE", ":", grules.token_id("':'"), ".");
	lrules.push("GRULE", "\\[", grules.token_id("'['"), ".");
	lrules.push("GRULE", "\\]", grules.token_id("']'"), ".");
	lrules.push("GRULE", "[(]", grules.token_id("'('"), ".");
	lrules.push("GRULE", "[)]", grules.token_id("')'"), ".");
	lrules.push("GRULE", "[?]", grules.token_id("'?'"), ".");
	lrules.push("GRULE", "[*]", grules.token_id("'*'"), ".");
	lrules.push("GRULE", "[+]", grules.token_id("'+'"), ".");
	lrules.push("GRULE", "[|]", grules.token_id("'|'"), ".");
	lrules.push("GRULE", ";", grules.token_id("';'"), ".");
	lrules.push("GRULE", "[{]", grules.token_id("'{'"), "SCRIPT");
	lrules.push("SCRIPT", "[}]", grules.token_id("'}'"), "GRULE");
	lrules.push("SCRIPT", "=", grules.token_id("'='"), ".");
	lrules.push("SCRIPT", ",", grules.token_id("','"), ".");
	lrules.push("SCRIPT", "[(]", grules.token_id("'('"), ".");
	lrules.push("SCRIPT", "[)]", grules.token_id("')'"), ".");
	lrules.push("SCRIPT", "[.]", grules.token_id("'.'"), ".");
	lrules.push("SCRIPT", ";", grules.token_id("';'"), ".");
	lrules.push("SCRIPT", "[+]=", grules.token_id("'+='"), ".");
	lrules.push("SCRIPT", "erase", grules.token_id("'erase'"), ".");
	lrules.push("SCRIPT", "first", grules.token_id("'first'"), ".");
	lrules.push("SCRIPT", "insert", grules.token_id("'insert'"), ".");
	lrules.push("SCRIPT", "match", grules.token_id("'match'"), ".");
	lrules.push("SCRIPT", "replace", grules.token_id("'replace'"), ".");
	lrules.push("SCRIPT", "replace_all", grules.token_id("'replace_all'"), ".");
	lrules.push("SCRIPT", "second", grules.token_id("'second'"), ".");
	lrules.push("SCRIPT", "substr", grules.token_id("'substr'"), ".");
	lrules.push("SCRIPT", "\\d+", grules.token_id("Integer"), ".");
	lrules.push("SCRIPT", "\\s+", lrules.skip(), ".");
	lrules.push("SCRIPT", "[$][1-9][0-9]*", grules.token_id("Index"), ".");
	lrules.push("SCRIPT", "'(''|[^'])*'", grules.token_id("ScriptString"), ".");
	lrules.push("GRULE", "[ \t]+|\n|\r\n", lrules.skip(), ".");
	lrules.push("GRULE", "%empty", grules.token_id("'%empty'"), ".");
	lrules.push("GRULE", "%%", grules.token_id("'%%'"), "MACRO");
	lrules.push("INITIAL,GRULE,SCRIPT", "{comment}", lrules.skip(), ".");
	lrules.push("INITIAL,GRULE,ID",
		"'(\\\\([^0-9cx]|[0-9]{1,3}|c[@a-zA-Z]|x\\d+)|[^'])+'|"
		"[\"](\\\\([^0-9cx]|[0-9]{1,3}|c[@a-zA-Z]|x\\d+)|[^\"])+[\"]",
		grules.token_id("Literal"), ".");
	lrules.push("INITIAL,GRULE,ID", "[.A-Z_a-z][-.0-9A-Z_a-z]*",
		grules.token_id("Name"), ".");
	lrules.push("ID", "[1-9][0-9]*", grules.token_id("Number"), ".");

	lrules.push("MACRO,RULE", "%%", grules.token_id("'%%'"), "RULE");
	lrules.push("MACRO", "[A-Z_a-z][0-9A-Z_a-z]*",
		grules.token_id("MacroName"), "REGEX");
	lrules.push("MACRO,REGEX", "\n|\r\n", lrules.skip(), "MACRO");

	lrules.push("REGEX", "[ \t]+", lrules.skip(), ".");
	lrules.push("RULE", "^[ \t]+({comment}([ \t]+|{comment})*)?",
		lrules.skip(), ".");
	lrules.push("REGEX,RULE", "\\^", grules.token_id("'^'"), ".");
	lrules.push("REGEX,RULE", "\\$", grules.token_id("'$'"), ".");
	lrules.push("REGEX,RULE", "[|]", grules.token_id("'|'"), ".");
	lrules.push("REGEX,RULE", "[(]([?](-?(i|s))*:)?",
		grules.token_id("'('"), ".");
	lrules.push("REGEX,RULE", "[)]", grules.token_id("')'"), ".");
	lrules.push("REGEX,RULE", "[?]", grules.token_id("'?'"), ".");
	lrules.push("REGEX,RULE", "[?][?]", grules.token_id("'\?\?'"), ".");
	lrules.push("REGEX,RULE", "[*]", grules.token_id("'*'"), ".");
	lrules.push("REGEX,RULE", "[*][?]", grules.token_id("'*?'"), ".");
	lrules.push("REGEX,RULE", "[+]", grules.token_id("'+'"), ".");
	lrules.push("REGEX,RULE", "[+][?]", grules.token_id("'+?'"), ".");
	lrules.push("REGEX,RULE", "{escape}|(\\[^?({escape}|{posix}|"
		"[^\\\\\\]])*\\])|[^\\s]", grules.token_id("Charset"), ".");
	lrules.push("REGEX,RULE", "[{][A-Z_a-z][-0-9A-Z_a-z]*[}]",
		grules.token_id("Macro"), ".");
	lrules.push("REGEX,RULE", "[{][0-9]+(,([0-9]+)?)?[}][?]?",
		grules.token_id("Repeat"), ".");
	lrules.push("REGEX,RULE", "[\"](\\\\.|[^\r\n\"])*[\"]",
		grules.token_id("String"), ".");

	lrules.push("RULE,ID", "[ \t]+({comment}([ \t]+|{comment})*)?",
		lrules.skip(), "ID");
	lrules.push("RULE,ID", "\n|\r\n", lrules.skip(), "RULE");
	lrules.push("ID", "skip\\s*[(]\\s*[)]", grules.token_id("'skip()'"), "RULE");
	lexertl::generator::build(lrules, g_config_parser._lsm);
}

void show_help()
{
	std::cout << "--help\t\t\tShows help.\n"
		"-checkout <checkout command (include $1 for pathname)>.\n"
		"-E <regex>\t\tSearch using DFA regex.\n"
		"-exclude <wildcard>\tExclude pathname matching wildcard.\n"
		"-f <config file>\tSearch using config file.\n"
		"-i\t\t\tCase insensitive searching.\n"
		"-o\t\t\tUpdate matching file.\n"
		"-P <regex>\t\tSearch using Perl regex.\n"
		"-r, -R, --recursive\tRecurse subdirectories.\n"
		"-replace\t\tReplacement text.\n"
		"-shutdown <command to run when exiting>.\n"
		"-startup <command to run at startup>.\n"
		"-vE <regex>\t\tSearch using DFA regex (negated).\n"
		"-VE <regex>\t\tSearch using DFA regex (all negated).\n"
		"-vf <config file>\tSearch using config file (negated).\n"
		"-Vf <config file>\tSearch using config file (all negated).\n"
		"-vP <regex>\t\tSearch using Perl regex (negated).\n"
		"-VP <regex>\t\tSearch using Perl regex (all negated).\n"
		"<pathname>...\t\tFiles to search (wildcards supported).\n\n"
		"Config file format:\n\n"
		"<grammar directives>\n"
		"%%\n"
		"<grammar>\n"
		"%%\n"
		"<regex macros>\n"
		"%%\n"
		"<regexes>\n"
		"%%\n\n"
		"Grammar Directives:\n\n"
		"%token\n"
		"%left\n"
		"%right\n"
		"%nonassoc\n"
		"%precedence\n"
		"%start\n\n"
		"Grammar scripting:\n\n"
		"match = $n;\n"
		"match += $n;\n"
		"match = substr($n, <omit from left>, <omit from right>);\n"
		"match += substr($n, <omit from left>, <omit from right>);\n\n"
		"Example:\n\n"
		"%token String\n"
		"%%\n"
		"list: String { match = substr($1, 1, 1); };\n"
		"list: list String { match += substr($2, 1, 1); };\n"
		"%%\n"
		"ws [ \\t\\r\\n]+\n"
		"%%\n"
		R"(["]([^"\\]|\\.)*["]                    String)"
		"\n"
		R"('([^'\\]|\\.)*'                        skip())"
		"\n"
		R"({ws}|[/][/].*|[/][*].{+}[\r\n]*?[*][/] skip())"
		"\n"
		"%%\n\n"
		"Note that you can pipeline searches by using multiple switches.\n"
		"The searches are run in the order they occur on the command line.\n";
}

int main(int argc, char *argv[])
{
	try
	{
		if (argc == 1)
		{
			show_help();
			return 1;
		}

		std::vector<config> configs;
		std::string startup;
		std::string shutdown;
		bool run = true;

		for (int i = 1; i < argc; ++i)
		{
			const char *param = argv[i];

			if (strcmp("-checkout", param) == 0)
			{
				++i;

				if (i < argc)
				{
					// "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
					// checkout <pathname>
					g_checkout = argv[i];
				}
				else
				{
					std::cerr << "Missing pathname following -checkout.\n";
					return 1;
				}
			}
			else if (strcmp("-E", param) == 0)
			{
				// DFA regex
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::dfa_regex, argv[i], false, false));
				}
				else
				{
					std::cerr << "Missing regex following -E.\n";
					return 1;
				}
			}
			else if (strcmp("-exclude", param) == 0)
			{
				++i;

				if (i < argc)
				{
					g_exclude.emplace_back(wildcardtl::wildcard(argv[i], true));
				}
				else
				{
					std::cerr << "Missing wildcard following -exclude.\n";
					return 1;
				}
			}
			else if (strcmp("-f", param) == 0)
			{
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::parser, argv[i], false, false));
				}
				else
				{
					std::cerr << "Missing pathname following -f.\n";
					return 1;
				}
			}
			else if (strcmp("--help", param) == 0)
			{
				show_help();
				return 0;
			}
			else if (strcmp("-i", param) == 0)
			{
				g_icase = true;
			}
			else if (strcmp("-o", param) == 0)
			{
				g_output = true;
			}
			else if (strcmp("-P", param) == 0)
			{
				// Perl style regex
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::regex, argv[i], false, false));
				}
				else
				{
					std::cerr << "Missing regex following -P.\n";
					return 1;
				}
			}
			else if (strcmp("-r", param) == 0 || strcmp("-R", param) == 0 ||
				strcmp("--recursive", param) == 0)
			{
				g_recursive = true;
			}
			else if (strcmp("-replace", param) == 0)
			{
				++i;

				if (i < argc)
				{
					g_replace = argv[i];
				}
				else
				{
					std::cerr << "Missing text following -replacement.\n";
					return 1;
				}
			}
			else if (strcmp("-shutdown", param) == 0)
			{
				++i;

				if (i < argc)
				{
					// "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
					// /delete /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep /noprompt
					shutdown = argv[i];
				}
				else
				{
					std::cerr << "Missing pathname following -shutdown.\n";
					return 1;
				}
			}
			else if (strcmp("-startup", param) == 0)
			{
				++i;

				if (i < argc)
				{
					// "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\tf.exe"
					// workspace /new /collection:http://tfssrv01:8080/tfs/PartnerDev gram_grep
					// /noprompt
					startup = argv[i];
				}
				else
				{
					std::cerr << "Missing pathname following -startup.\n";
					return 1;
				}
			}
			else if (strcmp("-vE", param) == 0)
			{
				// DFA regex
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::dfa_regex, argv[i], true, false));
				}
				else
				{
					std::cerr << "Missing regex following -vE.\n";
					return 1;
				}
			}
			else if (strcmp("-VE", param) == 0)
			{
				// DFA regex
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::dfa_regex, argv[i], true, true));
				}
				else
				{
					std::cerr << "Missing regex following -VE.\n";
					return 1;
				}
			}
			else if (strcmp("-vf", param) == 0)
			{
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::parser, argv[i], true, false));
				}
				else
				{
					std::cerr << "Missing pathname following -vf.\n";
					return 1;
				}
			}
			else if (strcmp("-Vf", param) == 0)
			{
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::parser, argv[i], true, true));
				}
				else
				{
					std::cerr << "Missing pathname following -Vf.\n";
					return 1;
				}
			}
			else if (strcmp("-vP", param) == 0)
			{
				// Perl style regex
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::regex, argv[i], true, false));
				}
				else
				{
					std::cerr << "Missing regex following -vP.\n";
					return 1;
				}
			}
			else if (strcmp("-VP", param) == 0)
			{
				// Perl style regex
				++i;

				if (i < argc)
				{
					configs.push_back(config(match_type::regex, argv[i], true, true));
				}
				else
				{
					std::cerr << "Missing regex following -VP.\n";
					return 1;
				}
			}
			else
			{
				g_pathnames.push_back(param);
			}
		}

		if (g_pathnames.empty())
		{
			std::cerr << "No input file specified.\n";
			return 1;
		}

		// Postponed to allow -i to be processed first.
		for (const auto &tuple : configs)
		{
			switch (tuple._type)
			{
				case match_type::dfa_regex:
				{
					lexertl::rules rules;
					lexer lexer;

					lexer._negate = tuple._negate;
					lexer._all = tuple._all;

					if (g_icase)
					{
						rules.flags(lexertl::icase | lexertl::dot_not_cr_lf);
					}

					rules.push(tuple._param, 1);
					rules.push(".{+}[\r\n]", rules.skip());
					lexertl::generator::build(rules, lexer._sm);
					g_pipeline.emplace_back(std::move(lexer));
					break;
				}
				case match_type::regex:
				{
					regex regex;

					regex._negate = tuple._negate;
					regex._all = tuple._all;
					regex._rx.assign(tuple._param, g_icase ?
						(std::regex_constants::ECMAScript |
							std::regex_constants::icase) :
						std::regex_constants::ECMAScript);
					g_pipeline.emplace_back(std::move(regex));
					break;
				}
				case match_type::parser:
				{
					parser parser;
					config_state state;

					parser._negate = tuple._negate;
					parser._all = tuple._all;
					g_curr_parser = &parser;

					if (g_config_parser._gsm.empty())
					{
						build_config_parser();
					}

					state.parse(tuple._param);

					if (parser._gsm.empty())
					{
						lexer lexer;

						lexer._negate = parser._negate;
						lexer._sm.swap(parser._lsm);
						g_pipeline.emplace_back(std::move(lexer));
					}
					else
					{
						g_pipeline.emplace_back(std::move(parser));
					}

					break;
				}
			}
		}

		if (g_pipeline.empty())
		{
			std::cerr << "No actions have been specified.\n";
			return 1;
		}

		if (!g_replace.empty() && g_modify)
		{
			std::cerr << "Cannot combine -replace with grammar actions "
				"that modify the input.\n";
			return 1;
		}

		if (!startup.empty())
		{
			if (::system(startup.c_str()))
			{
				std::cerr << "Failed to execute " << startup << '\n';
				run = false;
			}
		}

		if (run)
		{
			process();
		}

		if (!shutdown.empty())
		{
			if (::system(shutdown.c_str()))
			{
				std::cerr << "Failed to execute " << shutdown << '\n';
			}
		}

		std::cout << "Matches: " << g_hits << "    Matching files: " << g_files <<
			"    Total files searched: " << g_searched << '\n';
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << '\n';
	}

	return 0;
}
