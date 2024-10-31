#pragma once

#include <iostream>

extern bool g_line_buffered;
extern bool g_null_data;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& output_nl(std::basic_ostream<CharT, Traits>& os)
{
	if (g_null_data)
		os << '\0';
	else
		os << '\n';

	if (g_line_buffered)
		os << std::flush;

	return os;
}
