#pragma once

#include <cstdint>
#include <filesystem>
#include <pwd.h>
#include <unistd.h>

#include "vipdf_config.h"
#include <SFML/Graphics.hpp>

namespace utils {

namespace fs = std::filesystem;

const float padding = 8.f;
const float char_size = 16.f;
const float cmdline_height_ = 24.f;

inline static sf::Color hexToRGB(const uint32_t hex) {
  return sf::Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

inline static sf::Color hexToRGBA(const uint32_t hex) {
  return sf::Color((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

inline static std::string resolvePath(std::string path) {
  if (path.starts_with("~/")) {
    if (const char *home = std::getenv("HOME"))
      path = (fs::path(home) / path.substr(2)).string();
  }

  return path;
}

inline static bool isNumber(const std::string &s) {
  return !s.empty() &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

inline static fs::path ensureParent(const fs::path path) {

  const fs::path parent = path.parent_path();
  if (parent.empty())
    return path;

  std::error_code ec;
  fs::create_directories(parent, ec);
  if (ec)
    throw std::runtime_error("Cannot create " + parent.string() + ": " + ec.message());

  return path;
}

inline static std::string uriToPath(const std::string &uri) {
  const std::string prefix = "file://";
  if (uri.rfind(prefix, 0) != 0)
    return uri;

  std::string encoded = uri.substr(prefix.size());
  std::string path;
  path.reserve(encoded.size());

  for (size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.size() + 0 &&
        std::isxdigit(static_cast<unsigned char>(encoded[i + 1])) &&
        std::isxdigit(static_cast<unsigned char>(encoded[i + 2]))) {
      path += static_cast<char>(std::stoi(encoded.substr(i + 1, 2), nullptr, 16));
      i += 2;
    } else {
      path += encoded[i];
    }
  }
  return path;
}

inline static fs::path homeDir() {
  if (const char *h = std::getenv("HOME"); h && *h)
    return h;
  if (const passwd *pw = getpwuid(getuid()); pw && pw->pw_dir && *pw->pw_dir)
    return pw->pw_dir;
  return {};
}

inline static fs::path stateDir() {
  if (const char *xdg = std::getenv("XDG_STATE_HOME"); xdg && *xdg && fs::path(xdg).is_absolute())
    return fs::path(xdg) / "vipdf";

  if (fs::path home = homeDir(); !home.empty())
    return home / ".local/state/vipdf";

  return fs::temp_directory_path() / "vipdf";
}

inline static fs::path assetDir() {
  if (const char *env = std::getenv("VIPDF_ASSETS"); env && *env)
    return env;

  if (fs::exists(fs::path(VIPDF_DATADIR) / "fonts"))
    return VIPDF_DATADIR;

  std::error_code ec;
  fs::path exe = fs::read_symlink("/proc/self/exe", ec);
  if (!ec) {
    fs::path rel = exe.parent_path().parent_path() / "share/vipdf";
    if (fs::exists(rel / "fonts"))
      return rel;
  }

  throw std::runtime_error("Cannot find vipdf assets (set VIPDF_ASSETS)");
}

} // namespace utils
