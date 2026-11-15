#include "graphics/rich_text.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utf8.h>
#include <utility>

namespace graphics {

RichText::RichText(
    const graphics::FontLibrary &font_lib, GlyphAtlas &glyph_atlas, uint32_t character_size
)
    : font_lib_(font_lib), atlas_(glyph_atlas), hb_buffer_(hb_buffer_create()), size_{0.f, 0.f},
      pixel_size_(character_size), text_color_(sf::Color::White), line_height_{} {
  if (!hb_buffer_allocation_successful(hb_buffer_)) {
    hb_buffer_destroy(hb_buffer_);
    hb_buffer_ = nullptr;
    throw std::runtime_error("Failed to create hb_buffer");
  }

  font_loaded_ = false;
  default_font_type_ = FontType::Regular;
}

RichText::RichText(RichText &&other) noexcept
    : font_lib_(other.font_lib_), atlas_(other.atlas_),
      shaped_glyphs_(std::move(other.shaped_glyphs_)),
      vertex_arrays_(std::move(other.vertex_arrays_)), caret_x_(std::move(other.caret_x_)),
      hb_buffer_(std::exchange(other.hb_buffer_, nullptr)), size_(other.size_),
      pixel_size_(other.pixel_size_), text_color_(other.text_color_),
      line_height_(other.line_height_), text_(std::move(other.text_)),
      font_loaded_(other.font_loaded_), default_font_type_(other.default_font_type_) {}

RichText::~RichText() {
  if (hb_buffer_)
    hb_buffer_destroy(hb_buffer_);
}

void RichText::setString(const std::string &text) {
  setString(utf8::utf8to32(text));
}

void RichText::setString(const std::u32string &text) {
  text_ = text;

  vertex_arrays_.clear();
  shaped_glyphs_.clear();
  caret_x_.assign(1, 0.f);
  size_ = {0.f, 0.f};

  if (font_lib_.empty()) {
    std::cerr << "RichText::setString: no fonts loaded\n";
    return;
  }

  if (!font_loaded_) {
    loadFonts();
    font_loaded_ = true;
  }

  shapeAndCache(text_);
}

void RichText::setFillColor(sf::Color color) {
  text_color_ = color;
  setString(text_);
}

void RichText::setCharacterSize(uint32_t size) {
  if (pixel_size_ == size)
    return;

  pixel_size_ = size;
  loadFonts();

  if (!text_.empty())
    setString(text_);
}

void RichText::setBold() {
  default_font_type_ = FontType::Bold;
}

void RichText::setItalic() {
  default_font_type_ = FontType::Italic;
}

std::u32string RichText::getString() const {
  return text_;
}

sf::Vector2f RichText::getSize() const {
  return size_;
}

const std::vector<ShapedGlyph> &RichText::getShapedGlyphs() const {
  return shaped_glyphs_;
}

float RichText::getCaretX(std::size_t index) const {
  if (caret_x_.empty())
    return 0.f;
  return caret_x_[std::min(index, caret_x_.size() - 1)];
}

std::size_t RichText::getCaretCount() const {
  return caret_x_.size();
}

void RichText::draw(sf::RenderTarget &target, sf::RenderStates states) const {
  sf::RenderStates local = states;
  local.transform *= getTransform();
  for (std::size_t i = 0; i < vertex_arrays_.size(); ++i) {
    const sf::Texture &tex = atlas_.texture(i);
    local.texture = &tex;
    target.draw(vertex_arrays_[i], local);
  }
}

void RichText::loadFonts() {
  font_lib_.getFont(default_font_type_, pixel_size_);
}

void RichText::createTextRuns(const std::u32string &text, std::vector<Run> &runs, std::size_t base) {
  if (font_lib_.empty() || text.empty())
    return;

  std::size_t run_start = base;
  uint32_t first = static_cast<uint32_t>(text[0]);
  int type = font_lib_.getFontTypeForCodepoint(first, default_font_type_);
  std::u32string curr(1, text[0]);

  for (std::size_t i = 1; i < text.size(); i++) {
    uint32_t codepoint = static_cast<uint32_t>(text[i]);
    if (codepoint == 0xFE0F || codepoint == 0x200D) {
      curr += text[i];
      continue;
    }

    int new_type = font_lib_.getFontTypeForCodepoint(codepoint, default_font_type_);
    if (new_type == type) {
      curr += text[i];
      continue;
    }

    runs.push_back({curr, type, run_start});
    run_start += curr.size();

    type = new_type;
    curr.assign(1, text[i]);
  }

  if (!curr.empty())
    runs.push_back({curr, type, run_start});
}

void RichText::processLine(const std::vector<Run> &runs, float &pen_x, float &pen_y) {
  for (const Run &run : runs) {
    graphics::Font *font = font_lib_.getFont(run.font_type, pixel_size_);
    if (!font || !font->hb_font)
      continue;
    hb_font_t *hb_font = font->hb_font;

    hb_buffer_reset(hb_buffer_);
    hb_buffer_set_direction(hb_buffer_, HB_DIRECTION_LTR);
    hb_buffer_add_utf32(
        hb_buffer_,
        reinterpret_cast<const uint32_t *>(run.codepoints.data()),
        static_cast<int>(run.codepoints.size()),
        0,
        static_cast<int>(run.codepoints.size())
    );
    hb_buffer_guess_segment_properties(hb_buffer_);
    hb_shape(hb_font, hb_buffer_, nullptr, 0);

    unsigned int count = 0;
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer_, &count);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer_, &count);

    FT_Face ft_face = font->ft_face;
    std::vector<ClusterX> clusters;

    for (unsigned int i = 0; i < count; ++i) {
      if (clusters.empty() || clusters.back().cluster != info[i].cluster)
        clusters.push_back({info[i].cluster, pen_x});

      uint32_t glyph_idx = info[i].codepoint;
      float x_offset = static_cast<float>(pos[i].x_offset >> 6);
      float y_offset = static_cast<float>(pos[i].y_offset >> 6);
      float x_adv = static_cast<float>(pos[i].x_advance >> 6);

      const AtlasGlyph *glyph =
          atlas_.getOrPack(run.font_type, glyph_idx, pixel_size_, ft_face, hb_font);

      if (glyph && glyph->uv.size != sf::Vector2f{0.f, 0.f}) {
        while (vertex_arrays_.size() <= glyph->page)
          vertex_arrays_.emplace_back(sf::PrimitiveType::Triangles);

        sf::Color color = glyph->is_color ? sf::Color::White : text_color_;
        auto &va = vertex_arrays_[glyph->page];

        float x = pen_x + x_offset + static_cast<float>(glyph->bearing_x);
        float y = pen_y - y_offset - static_cast<float>(glyph->bearing_y);
        float w = glyph->uv.size.x * static_cast<float>(atlas_.width()) * glyph->scale;
        float h = glyph->uv.size.y * static_cast<float>(atlas_.height()) * glyph->scale;

        float u = glyph->uv.position.x * static_cast<float>(atlas_.width());
        float v = glyph->uv.position.y * static_cast<float>(atlas_.height());
        float uw = glyph->uv.size.x * static_cast<float>(atlas_.width());
        float uh = glyph->uv.size.y * static_cast<float>(atlas_.height());

        sf::Vector2f p0{x, y};
        sf::Vector2f p1{x + w, y};
        sf::Vector2f p2{x + w, y + h};
        sf::Vector2f p3{x, y + h};

        sf::Vector2f uv0{u, v};
        sf::Vector2f uv1{u + uw, v};
        sf::Vector2f uv2{u + uw, v + uh};
        sf::Vector2f uv3{u, v + uh};

        va.append({p0, color, uv0});
        va.append({p1, color, uv1});
        va.append({p2, color, uv2});
        va.append({p0, color, uv0});
        va.append({p2, color, uv2});
        va.append({p3, color, uv3});
      }

      long adv = (glyph && glyph->is_color) ? glyph->advance : static_cast<long>(x_adv);
      shaped_glyphs_.push_back({{pen_x, pen_y}, adv});
      pen_x += glyph ? (glyph->is_color ? static_cast<float>(glyph->advance) : x_adv) : x_adv;
    }

    clusters.push_back({static_cast<uint32_t>(run.codepoints.size()), pen_x});

    for (std::size_t k = 0; k + 1 < clusters.size(); ++k) {
      uint32_t c0 = clusters[k].cluster;
      uint32_t c1 = clusters[k + 1].cluster;
      if (c1 <= c0)
        continue;

      float x0 = clusters[k].x;
      float x1 = clusters[k + 1].x;
      for (uint32_t c = c0; c < c1; ++c) {
        std::size_t idx = run.offset + c;
        if (idx < caret_x_.size())
          caret_x_[idx] = x0 + (x1 - x0) * static_cast<float>(c - c0) / static_cast<float>(c1 - c0);
      }
    }

    std::size_t end_idx = run.offset + run.codepoints.size();
    if (end_idx < caret_x_.size())
      caret_x_[end_idx] = pen_x;
  }
}

void RichText::shapeAndCache(const std::u32string &text) {
  if (font_lib_.empty() || text.empty())
    return;

  Font *latin = font_lib_.getFont(graphics::FontType::Regular, pixel_size_);
  if (!latin || !latin->ft_face)
    return;

  FT_Face latin_face = latin->ft_face;
  float ascender = static_cast<float>(latin_face->size->metrics.ascender >> 6);
  float descender = static_cast<float>(latin_face->size->metrics.descender >> 6);
  float line_gap = static_cast<float>(latin_face->size->metrics.height >> 6);
  line_height_ = std::max(ascender - descender, line_gap);

  caret_x_.assign(text.size() + 1, -1.f);
  caret_x_[0] = 0.f;

  float pen_y = ascender;
  float max_x = 0.f;

  std::u32string::size_type start = 0;
  while (true) {
    auto end = text.find(U'\n', start);
    std::u32string segment = text.substr(start, end == std::u32string::npos ? end : end - start);

    if (!segment.empty()) {
      float pen_x = 0.f;
      std::vector<Run> runs;
      createTextRuns(segment, runs, start);
      processLine(runs, pen_x, pen_y);
      max_x = std::max(max_x, pen_x);
    }

    if (end == std::u32string::npos)
      break;

    pen_y += line_height_;
    start = end + 1;
  }

  for (std::size_t i = 0; i < caret_x_.size(); ++i) {
    if (caret_x_[i] < 0.f)
      caret_x_[i] = i > 0 ? caret_x_[i - 1] : 0.f;
  }

  size_.x = max_x;
  size_.y = pen_y;
}

} // namespace graphics
