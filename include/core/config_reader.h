#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <inicpp.h>
#include <string>

namespace core {

namespace fs = std::filesystem;

struct Config {
  uint32_t app_background_color = 0x1d2021; // #1d2021

  uint32_t cmdline_background_color = 0x3c3836;        // #3c3836
  uint32_t cmdline_text_color = 0xebdfc3;              // #ebdfc3
  uint32_t cmdline_textbox_highlight_color = 0x7daea3; // #7daea3
  uint32_t cmdline_highlight_text_color = 0x3c3836;    // #3c3836

  uint32_t completions_background_color = 0x282828; // #282828
  uint32_t completions_left_text_color = 0xebdfc3;  // #ebdfc3
  uint32_t completions_right_text_color = 0xd5c7a2; // #d5c7a2
  uint32_t completions_highlight_color = 0x458588;  // #458588

  uint32_t notification_background_idle_color = 0x1d2021;  // #1d2021
  uint32_t notification_background_hover_color = 0x3c3836; // #3c3836
  uint32_t notification_outline_color = 0x5b554d;          // #5b554d
  uint32_t notification_heading_text_color = 0xebdfc3;     // #ebdfc3
  uint32_t notification_msg_text_color = 0xebdfc3;         // #ebdfc3

  uint32_t password_dialog_background_color = 0x1d2021;         // #1d2021
  uint32_t password_dialog_outline_color = 0x5b554d;            // #5b554d
  uint32_t password_dialog_textbox_background_color = 0x282828; // #282828
  uint32_t password_dialog_textbox_outline_color = 0x5b554d;    // #5b554d
  uint32_t password_dialog_text_color = 0xebdfc3;               // #ebdfc3
  uint32_t password_dialog_textbox_highlight_color = 0x7daea3;  // #7daea3
  uint32_t password_dialog_highlight_text_color = 0x3c3836;     // #3c3836

  uint32_t statusbar_background_colo = 0x282828; // #282828
  uint32_t statusbar_text_color = 0xebdfc3;      // #ebdfc3

  uint32_t selection_highlight_color = 0x0078D74D; // #0078d7
  uint32_t search_highlight_color = 0xADFF2F66;    // #ADFF2F
  uint32_t search_box_color = 0xFFD70050;          // #FFD700

  bool default_recolor = true;
  uint32_t recolor_light = 0x282828;
  uint32_t recolor_dark = 0xebdbb2;

  uint32_t highlight_color_1 = 0xf5c211; // #f5c211
  uint32_t highlight_color_2 = 0xff7800; // #ff7800
  uint32_t highlight_color_3 = 0xed333b; // #ed333b
  uint32_t highlight_color_4 = 0x3584e4; // #3584e4
  uint32_t highlight_color_5 = 0x33d17a; // #33d17a

  float delta_zoom = 0.1f;
  std::size_t page_cache_size = 15;
  std::size_t thread_count = 5;

  std::size_t cmd_history_count = 1000;
  std::size_t file_history_count = 10;
  std::size_t search_history_count = 1000;
};

class ConfigReader {
public:
  explicit ConfigReader(const fs::path &path);

  const Config &getConfig() const;

private:
  void loadConfig();

  uint32_t get_color(const std::string &section, const std::string &key, uint32_t fallback) const;
  bool get_bool(const std::string &section, const std::string &key, bool fallback) const;
  float get_float(const std::string &section, const std::string &key, float fallback) const;
  std::size_t
  get_size(const std::string &section, const std::string &key, std::size_t fallback) const;
  std::string
  get_string(const std::string &section, const std::string &key, const std::string &fallback) const;

  bool lookup(const std::string &section, const std::string &key, std::string &out) const;

private:
  ini::IniFile ini_;
  Config config_;

  static constexpr std::size_t MIN_CACHE_SIZE = 8;
  static constexpr std::size_t MIN_THREADS = 1;
  const std::size_t MAX_THREADS;
};

} // namespace core
