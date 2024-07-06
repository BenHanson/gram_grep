#pragma once

#include <string>
#include "types.hpp"
#include <vector>

std::vector<std::string_view> split(const char* str, const char c);
bool is_windows();
void read_switches(const int argc, const char* const argv[],
    std::vector<config>& configs, std::vector<std::string>& files);
void show_help();
