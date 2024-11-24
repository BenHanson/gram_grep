#pragma once

#include <iostream>
#include "types.hpp"

extern bool g_ne;
extern options g_options;

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
void output_text(std::basic_ostream<CharT, Traits>& os,
	const CharT* szColour, const std::basic_string_view<CharT> text)
{
	if (g_options._colour)
	{
		os << szColour;

		if (!g_ne)
			os << szEraseEOL;
	}

	os << text;

	if (g_options._colour)
	{
		std::cerr << szDefaultText;

		if (!g_ne)
			std::cout << szEraseEOL;
	}
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os,
	const CharT* szColour, const std::basic_string<CharT>& text)
{
	output_text(os, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text(std::basic_ostream<CharT, Traits>& os,
	const CharT* szColour, const CharT* text)
{
	output_text(os, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os,
	const CharT* szColour, const std::basic_string_view<CharT> text)
{
	output_text(os, szColour, text);
	os << output_nl;
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os,
	const CharT* szColour, const CharT* text)
{
	output_text_nl(os, szColour, std::string_view(text));
}

template<class CharT, class Traits>
void output_text_nl(std::basic_ostream<CharT, Traits>& os,
	const CharT* szColour, const std::basic_string<CharT>& text)
{
	output_text_nl(os, szColour, std::string_view(text));
}
