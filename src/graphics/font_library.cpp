#include "graphics/font_library.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace graphics {

FontLibrary::FontLibrary() {
  if (FT_Init_FreeType(&library_) != 0)
    throw std::runtime_error("Failed to initialize font library");
}

FontLibrary::FontLibrary(const std::vector<std::pair<FontType, const char *>> &paths) {
  if (FT_Init_FreeType(&library_) != 0)
    throw std::runtime_error("Failed to initialize font library");

  for (const auto &[type, path] : paths)
    tryLoadFont(type, path);
}

FontLibrary::~FontLibrary() {
  destroy();
}

bool FontLibrary::empty() const {
  return is_empty_;
}

std::size_t FontLibrary::size() const {
  std::size_t count = 0;

  for (FT_Face face : ft_fonts_) {
    if (face)
      ++count;
  }

  return count;
}

void FontLibrary::tryLoadFont(FontType type, const char *path) {
  FT_Face face = nullptr;

  if (FT_New_Face(library_, path, 0, &face) != 0) {
    if (type == FontType::Regular) {
      throw std::runtime_error(std::string("Failed to load latin font: ") + path);
    }

    std::cerr << "Warning: could not load " << path << " — skipping\n";
    return;
  }

  for (auto it = cache_.begin(); it != cache_.end();) {
    if (it->first.type == type) {
      if (it->second.hb_font) {
        hb_font_destroy(it->second.hb_font);
        it->second.hb_font = nullptr;
      }

      if (it->second.ft_face) {
        FT_Done_Face(it->second.ft_face);
        it->second.ft_face = nullptr;
      }

      it = cache_.erase(it);
    } else {
      ++it;
    }
  }

  if (ft_fonts_[type]) {
    FT_Done_Face(ft_fonts_[type]);
    ft_fonts_[type] = nullptr;
  }

  ft_fonts_[type] = face;
  fonts_paths_[type] = path;
  is_empty_ = false;
}

FontType FontLibrary::getFontTypeForCodepoint(uint32_t codepoint, FontType default_type) const {
  if (ft_fonts_[default_type] && FT_Get_Char_Index(ft_fonts_[default_type], codepoint) != 0)
    return default_type;

  if (default_type != FontType::Regular && ft_fonts_[FontType::Regular] &&
      FT_Get_Char_Index(ft_fonts_[FontType::Regular], codepoint) != 0)
    return FontType::Regular;

  if (ft_fonts_[FontType::Emoji] && FT_Get_Char_Index(ft_fonts_[FontType::Emoji], codepoint) != 0)
    return FontType::Emoji;

  if (ft_fonts_[FontType::CJK] && FT_Get_Char_Index(ft_fonts_[FontType::CJK], codepoint) != 0)
    return FontType::CJK;

  return FontType::Regular;
}

Font *FontLibrary::getFont(FontType type, uint32_t pixel_size) const {
  if (pixel_size == 0)
    return nullptr;

  FontKey key{type, pixel_size};

  auto it = cache_.find(key);
  if (it != cache_.end())
    return &it->second;

  const std::string &path = fonts_paths_[type];
  if (path.empty())
    return nullptr;

  FT_Face face = nullptr;
  if (FT_New_Face(library_, path.c_str(), 0, &face) != 0)
    return nullptr;

  if (face->num_fixed_sizes > 0) {
    int best = 0;
    int best_diff = std::abs(face->available_sizes[0].height - static_cast<int>(pixel_size));

    for (int i = 1; i < face->num_fixed_sizes; ++i) {
      int diff = std::abs(face->available_sizes[i].height - static_cast<int>(pixel_size));
      if (diff < best_diff) {
        best_diff = diff;
        best = i;
      }
    }

    if (FT_Select_Size(face, best) != 0) {
      FT_Done_Face(face);
      return nullptr;
    }
  } else {
    if (FT_Set_Pixel_Sizes(face, 0, pixel_size) != 0) {
      FT_Done_Face(face);
      return nullptr;
    }
  }

  hb_font_t *hb_font = hb_ft_font_create(face, nullptr);
  if (!hb_font) {
    FT_Done_Face(face);
    return nullptr;
  }

  Font font;
  font.ft_face = face;
  font.hb_font = hb_font;

  auto [ins, success] = cache_.emplace(key, font);
  if (!success) {
    hb_font_destroy(hb_font);
    FT_Done_Face(face);
    return &ins->second;
  }

  return &ins->second;
}

void FontLibrary::destroy() {
  for (auto &[key, font] : cache_) {
    if (font.hb_font) {
      hb_font_destroy(font.hb_font);
      font.hb_font = nullptr;
    }

    if (font.ft_face) {
      FT_Done_Face(font.ft_face);
      font.ft_face = nullptr;
    }
  }

  cache_.clear();

  for (FT_Face &face : ft_fonts_) {
    if (face) {
      FT_Done_Face(face);
      face = nullptr;
    }
  }

  if (library_) {
    FT_Done_FreeType(library_);
    library_ = nullptr;
  }

  is_empty_ = true;
}

} // namespace graphics
