#pragma once

#include <cstddef>

namespace utils {

constexpr float PADDING = 8.f;
constexpr float TEXT_SIZE = 16.f;
constexpr float CMDLINE_HEIGHT = 24.f;
constexpr float OUTLINE_THICKNESS = 2.f;

constexpr std::size_t DISPLAY_LIST_CACHE_SIZE = 200;

constexpr char FONT_REGULAR[] = "fonts/JetBrainsMonoNLNerdFont-Regular.ttf";
constexpr char FONT_BOLD[] = "fonts/JetBrainsMonoNLNerdFont-Bold.ttf";
constexpr char FONT_ITALITC[] = "fonts/JetBrainsMonoNLNerdFont-Italic.ttf";
constexpr char FONT_EMOJI[] = "fonts/NotoEmoji-Regular.ttf";
constexpr char FONT_CJK[] = "fonts/NotoSansMonoCJK-VF.ttc";

constexpr char DEFAULT_COMMANDS[] = "commands.json";
constexpr char DEFAULT_CONFIG[] = "config.ini";

constexpr char CMD_HISTORY[] = "history/cmd_history";
constexpr char RECENT_FILES[] = "history/recent_files";
constexpr char SEARCH_HISTORY[] = "history/search_history";
constexpr char BOOKMARKS_DB[] = "bookmarks/bookmarks.db3";

} // namespace utils
