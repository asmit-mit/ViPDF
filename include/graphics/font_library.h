#pragma once

#include <ft2build.h>
#include <hb-ft.h>
#include <hb.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace graphics {

enum FontType : int {
  Regular = 0,
  Bold = 1,
  Italic = 2,
  Emoji = 3,
};

struct Font {
  FT_Face ft_face = nullptr;
  hb_font_t *hb_font = nullptr;
};

class FontLibrary {
public:
  FontLibrary(const std::vector<const char *> &paths);

  FontLibrary(const FontLibrary &) = delete;
  FontLibrary &operator=(const FontLibrary &) = delete;

  FontLibrary(FontLibrary &&) = delete;
  FontLibrary &operator=(FontLibrary &&) = delete;

  ~FontLibrary();

  int getFontTypeForCodepoint(uint32_t codepoint, int default_type = FontType::Regular) const;
  Font *getFont(int type, uint32_t pixel_size) const;

  bool empty() const;
  std::size_t size() const;

private:
  struct FontKey {
    int type;
    uint32_t size;

    bool operator==(const FontKey &other) const {
      return type == other.type && size == other.size;
    }
  };

  struct FontKeyHash {
    std::size_t operator()(const FontKey &key) const {
      std::size_t seed = 0;

      auto hashCombine = [&seed](std::size_t value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      };

      hashCombine(std::hash<int>{}(static_cast<int>(key.type)));
      hashCombine(std::hash<uint32_t>{}(key.size));

      return seed;
    }
  };

private:
  void tryLoadFont(int type, const char *path);
  bool hasGlyph(int type, uint32_t codepoint) const;
  int findSystemFallback(uint32_t codepoint, int style) const;
  int addFallbackFont(const std::string &path, int face_index) const;
  bool queryFontconfig(uint32_t codepoint, int style, std::string &out_path, int &out_index) const;
  void destroy();

private:
  static constexpr int BUILTIN_SLOTS = 4;

  FT_Library library_ = nullptr;

  mutable std::vector<FT_Face> ft_fonts_{};
  mutable std::vector<std::string> fonts_paths_{};
  mutable std::vector<int> fonts_indices_{};

  mutable std::unordered_map<uint32_t, int> fallback_cache_;
  mutable std::unordered_map<FontKey, Font, FontKeyHash> cache_;

  mutable std::mutex mutex_;
  bool is_empty_ = true;
};

} // namespace graphics
