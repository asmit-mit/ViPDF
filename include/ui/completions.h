#pragma once

#include "core/cmd_autocomplete.h"
#include "graphics/rich_text.h"
#include "ui/widget.h"

#include <SFML/Graphics.hpp>

namespace ui {

class Completions : public Widget {
public:
  explicit Completions(
      const graphics::FontLibrary &font_lib,
      graphics::GlyphAtlas &glyph_atlas,
      const sf::Font &cmd_font,
      const sf::Font &desc_font,
      int font_size
  );

  void draw(sf::RenderTarget &window) const override;
  void update() override;
  void handleEvent(const sf::Event &event) override;
  void onResize(const sf::Vector2f &size);

  void setFillColor(const sf::Color &color);
  void setCmdColor(const sf::Color &color);
  void setDescColor(const sf::Color &color);
  void setPosition(const sf::Vector2f &pos);

  bool isVisible();
  void show();
  void hide();
  void moveUp();
  void moveDown();
  std::string &getSelectedText();

  void setCompletionList(std::vector<core::CmdAutocompleteItem> completions_);
  void clear();

private:
  std::vector<core::CmdAutocompleteItem> completions_;
  std::vector<graphics::RichText> display_cmd_list_;
  std::vector<sf::Text> display_desc_list_;

  sf::RectangleShape display_area_;
  sf::RectangleShape selected_cmd_area_;
  sf::RectangleShape selected_desc_area_;
  sf::Vector2f window_size_;

  const sf::Font &cmd_font_;
  const sf::Font &desc_font_;
  sf::Color cmd_fg_color_;
  sf::Color desc_fg_color_;

  std::size_t first_visible_;
  std::size_t selected_;

  int font_size_;
  bool visible_;
  bool list_dirty_;
  bool selected_has_desc_;

  static constexpr std::size_t max_list_items_ = 6;
};

} // namespace ui
