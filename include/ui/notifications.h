#pragma once

#include "core/event_bus.h"
#include "graphics/rich_text.h"
#include "ui/widget.h"

namespace ui {

class Notifications : public Widget {
public:
  explicit Notifications(
      const graphics::FontLibrary &font_lib,
      graphics::GlyphAtlas &glyph_atlas,
      const sf::Font &font_normal,
      const sf::Font &font_bold,
      core::EventBus &event_bus
  );

  void draw(sf::RenderTarget &window) const override;
  void update() override;
  void handleEvent(const sf::Event &event) override;

  void onResize(const sf::Vector2f &size);

private:
  void show(const std::string &msg);
  void layout();
  std::string
  wrapText(const std::string &raw, const sf::Font &font, unsigned int char_size, float max_width);

private:
  core::EventBus &event_bus_;
  const graphics::FontLibrary &font_library_;
  const sf::Font &font_normal_;
  const sf::Font &font_bold_;

  sf::Text display_header_;
  graphics::RichText display_msg_;
  sf::RectangleShape display_area_;
  sf::Clock display_timer_;
  sf::Vector2f window_size_;

  sf::Color idle_color_;
  sf::Color hover_color_;

  bool visible_;
  bool hovered_;

  static constexpr float max_width_ = 400.f;
  static constexpr float padding_ = 10.f;
  static constexpr float timeout_ = 5.f;
};

} // namespace ui
