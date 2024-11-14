#pragma once

#include <iostream>

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
