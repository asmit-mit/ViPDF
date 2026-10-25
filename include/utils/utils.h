#pragma once

#include <cstdint>
#include <filesystem>

#include <SFML/Graphics.hpp>

namespace utils {

const float padding = 8.f;
const float char_size = 16.f;
const float cmdline_height_ = 24.f;

inline sf::Color hexToRGB(const uint32_t hex) {
  return sf::Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

inline sf::Color hexToRGBA(const uint32_t hex) {
  return sf::Color((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

inline std::string resolvePath(std::string path) {
  if (path.starts_with("~/")) {
    if (const char *home = std::getenv("HOME"))
      path = (std::filesystem::path(home) / path.substr(2)).string();
  }

  return path;
}

inline bool isNumber(const std::string &s) {
  return !s.empty() &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

inline std::filesystem::path ensureParent(const std::filesystem::path path) {
  namespace fs = std::filesystem;

  const fs::path parent = path.parent_path();
  if (parent.empty())
    return path;

  std::error_code ec;
  fs::create_directories(parent, ec);
  if (ec)
    throw std::runtime_error("Cannot create " + parent.string() + ": " + ec.message());

  return path;
}

} // namespace utils
