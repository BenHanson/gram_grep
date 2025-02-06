#pragma once

#include "colours.hpp"
#include "types.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>

extern options g_options;

bool is_a_tty(FILE* fd);

const char* gg_text();

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& output_gg(std::basic_ostream<CharT, Traits>& os)
{
	os << gg_text();
	return os;
}

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& output_nl(std::basic_ostream<CharT, Traits>& os)
{
	if (g_options._print_null)
		os << '\0';
	else
		os << '\n';

	if (g_options._line_buffered)
		os << std::flush;

	return os;
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string_view<CharT> text)
{
	if (g_options._colour && tty)
	{
		os << szColour;

		if (!g_options._ne)
			os << szEraseEOL;
	}

	os << text;

	if (g_options._colour && tty)
	{
		os << szDefaultText;

		if (!g_options._ne)
			os << szEraseEOL;
	}
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string<CharT>& text)
{
	output_text(os, tty, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const CharT* text)
{
	output_text(os, tty, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string_view<CharT> text)
{
	output_text(os, tty, szColour, text);
	os << output_nl;
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const CharT* text)
{
	output_text_nl(os, tty, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os, const bool tty,
	const CharT* szColour, const std::basic_string<CharT>& text)
{
	output_text_nl(os, tty, szColour, std::string_view(text));
}
