#pragma once

#include <string>

#include <SFML/Graphics.hpp>

#include "core/config_reader.h"
#include "core/event_bus.h"
#include "ui/textbox.h"
#include "ui/widget.h"

namespace ui {

class ExternalLinkDialog : public Widget {
public:
  explicit ExternalLinkDialog(
      const sf::Font &font_regular,
      const sf::Font &font_bold,
      core::EventBus &event_bus,
      ui::Textbox &textbox,
      const core::Config &config
  );

  void draw(sf::RenderTarget &window) const override;
  void update() override;
  void handleEvent(const sf::Event &event) override;

  void onResize(const sf::Vector2f &size);

private:
  void setUrl(const std::string &url);
  void close();

  core::EventBus &event_bus_;

  sf::Text title_label_;
  sf::Text message_label_;
  ui::Textbox &textbox_;
  sf::RectangleShape display_area_;
  sf::RectangleShape input_bg_;

  sf::Vector2f window_size_{0.f, 0.f};
  std::string url_;

  static constexpr float dialog_width_ = 480.f;
  static constexpr float dialog_height_ = 130.f;
  static constexpr float outer_padding_ = 16.f;
  static constexpr float input_height_ = 36.f;

  bool visible_{false};
};

} // namespace ui
