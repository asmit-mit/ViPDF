#pragma once

#include <cstdint>

namespace settings {

constexpr uint32_t bg_ = 0x1d2021;
constexpr uint32_t fg_ = 0xebdfc3;
constexpr uint32_t cmd_bg_ = 0x3c3836;
constexpr uint32_t status_bg_ = 0x282828;

constexpr uint32_t completions_desc_fg_ = 0xd5c7a2;
constexpr uint32_t completion_highlight_bg_ = 0x458588;

constexpr uint32_t scrollwheel_color_ = 0x458588;
constexpr uint32_t textbox_highlight_fg_ = 0x7daea3;

constexpr uint32_t notification_outline_bg_ = 0x5b554d;
constexpr uint32_t notification_hover_bg_ = 0x3c3836;

constexpr uint32_t selection_box_color_ = 0xADFF2F50;
constexpr uint32_t search_box_color_ = 0xFFD70050;
constexpr uint32_t search_highlight_color_ = 0xADFF2F66;

constexpr uint32_t highlight_color_yellow_ = 0xf5c211;
constexpr uint32_t highlight_color_orange_ = 0xff7800;
constexpr uint32_t highlight_color_red_ = 0xed333b;
constexpr uint32_t highlight_color_blue_ = 0x3584e4;
constexpr uint32_t highlight_color_green_ = 0x33d17a;

constexpr bool default_recolor = true;
constexpr uint32_t recolor_light = 0x282828;
constexpr uint32_t recolor_dark = 0xebdbb2;

constexpr int status_font_size_ = 16;

constexpr float delta_zoom_ = 0.1f;
constexpr std::size_t display_list_cache_size_ = 200;
constexpr std::size_t page_cache_size_ = 15;
constexpr std::size_t thread_count_ = 5;

constexpr char font_regular[] = "fonts/JetBrainsMonoNLNerdFont-Regular.ttf";
constexpr char font_bold[] = "fonts/JetBrainsMonoNLNerdFont-Bold.ttf";
constexpr char font_italic[] = "fonts/JetBrainsMonoNLNerdFont-Italic.ttf";
constexpr char font_emoji[] = "fonts/NotoEmoji-Regular.ttf";
constexpr char font_cjk[] = "fonts/NotoSansMonoCJK-VF.ttc";
constexpr char default_commands[] = "commands.json";

constexpr char commands_json[] = "commands.json";
constexpr char cmd_history[] = "history/cmd_history";
constexpr char recent_files[] = "history/recent_files";
constexpr char search_history[] = "history/search_history";
constexpr char bookmarks_db[] = "bookmarks/bookmarks.db3";

constexpr char app_name[] = "ViPDF";

} // namespace settings
