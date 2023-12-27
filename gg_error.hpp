#pragma once

#include <stdexcept>

class gg_error : public std::runtime_error
{
public:
	gg_error(const std::string& msg) :
		std::runtime_error(msg)
	{
	}
};
