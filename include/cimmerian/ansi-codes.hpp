#pragma once

// ── ANSI constants ────────────────────────────────────────────────────────

namespace Cimmerian::Ansi {

static constexpr const char* ANSI_RESET = "\x1b[0m";

static constexpr const char* ANSI_COLOR_RED = "\x1b[31m";
static constexpr const char* ANSI_COLOR_GREEN = "\x1b[32m";
static constexpr const char* ANSI_COLOR_YELLOW = "\x1b[33m";
static constexpr const char* ANSI_COLOR_BLUE = "\x1b[34m";
static constexpr const char* ANSI_COLOR_MAGENTA = "\x1b[35m";
static constexpr const char* ANSI_COLOR_CYAN = "\x1b[36m";
static constexpr const char* ANSI_COLOR_WHITE = "\x1b[37m";

static constexpr const char* ANSI_COLOR_BRIGHT_RED = "\x1b[91m";
static constexpr const char* ANSI_COLOR_BRIGHT_GREEN = "\x1b[92m";
static constexpr const char* ANSI_COLOR_BRIGHT_YELLOW = "\x1b[93m";
static constexpr const char* ANSI_COLOR_BRIGHT_BLUE = "\x1b[94m";
static constexpr const char* ANSI_COLOR_BRIGHT_MAGENTA = "\x1b[95m";
static constexpr const char* ANSI_COLOR_BRIGHT_CYAN = "\x1b[96m";
static constexpr const char* ANSI_COLOR_BRIGHT_WHITE = "\x1b[97m";

static constexpr const char* ANSI_DECORATION_BOLD = "\x1b[1m";
static constexpr const char* ANSI_DECORATION_DIM = "\x1b[2m";
static constexpr const char* ANSI_DECORATION_ITALIC = "\x1b[3m";
static constexpr const char* ANSI_DECORATION_UNDERLINE = "\x1b[4m";
static constexpr const char* ANSI_DECORATION_STRIKETHROUGH = "\x1b[9m";
} // namespace Cimmerian::Ansi
