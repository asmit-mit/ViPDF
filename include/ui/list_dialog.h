#pragma once

#include "graphics/font_library.h"
#include "graphics/rich_text.h"
#include "ui/textbox.h"
#include "ui/widget.h"

namespace ui {

class ListDialog : public Widget {
public:
  explicit ListDialog(
      const graphics::FontLibrary &font_lib,
      graphics::GlyphAtlas &glyph_atlas,
      const sf::Font &font_regular,
      std::string dialog_heading,
      std::string heading1,
      std::string heading2,
      std::string heading3
  );

  void draw(sf::RenderTarget &window) const override;
  void update() override;
  void handleEvent(const sf::Event &event) override;
  void onResize(sf::Vector2f size);

  void setList(std::vector<std::array<std::string, 3>> list);
  void show();
  void hide();

private:
  void layout();

private:
  const graphics::FontLibrary &font_lib_;
  graphics::GlyphAtlas &glyph_atlas_;
  const sf::Font &font_regular_;

  sf::Text dialog_heading_;
  sf::Text heading1_;
  sf::Text heading2_;
  sf::Text heading3_;

  std::vector<sf::Text> col1;
  std::vector<graphics::RichText> col2;
  std::vector<sf::Text> col3;

  sf::RectangleShape background_;

  std::vector<std::array<std::string, 3>> list_;
  std::size_t curr_idx_;

  Textbox textbox_;

  bool show_dialog_;

  static constexpr float max_width_ = 800.f;
  static constexpr int max_items_ = 8;
};

} // namespace ui
