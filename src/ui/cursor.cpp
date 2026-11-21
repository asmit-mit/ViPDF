#include "ui/cursor.h"

namespace ui {

Cursor::Cursor() {
  cursor_.setSize({2.f, 24.f});
}

void Cursor::draw(sf::RenderTarget &window) const {
  if (visible_)
    window.draw(cursor_);
}

void Cursor::update() {
  const bool was_visible = visible_;

  if (!blinking_) {
    visible_ = false;
  } else {
    const auto elapsed = clock_.getElapsedTime();

    if (elapsed < typing_delay_) {
      visible_ = true;
    } else {
      const auto blink = elapsed - typing_delay_;
      visible_ = (blink.asMilliseconds() / blink_interval_.asMilliseconds()) % 2 == 0;
    }
  }

  if (visible_ != was_visible)
    draw_flag_ = true;
}

void Cursor::setColor(const sf::Color &color) {
  cursor_.setFillColor(color);
  draw_flag_ = true;
}

void Cursor::setPosition(const sf::Vector2f &position) {
  cursor_.setPosition(position);
  draw_flag_ = true;
}

sf::Vector2f Cursor::getPosition() const {
  return cursor_.getPosition();
}

void Cursor::start() {
  blinking_ = true;
  visible_ = true;
  clock_.restart();
  draw_flag_ = true;
}

void Cursor::stop() {
  blinking_ = false;
  visible_ = false;
  draw_flag_ = true;
}

void Cursor::typing() {
  if (!blinking_)
    return;

  if (!visible_)
    draw_flag_ = true;
  visible_ = true;
  clock_.restart();
}

} // namespace ui
