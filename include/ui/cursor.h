#pragma once

#include "ui/widget.h"

namespace ui {

class Cursor : public Widget {
public:
  explicit Cursor();

  void draw(sf::RenderTarget &window) const override;
  void update() override;
  void handleEvent(const sf::Event &) override {}

  void setColor(const sf::Color &color);
  void setPosition(const sf::Vector2f &position);
  sf::Vector2f getPosition() const;
  void setSize(const sf::Vector2f &position);

  void start();
  void stop();
  void typing();

private:
  sf::RectangleShape cursor_;

  bool blinking_ = false;
  bool visible_ = false;

  sf::Clock clock_;

  static constexpr sf::Time typing_delay_ = sf::milliseconds(500);
  static constexpr sf::Time blink_interval_ = sf::milliseconds(500);
};

} // namespace ui
