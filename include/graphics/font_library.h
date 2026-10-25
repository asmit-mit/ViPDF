#pragma once

#include <ft2build.h>
#include <hb-ft.h>
#include <hb.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace graphics {

enum FontType : int {
  Regular = 0,
  Bold = 1,
  Italic = 2,
  Emoji = 3,
  CJK = 4,
};

struct Font {
  FT_Face ft_face = nullptr;
  hb_font_t *hb_font = nullptr;
};

class FontLibrary {
public:
  FontLibrary();
  FontLibrary(const std::vector<std::pair<FontType, const char *>> &paths);

  FontLibrary(const FontLibrary &) = delete;
  FontLibrary &operator=(const FontLibrary &) = delete;

  FontLibrary(FontLibrary &&) = delete;
  FontLibrary &operator=(FontLibrary &&) = delete;

  ~FontLibrary();

  void tryLoadFont(FontType type, const char *path);
  FontType
  getFontTypeForCodepoint(uint32_t codepoint, FontType default_type = FontType::Regular) const;
  Font *getFont(FontType type, uint32_t pixel_size) const;

  bool empty() const;
  std::size_t size() const;

private:
  struct FontKey {
    FontType type;
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
  void destroy();

  FT_Library library_ = nullptr;

  std::array<FT_Face, 5> ft_fonts_{};
  std::array<std::string, 5> fonts_paths_{};

  mutable std::unordered_map<FontKey, Font, FontKeyHash> cache_;

  bool is_empty_ = true;
};

} // namespace graphics
