#include "graphics/glyph_atlas.h"

#include <stdexcept>

#include <hb-ot.h>
#include <hb-raster.h>

namespace graphics {

GlyphAtlas::GlyphAtlas(unsigned int width, unsigned int height)
    : atlas_w_(width), atlas_h_(height), curr_page_{} {
  atlas_pages_.emplace_back();
  if (!atlas_pages_.back().texture.resize({atlas_w_, atlas_h_}))
    throw std::runtime_error("Failed to resize atlas texture");
}

const AtlasGlyph *GlyphAtlas::getOrPack(
    int font_type, uint32_t glyph_idx, uint32_t pixel_size, FT_Face ft_face, hb_font_t *hb_font
) {
  Key key{font_type, glyph_idx, pixel_size};

  auto it = cache_.find(key);
  if (it != cache_.end())
    return &it->second;

  AtlasGlyph glyph;
  bool success = false;

  hb_face_t *hb_face = hb_font_get_face(hb_font);
  bool colrv1 = hb_ot_color_glyph_has_paint(hb_face, glyph_idx);
  if (colrv1)
    success = rastarizeGlyph(key, glyph, hb_font, hb_face);
  else
    success = rastarizeGlyph(key, glyph, ft_face);

  if (!success)
    return nullptr;

  auto [ins, _] = cache_.emplace(key, glyph);
  return &ins->second;
}

std::size_t GlyphAtlas::pageCount() const {
  return atlas_pages_.size();
}

std::size_t GlyphAtlas::width() {
  return atlas_w_;
}

std::size_t GlyphAtlas::height() {
  return atlas_h_;
}

void GlyphAtlas::clear() {
  cache_.clear();
  curr_page_ = 0;
}

bool GlyphAtlas::rastarizeGlyph(const Key &key, AtlasGlyph &glyph, FT_Face face) {
  if (!face)
    return false;

  if (FT_Load_Glyph(face, key.glyph_idx, FT_LOAD_COLOR | FT_LOAD_RENDER) != 0)
    return false;

  FT_GlyphSlot slot = face->glyph;
  FT_Bitmap &bitmap = slot->bitmap;

  glyph.bearing_x = slot->bitmap_left;
  glyph.bearing_y = slot->bitmap_top;
  glyph.advance = slot->advance.x >> 6;
  glyph.is_color = (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA);

  unsigned int width = bitmap.width;
  unsigned int height = bitmap.rows;

  if (width == 0 || height == 0) {
    glyph.uv = {};
    glyph.page = curr_page_;
    return true;
  }

  AtlasPage &curr_page = atlas_pages_[curr_page_];
  if (curr_page.cursor_x + width > atlas_w_) {
    curr_page.cursor_y += curr_page.row_h;
    curr_page.cursor_x = 0;
    curr_page.row_h = 0;
  }

  if (curr_page.cursor_y + height > atlas_h_)
    addPage();

  AtlasPage &curr = atlas_pages_[curr_page_];
  glyph.page = curr_page_;

  unsigned int cx = curr.cursor_x;
  unsigned int cy = curr.cursor_y;

  sf::Image staging({width, height}, sf::Color::Transparent);
  const uint8_t *src = bitmap.buffer;
  int pitch = std::abs(bitmap.pitch);

  if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
    for (unsigned int y = 0; y < height; ++y) {
      const uint8_t *row = src + static_cast<std::ptrdiff_t>(y) * pitch;
      for (unsigned int x = 0; x < width; ++x) {
        uint8_t b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2], a = row[x * 4 + 3];
        if (a != 0 && a != 255) {
          r = static_cast<uint8_t>(std::min(255u, (uint32_t)r * 255u / a));
          g = static_cast<uint8_t>(std::min(255u, (uint32_t)g * 255u / a));
          b = static_cast<uint8_t>(std::min(255u, (uint32_t)b * 255u / a));
        }
        staging.setPixel({x, y}, sf::Color(r, g, b, a));
      }
    }
  } else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
    for (unsigned int y = 0; y < height; ++y) {
      const uint8_t *row = src + static_cast<std::ptrdiff_t>(y) * pitch;
      for (unsigned int x = 0; x < width; ++x)
        staging.setPixel({x, y}, sf::Color(255, 255, 255, row[x]));
    }
  } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
    for (unsigned int y = 0; y < height; ++y) {
      const uint8_t *row = src + static_cast<std::ptrdiff_t>(y) * pitch;
      for (unsigned int x = 0; x < width; ++x) {
        uint8_t a = (row[x / 8] & (0x80u >> (x & 7u))) ? 255u : 0u;
        staging.setPixel({x, y}, sf::Color(255, 255, 255, a));
      }
    }
  }

  curr.texture.update(staging, {cx, cy});

  glyph.uv = sf::
      FloatRect({(float)cx / atlas_w_, (float)cy / atlas_h_}, {(float)width / atlas_w_, (float)height / atlas_h_});

  if (glyph.is_color) {
    float scale = static_cast<float>(key.pixel_size) / static_cast<float>(height);
    glyph.bearing_x = static_cast<int>(glyph.bearing_x * scale);
    glyph.bearing_y = static_cast<int>(glyph.bearing_y * scale);
    glyph.advance = static_cast<long>(key.pixel_size);
    glyph.scale = scale;
  }

  curr.row_h = std::max(curr.row_h, height);
  curr.cursor_x = cx + width;

  return true;
}

void GlyphAtlas::addPage() {
  curr_page_++;
  if (curr_page_ >= atlas_pages_.size()) {
    atlas_pages_.emplace_back();
    if (!atlas_pages_.back().texture.resize({atlas_w_, atlas_h_}))
      throw std::runtime_error("Failed to resize atlas texture");
    return;
  }

  AtlasPage &page = atlas_pages_[curr_page_];
  page.cursor_x = 0;
  page.cursor_y = 0;
  page.row_h = 0;
  page.texture.update(sf::Image({atlas_w_, atlas_h_}, sf::Color::Transparent));
}

bool GlyphAtlas::rastarizeGlyph(
    const Key &key, AtlasGlyph &glyph, hb_font_t *font, hb_face_t *face
) {
  if (!font)
    return false;

  hb_glyph_extents_t extents{};
  if (!hb_font_get_glyph_extents(font, key.glyph_idx, &extents))
    return false;

  int x0 = static_cast<int>(std::floor(extents.x_bearing / 64.0));
  int x1 = static_cast<int>(std::ceil((extents.x_bearing + extents.width) / 64.0));

  int y1 = static_cast<int>(std::ceil(extents.y_bearing / 64.0));
  int y0 = static_cast<int>(std::floor((extents.y_bearing + extents.height) / 64.0));

  int glyph_w = x1 - x0;
  int glyph_h = y1 - y0;

  if (glyph_w <= 0 || glyph_h <= 0)
    return false;

  hb_raster_paint_t *paint = hb_raster_paint_create_or_fail();
  if (!paint)
    return false;

  hb_raster_paint_set_scale_factor(paint, 64.0f, 64.0f);
  hb_raster_paint_set_transform(paint, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f);
  hb_raster_paint_set_palette(paint, 0);
  hb_raster_paint_set_background(paint, HB_COLOR(0, 0, 0, 0));

  hb_raster_extents_t rext{};
  rext.x_origin = x0;
  rext.y_origin = y0;
  rext.width = static_cast<unsigned int>(glyph_w);
  rext.height = static_cast<unsigned int>(glyph_h);
  rext.stride = 0;

  hb_raster_paint_set_extents(paint, &rext);
  hb_raster_paint_glyph(paint, font, key.glyph_idx);

  hb_raster_image_t *image = hb_raster_paint_render(paint);
  hb_raster_paint_destroy(paint);

  if (!image)
    return false;

  hb_raster_extents_t image_ext{};
  hb_raster_image_get_extents(image, &image_ext);

  const uint8_t *pixels = hb_raster_image_get_buffer(image);

  if (!pixels || hb_raster_image_get_format(image) != HB_RASTER_FORMAT_BGRA32) {
    hb_raster_image_destroy(image);
    return false;
  }

  const unsigned int width = image_ext.width;
  const unsigned int height = image_ext.height;

  AtlasPage *curr = &atlas_pages_[curr_page_];

  if (curr->cursor_x + width > atlas_w_) {
    curr->cursor_y += curr->row_h;
    curr->cursor_x = 0;
    curr->row_h = 0;
  }

  if (curr->cursor_y + height > atlas_h_) {
    addPage();
    curr = &atlas_pages_[curr_page_];
  }

  const unsigned int cx = curr->cursor_x;
  const unsigned int cy = curr->cursor_y;

  sf::Image staging({width, height}, sf::Color::Transparent);

  for (unsigned int y = 0; y < height; ++y) {
    const uint8_t *src = pixels + (height - 1 - y) * image_ext.stride;

    for (unsigned int x = 0; x < width; ++x) {
      uint8_t b = src[x * 4 + 0];
      uint8_t g = src[x * 4 + 1];
      uint8_t r = src[x * 4 + 2];
      uint8_t a = src[x * 4 + 3];

      if (a != 0 && a != 255) {
        r = static_cast<uint8_t>(std::min(255u, static_cast<uint32_t>(r) * 255u / a));
        g = static_cast<uint8_t>(std::min(255u, static_cast<uint32_t>(g) * 255u / a));
        b = static_cast<uint8_t>(std::min(255u, static_cast<uint32_t>(b) * 255u / a));
      }

      staging.setPixel({x, y}, sf::Color(r, g, b, a));
    }
  }

  curr->texture.update(staging, {cx, cy});

  hb_raster_image_destroy(image);

  hb_position_t hb_advance = hb_font_get_glyph_h_advance(font, key.glyph_idx);

  glyph.page = curr_page_;
  glyph.is_color = true;
  glyph.uv = sf::
      FloatRect({static_cast<float>(cx) / atlas_w_, static_cast<float>(cy) / atlas_h_}, {static_cast<float>(width) / atlas_w_, static_cast<float>(height) / atlas_h_});
  glyph.advance = static_cast<long>(hb_advance >> 6);
  glyph.bearing_x = x0;
  glyph.bearing_y = y1;

  float scale = static_cast<float>(key.pixel_size) / static_cast<float>(height);
  glyph.scale = scale;
  glyph.bearing_x = static_cast<int>(glyph.bearing_x * scale);
  glyph.bearing_y = static_cast<int>(glyph.bearing_y * scale);
  glyph.advance = static_cast<long>(key.pixel_size);

  curr->row_h = std::max(curr->row_h, height);
  curr->cursor_x = cx + width;

  return true;
}

const sf::Texture &GlyphAtlas::texture(std::size_t page) const {
  return atlas_pages_.at(page).texture;
}

} // namespace graphics
