#pragma once

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <pwd.h>
#include <unistd.h>

#include "vipdf_config.h"

namespace utils {

namespace fs = std::filesystem;

inline std::string resolvePath(std::string path) {
  if (path.starts_with("~/")) {
    if (const char *home = std::getenv("HOME"))
      path = (fs::path(home) / path.substr(2)).string();
  }
  return path;
}

inline fs::path ensureParent(const fs::path path) {
  const fs::path parent = path.parent_path();
  if (parent.empty())
    return path;

  std::error_code ec;
  fs::create_directories(parent, ec);
  if (ec)
    throw std::runtime_error("Cannot create " + parent.string() + ": " + ec.message());

  return path;
}

inline std::string uriToPath(const std::string &uri) {
  const std::string prefix = "file://";
  if (uri.rfind(prefix, 0) != 0)
    return uri;

  const std::string encoded = uri.substr(prefix.size());
  std::string path;
  path.reserve(encoded.size());

  for (size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.size() &&
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

inline fs::path homeDir() {
  if (const char *h = std::getenv("HOME"); h && *h)
    return h;
  if (const passwd *pw = getpwuid(getuid()); pw && pw->pw_dir && *pw->pw_dir)
    return pw->pw_dir;
  return {};
}

inline fs::path stateDir() {
  if (const char *xdg = std::getenv("XDG_STATE_HOME"); xdg && *xdg && fs::path(xdg).is_absolute())
    return fs::path(xdg) / "vipdf";

  if (fs::path home = homeDir(); !home.empty())
    return home / ".local/state/vipdf";

  return fs::temp_directory_path() / "vipdf";
}

inline fs::path configDir() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg && fs::path(xdg).is_absolute())
    return fs::path(xdg) / "vipdf";

  if (fs::path home = homeDir(); !home.empty())
    return home / ".config/vipdf";

  return fs::temp_directory_path() / "vipdf";
}

inline fs::path configPath() {
  return configDir() / "config.ini";
}

inline fs::path assetDir() {
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
