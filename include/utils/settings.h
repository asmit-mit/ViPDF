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

constexpr bool default_recolor = true;
constexpr uint32_t recolor_light = 0x282828;
constexpr uint32_t recolor_dark = 0xebdbb2;

constexpr float delta_zoom_ = 0.1f;
constexpr std::size_t display_list_cache_size_ = 200;
constexpr std::size_t page_cache_size_ = 15;
constexpr std::size_t thread_count_ = 5;

constexpr char font_regular[] = "./config/fonts/JetBrainsMonoNerdFont-Regular.ttf";
constexpr char font_bold[] = "./config/fonts/JetBrainsMonoNerdFont-Bold.ttf";
constexpr char font_italic[] = "./config/fonts/JetBrainsMonoNerdFont-Italic.ttf";
constexpr char font_emoji[] = "./config/fonts/NotoEmoji-Regular.ttf";
constexpr char font_cjk[] = "./config/fonts/NotoSansMonoCJK-VF.ttc";
constexpr char commands_json[] = "./config/commands.json";

} // namespace settings
