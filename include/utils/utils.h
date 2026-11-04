#pragma once

#include <clip.h>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mupdf/fitz.h>
#include <pwd.h>
#include <unistd.h>

#include "vipdf_config.h"
#include <SFML/Graphics.hpp>

namespace utils {

namespace fs = std::filesystem;

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

static inline bool copyImageToClipboard(fz_context *ctx, fz_image *img) {
  if (!ctx || !img)
    return false;

  fz_pixmap *pix = nullptr;
  fz_pixmap *rgb = nullptr;
  std::vector<uint8_t> rgba;
  int width = 0, height = 0;
  bool ok = false;

  fz_var(pix);
  fz_var(rgb);

  fz_try(ctx) {
    pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);

    const int has_alpha = fz_pixmap_alpha(ctx, pix);
    rgb =
        fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), nullptr, nullptr, fz_default_color_params, has_alpha);

    width = fz_pixmap_width(ctx, rgb);
    height = fz_pixmap_height(ctx, rgb);
    const int stride = fz_pixmap_stride(ctx, rgb);
    const int n = fz_pixmap_components(ctx, rgb);
    const unsigned char *samples = fz_pixmap_samples(ctx, rgb);

    rgba.resize(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y) {
      const unsigned char *src = samples + static_cast<size_t>(y) * stride;
      uint8_t *dst = rgba.data() + static_cast<size_t>(y) * width * 4;
      for (int x = 0; x < width; ++x) {
        dst[4 * x + 0] = src[n * x + 0];
        dst[4 * x + 1] = src[n * x + 1];
        dst[4 * x + 2] = src[n * x + 2];
        dst[4 * x + 3] = (n == 4) ? src[n * x + 3] : 255;
      }
    }
    ok = true;
  }
  fz_always(ctx) {
    fz_drop_pixmap(ctx, rgb);
    fz_drop_pixmap(ctx, pix);
  }
  fz_catch(ctx) {
    std::cerr << "Error copying image: " << fz_caught_message(ctx) << "\n";
  }

  if (!ok || width <= 0 || height <= 0)
    return false;

  clip::image_spec spec;
  spec.width = width;
  spec.height = height;
  spec.bits_per_pixel = 32;
  spec.bytes_per_row = width * 4;
  spec.red_mask = 0x000000ff;
  spec.red_shift = 0;
  spec.green_mask = 0x0000ff00;
  spec.green_shift = 8;
  spec.blue_mask = 0x00ff0000;
  spec.blue_shift = 16;
  spec.alpha_mask = 0xff000000;
  spec.alpha_shift = 24;

  clip::image clip_img(rgba.data(), spec);
  return clip::set_image(clip_img);
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

inline static fs::path configDir() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg && fs::path(xdg).is_absolute())
    return fs::path(xdg) / "vipdf";

  if (fs::path home = homeDir(); !home.empty())
    return home / ".config/vipdf";

  return fs::temp_directory_path() / "vipdf";
}

inline static fs::path configPath() {
  return configDir() / "config.ini";
}

} // namespace utils
