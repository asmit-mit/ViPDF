#pragma once

#include <SFML/Graphics.hpp>

#include <ft2build.h>
#include <hb.h>
#include <string>

#include "graphics/font_library.h"
#include "graphics/glyph_atlas.h"

#include FT_FREETYPE_H
#include FT_BITMAP_H

namespace graphics {

struct ShapedGlyph {
  sf::Vector2f position;
  long advance;
};

class RichText : public sf::Drawable, public sf::Transformable {
public:
  RichText(
      const graphics::FontLibrary &font_lib, GlyphAtlas &glyph_atlas, uint32_t character_size = 30
  );

  RichText(const RichText &) = delete;
  RichText &operator=(const RichText &) = delete;

  RichText(RichText &&other) noexcept;
  RichText &operator=(RichText &&other) noexcept = delete;

  ~RichText() = default;

  void setString(const std::string &text);
  void setString(const std::u32string &text);
  void setFillColor(sf::Color color);
  void setCharacterSize(uint32_t size);
  void setBold();
  void setItalic();

  std::u32string getString() const;
  sf::Vector2f getSize() const;
  const std::vector<ShapedGlyph> &getShapedGlyphs() const;

private:
  struct Run {
    std::u32string codepoints;
    FontType font_type;
  };

private:
  void loadFonts();
  void createTextRuns(const std::u32string &text, std::vector<Run> &runs);
  void processLine(const std::vector<Run> &runs, float &pen_x, float &pen_y);
  void shapeAndCache(const std::u32string &text);

  void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

private:
  const graphics::FontLibrary &font_lib_;
  GlyphAtlas &atlas_;

  std::vector<ShapedGlyph> shaped_glyphs_;
  std::vector<sf::VertexArray> vertex_arrays_;

  hb_buffer_t *hb_buffer_;

  sf::Vector2f size_;
  uint32_t pixel_size_;
  sf::Color text_color_;
  float line_height_;

  std::u32string text_;
  bool font_loaded_;
  FontType default_font_type_;
};

} // namespace graphics
