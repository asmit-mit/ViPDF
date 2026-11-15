#include "graphics/font_library.h"

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace graphics {

FontLibrary::FontLibrary(const std::vector<const char *> &paths) {
  if (FT_Init_FreeType(&library_) != 0)
    throw std::runtime_error("Failed to initialize font library");

  std::size_t slots = std::max<std::size_t>(BUILTIN_SLOTS, paths.size());
  ft_fonts_.assign(slots, nullptr);
  fonts_paths_.assign(slots, std::string{});
  fonts_indices_.assign(slots, 0);

  try {
    for (std::size_t i = 0; i < paths.size(); ++i)
      tryLoadFont(static_cast<int>(i), paths[i]);
  } catch (...) {
    destroy();
    throw;
  }
}

FontLibrary::~FontLibrary() {
  destroy();
}

bool FontLibrary::empty() const {
  return is_empty_;
}

std::size_t FontLibrary::size() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::size_t count = 0;
  for (FT_Face face : ft_fonts_) {
    if (face)
      ++count;
  }

  return count;
}

bool FontLibrary::hasGlyph(int type, uint32_t codepoint) const {
  return type >= 0 && type < static_cast<int>(ft_fonts_.size()) && ft_fonts_[type] &&
         FT_Get_Char_Index(ft_fonts_[type], codepoint) != 0;
}

int FontLibrary::getFontTypeForCodepoint(uint32_t codepoint, int default_type) const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (hasGlyph(default_type, codepoint))
    return default_type;

  if (default_type != FontType::Regular && hasGlyph(FontType::Regular, codepoint))
    return FontType::Regular;

  if (hasGlyph(FontType::Emoji, codepoint))
    return FontType::Emoji;

  if (hasGlyph(FontType::CJK, codepoint))
    return FontType::CJK;

  for (int t = BUILTIN_SLOTS; t < static_cast<int>(ft_fonts_.size()); ++t) {
    if (hasGlyph(t, codepoint))
      return t;
  }

  int t = findSystemFallback(codepoint, default_type);
  return t >= 0 ? t : FontType::Regular;
}

Font *FontLibrary::getFont(int type, uint32_t pixel_size) const {
  if (pixel_size == 0)
    return nullptr;

  std::lock_guard<std::mutex> lock(mutex_);

  if (type < 0 || type >= static_cast<int>(fonts_paths_.size()))
    return nullptr;

  FontKey key{type, pixel_size};

  auto it = cache_.find(key);
  if (it != cache_.end())
    return &it->second;

  const std::string &path = fonts_paths_[type];
  if (path.empty())
    return nullptr;

  FT_Face face = nullptr;
  if (FT_New_Face(library_, path.c_str(), fonts_indices_[type], &face) != 0)
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

void FontLibrary::tryLoadFont(int type, const char *path) {
  FT_Face face = nullptr;

  if (!path || FT_New_Face(library_, path, 0, &face) != 0) {
    const char *name = path ? path : "(null)";

    if (type == FontType::Regular)
      throw std::runtime_error(std::string("Failed to load latin font: ") + name);

    std::cerr << "Warning: could not load " << name << " — skipping\n";
    return;
  }

  ft_fonts_[type] = face;
  fonts_paths_[type] = path;
  fonts_indices_[type] = 0;
  is_empty_ = false;
}

int FontLibrary::addFallbackFont(const std::string &path, int face_index) const {
  for (std::size_t t = BUILTIN_SLOTS; t < ft_fonts_.size(); ++t) {
    if (fonts_paths_[t] == path && fonts_indices_[t] == face_index)
      return static_cast<int>(t);
  }

  FT_Face face = nullptr;
  if (FT_New_Face(library_, path.c_str(), face_index, &face) != 0)
    return -1;

  ft_fonts_.push_back(face);
  fonts_paths_.push_back(path);
  fonts_indices_.push_back(face_index);
  return static_cast<int>(ft_fonts_.size() - 1);
}

int FontLibrary::findSystemFallback(uint32_t codepoint, int style) const {
  uint32_t key = (static_cast<uint32_t>(style) << 24) | codepoint;

  auto it = fallback_cache_.find(key);
  if (it != fallback_cache_.end())
    return it->second;

  int type = -1;
  std::string path;
  int face_index = 0;

  if (queryFontconfig(codepoint, style, path, face_index)) {
    int t = addFallbackFont(path, face_index);
    if (t >= 0 && FT_Get_Char_Index(ft_fonts_[t], codepoint) != 0)
      type = t;
  }

  fallback_cache_[key] = type;
  return type;
}

bool FontLibrary::queryFontconfig(
    uint32_t codepoint, int style, std::string &out_path, int &out_index
) const {
  if (!FcInit())
    return false;

  FcPattern *pat = FcPatternCreate();
  FcCharSet *cs = FcCharSetCreate();
  if (!pat || !cs) {
    if (pat)
      FcPatternDestroy(pat);
    if (cs)
      FcCharSetDestroy(cs);
    return false;
  }

  FcCharSetAddChar(cs, codepoint);
  FcPatternAddCharSet(pat, FC_CHARSET, cs);
  FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<const FcChar8 *>("sans-serif"));

  if (style == FontType::Bold)
    FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);
  else if (style == FontType::Italic)
    FcPatternAddInteger(pat, FC_SLANT, FC_SLANT_ITALIC);
  else if (style == FontType::Emoji)
    FcPatternAddBool(pat, FC_COLOR, FcTrue);

  FcConfigSubstitute(nullptr, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);

  FcResult res;
  FcFontSet *fs = FcFontSort(nullptr, pat, FcFalse, nullptr, &res);

  bool found = false;
  std::string best_path;
  int best_index = 0;

  for (int i = 0; fs && i < fs->nfont; ++i) {
    FcCharSet *fcs = nullptr;
    if (FcPatternGetCharSet(fs->fonts[i], FC_CHARSET, 0, &fcs) != FcResultMatch ||
        !FcCharSetHasChar(fcs, codepoint))
      continue;

    FcChar8 *file = nullptr;
    if (FcPatternGetString(fs->fonts[i], FC_FILE, 0, &file) != FcResultMatch)
      continue;

    int index = 0;
    FcPatternGetInteger(fs->fonts[i], FC_INDEX, 0, &index);

    FcChar8 *family = nullptr;
    FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family);
    bool is_noto = family &&
                   std::strncmp(reinterpret_cast<const char *>(family), "Noto Sans", 9) == 0;

    if (!found) {
      best_path = reinterpret_cast<const char *>(file);
      best_index = index & 0xFFFF;
      found = true;
    }

    if (is_noto) {
      best_path = reinterpret_cast<const char *>(file);
      best_index = index & 0xFFFF;
      break;
    }
  }

  if (fs)
    FcFontSetDestroy(fs);
  FcCharSetDestroy(cs);
  FcPatternDestroy(pat);

  if (found) {
    out_path = std::move(best_path);
    out_index = best_index;
  }
  return found;
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
  fallback_cache_.clear();

  for (FT_Face &face : ft_fonts_) {
    if (face) {
      FT_Done_Face(face);
      face = nullptr;
    }
  }

  ft_fonts_.clear();
  fonts_paths_.clear();
  fonts_indices_.clear();

  if (library_) {
    FT_Done_FreeType(library_);
    library_ = nullptr;
  }

  is_empty_ = true;
}

} // namespace graphics
