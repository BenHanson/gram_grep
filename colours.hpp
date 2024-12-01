#pragma once

#include <string>

constexpr const char szDarkRedText[] = "\x1b[31m";
inline std::string g_bn_text = "\x1b[32m";
inline std::string g_fn_text = "\x1b[35m";
inline std::string g_ln_text = "\x1b[32m";
inline std::string g_ms_text = "\x1b[01;31m";
inline std::string g_se_text = "\x1b[36m";
inline std::string g_sl_text;
inline std::string g_wa_text = "\x1b[38;5;229m";
constexpr const char szEraseEOL[] = "\x1b[K";
constexpr const char szDefaultText[] = "\x1b[m";
