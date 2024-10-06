#pragma once

#include <map>
#include <stack>
#include "types.hpp"
#include <utility>
#include <vector>

std::pair<bool, bool> search(std::vector<match>& ranges, const char* data_first,
    std::stack<std::string>& matches,
    std::map<std::pair<std::size_t, std::size_t>, std::string>& replacements,
    capture_vector& captures);
