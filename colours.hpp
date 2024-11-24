#pragma once

#include <string>

constexpr const char szDarkRedText[] = "\x1b[31m";
inline std::string g_bn_text = "\x1b[32m";
inline std::string g_fn_text = "\x1b[35m";
inline std::string g_ln_text = "\x1b[32m";
inline std::string g_ms_text = "\x1b[01;91m";
inline std::string g_se_text = "\x1b[36m";
inline std::string g_sl_text;
constexpr const char szYellowText[] = "\x1b[01;33m";
constexpr const char szEraseEOL[] = "\x1b[K";
constexpr const char szDefaultText[] = "\x1b[m";
