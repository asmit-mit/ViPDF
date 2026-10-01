#pragma once

#include "core/event_bus.h"
#include "graphics/rich_text.h"
#include "ui/widget.h"

namespace ui {

class Statusbar : public Widget {
public:
  explicit Statusbar(
      graphics::FontLibrary &font_lib,
      graphics::GlyphAtlas &glyph_atlas,
      const sf::Font &font,
      core::EventBus &event_bus
  );

  void draw(sf::RenderTarget &window) const override;
  void update() override;

  void onResize(const sf::Vector2f &size);

private:
  const sf::Font &font_;
  sf::RectangleShape display_area_;
  graphics::RichText display_filepath_;
  sf::Text display_search_state_;
  sf::Text display_page_state_;
  sf::Text display_zoom_;

  core::EventBus &event_bus_;

  std::u32string filepath_;
  std::u32string last_truncated_;

  std::size_t search_num_;
  std::size_t total_search_results_;

  std::size_t page_num_;
  std::size_t total_pages_;

  bool page_details_changed_;
  bool search_details_changed_;
  bool move_up_;
  float curr_x_, curr_y_;
  float last_available_width_;
};

} // namespace ui
