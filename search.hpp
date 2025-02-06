#pragma once

#include "types.hpp"

#include <map>
#include <string>
#include <utility>

bool search(match_data& data,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements);
