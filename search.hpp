#pragma once

#include <map>
#include <stack>
#include "types.hpp"
#include <utility>
#include <vector>

std::pair<bool, bool> search(match_data& data,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements);
