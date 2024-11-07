#pragma once

#include <string>

constexpr const char szDarkRedText[] = "\x1b[31m\x1b[K";
inline std::string g_bn_text = "\x1b[92m\x1b[K";
inline std::string g_fn_text = "\x1b[38;5;141m\x1b[K";
inline std::string g_ln_text = "\x1b[92m\x1b[K";
inline std::string g_ms_text = "\x1b[01;91m\x1b[K";
inline std::string g_se_text = "\x1b[36m\x1b[K";
constexpr const char szYellowText[] = "\x1b[38;5;193m\x1b[K";
constexpr const char szDefaultText[] = "\x1b[m\x1b[K";
