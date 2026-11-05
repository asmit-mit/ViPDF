#include "core/config_reader.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <thread>

namespace core {

namespace {

std::string trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

bool parse_color(std::string s, uint32_t &out) {
  s = trim(std::move(s));
  if (!s.empty() && s.front() == '#')
    s.erase(0, 1);
  else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    s.erase(0, 2);

  if (s.empty() || s.size() > 8)
    return false;
  if (!std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c); }))
    return false;

  out = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
  return true;
}

} // namespace

ConfigReader::ConfigReader(const fs::path &path)
    : MAX_THREADS(std::thread::hardware_concurrency()) {
  if (!fs::exists(path))
    return;

  try {
    ini_.load(path.string());
    loadConfig();
  } catch (const std::exception &) {
    std::cerr << "Error while reading config.ini file\n";
  }
}

const Config &ConfigReader::getConfig() const {
  return config_;
}

void ConfigReader::loadConfig() {
  config_.app_background_color = get_color("app", "background", config_.app_background_color);

  config_.cmdline_background_color =
      get_color("cmdline", "background", config_.cmdline_background_color);
  config_.cmdline_text_color = get_color("cmdline", "text", config_.cmdline_text_color);
  config_.cmdline_textbox_highlight_color =
      get_color("cmdline", "textbox_highlight", config_.cmdline_textbox_highlight_color);
  config_.cmdline_highlight_text_color =
      get_color("cmdline", "highlight_text", config_.cmdline_highlight_text_color);

  config_.completions_background_color =
      get_color("completions", "background", config_.completions_background_color);
  config_.completions_left_text_color =
      get_color("completions", "left_text", config_.completions_left_text_color);
  config_.completions_right_text_color =
      get_color("completions", "right_text", config_.completions_right_text_color);
  config_.completions_highlight_color =
      get_color("completions", "highlight", config_.completions_highlight_color);

  config_.notification_background_idle_color =
      get_color("notification", "background_idle", config_.notification_background_idle_color);
  config_.notification_background_hover_color =
      get_color("notification", "background_hover", config_.notification_background_hover_color);
  config_.notification_outline_color =
      get_color("notification", "outline", config_.notification_outline_color);
  config_.notification_heading_text_color =
      get_color("notification", "heading_text", config_.notification_heading_text_color);
  config_.notification_msg_text_color =
      get_color("notification", "msg_text", config_.notification_msg_text_color);

  config_.password_dialog_background_color =
      get_color("password_dialog", "background", config_.password_dialog_background_color);
  config_.password_dialog_outline_color =
      get_color("password_dialog", "outline", config_.password_dialog_outline_color);
  config_.password_dialog_textbox_background_color =
      get_color("password_dialog", "textbox_background", config_.password_dialog_textbox_background_color);
  config_.password_dialog_textbox_outline_color =
      get_color("password_dialog", "textbox_outline", config_.password_dialog_textbox_outline_color);
  config_.password_dialog_text_color =
      get_color("password_dialog", "text", config_.password_dialog_text_color);
  config_.password_dialog_textbox_highlight_color =
      get_color("password_dialog", "textbox_highlight", config_.password_dialog_textbox_highlight_color);
  config_.password_dialog_highlight_text_color =
      get_color("password_dialog", "highlight_text", config_.password_dialog_highlight_text_color);

  config_.statusbar_background_colo =
      get_color("statusbar", "background", config_.statusbar_background_colo);
  config_.statusbar_text_color = get_color("statusbar", "text", config_.statusbar_text_color);

  config_.selection_highlight_color =
      get_color("highlights", "selection", config_.selection_highlight_color);
  config_.search_highlight_color = get_color("highlights", "search", config_.search_highlight_color);
  config_.search_box_color = get_color("highlights", "search_box", config_.search_box_color);
  config_.highlight_color_1 = get_color("highlights", "color_1", config_.highlight_color_1);
  config_.highlight_color_2 = get_color("highlights", "color_2", config_.highlight_color_2);
  config_.highlight_color_3 = get_color("highlights", "color_3", config_.highlight_color_3);
  config_.highlight_color_4 = get_color("highlights", "color_4", config_.highlight_color_4);
  config_.highlight_color_5 = get_color("highlights", "color_5", config_.highlight_color_5);

  config_.default_recolor = get_bool("recolor", "default_recolor", config_.default_recolor);
  config_.recolor_light = get_color("recolor", "light", config_.recolor_light);
  config_.recolor_dark = get_color("recolor", "dark", config_.recolor_dark);

  config_.delta_zoom = get_float("general", "delta_zoom", config_.delta_zoom);
  config_.page_cache_size =
      std::max(MIN_CACHE_SIZE, get_size("general", "page_cache_size", config_.page_cache_size));
  config_.thread_count =
      std::clamp(get_size("general", "thread_count", config_.thread_count), MIN_THREADS, MAX_THREADS);
}

bool ConfigReader::lookup(
    const std::string &section, const std::string &key, std::string &out
) const {
  auto sec = ini_.find(section);
  if (sec == ini_.end())
    return false;

  auto opt = sec->second.find(key);
  if (opt == sec->second.end())
    return false;

  try {
    out = trim(opt->second.as<std::string>());
  } catch (const std::exception &) {
    return false;
  }

  return !out.empty();
}

uint32_t ConfigReader::get_color(
    const std::string &section, const std::string &key, uint32_t fallback
) const {
  std::string raw;
  uint32_t value;
  if (lookup(section, key, raw) && parse_color(raw, value))
    return value;
  return fallback;
}

bool ConfigReader::get_bool(
    const std::string &section, const std::string &key, bool fallback
) const {
  std::string raw;
  if (!lookup(section, key, raw))
    return fallback;

  std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
  if (raw == "true" || raw == "1" || raw == "yes" || raw == "on")
    return true;
  if (raw == "false" || raw == "0" || raw == "no" || raw == "off")
    return false;
  return fallback;
}

float ConfigReader::get_float(
    const std::string &section, const std::string &key, float fallback
) const {
  std::string raw;
  if (!lookup(section, key, raw))
    return fallback;
  try {
    return std::stof(raw);
  } catch (const std::exception &) {
    return fallback;
  }
}

std::size_t ConfigReader::get_size(
    const std::string &section, const std::string &key, std::size_t fallback
) const {
  std::string raw;
  if (!lookup(section, key, raw))
    return fallback;
  try {
    return static_cast<std::size_t>(std::stoull(raw));
  } catch (const std::exception &) {
    return fallback;
  }
}

std::string ConfigReader::get_string(
    const std::string &section, const std::string &key, const std::string &fallback
) const {
  std::string raw;
  return lookup(section, key, raw) ? raw : fallback;
}

} // namespace core
