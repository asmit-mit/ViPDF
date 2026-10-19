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
  void subscribeToEvents();

  bool shouldShowPageInfo() const;
  bool shouldShowSearch() const;
  bool shouldShowPendingKeys() const;

  void refreshTexts();
  void layoutRightWidgets(float y);
  void setFilepath(std::u32string filepath);
  void fitFilepath(float max_width);

  static float placeLeftOf(sf::Text &text, float right_edge, float y);

private:
  const sf::Font &font_;
  core::EventBus &event_bus_;

  sf::RectangleShape display_area_;
  graphics::RichText display_filepath_;
  sf::Text display_search_state_;
  sf::Text display_page_state_;
  sf::Text display_zoom_;
  sf::Text display_pending_keys_;

  std::u32string filepath_;
  std::u32string last_truncated_;
  float last_available_width_;
  float max_filepath_width_;

  std::size_t search_num_;
  std::size_t total_search_results_;
  bool search_details_changed_;

  std::size_t page_num_;
  std::size_t total_pages_;
  bool page_details_changed_;

  bool move_up_;
  float curr_x_;
  float curr_y_;
};

} // namespace ui
