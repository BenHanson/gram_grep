#include "pch.h"

#include <stdio.h>
#if _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "output.hpp"

bool is_a_tty(FILE* fd)
{
#ifdef _WIN32
	return _isatty(_fileno(fd));
#else
	return isatty(fileno(fd));
#endif
}
