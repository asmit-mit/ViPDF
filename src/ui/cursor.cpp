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
  if (!blinking_) {
    visible_ = false;
    return;
  }

  const auto elapsed = clock_.getElapsedTime();

  if (elapsed < typing_delay_) {
    visible_ = true;
    return;
  }

  const auto blink = elapsed - typing_delay_;
  visible_ = (blink.asMilliseconds() / blink_interval_.asMilliseconds()) % 2 == 0;
}

void Cursor::setColor(const sf::Color &color) {
  cursor_.setFillColor(color);
}

void Cursor::setPosition(const sf::Vector2f &position) {
  cursor_.setPosition(position);
}

sf::Vector2f Cursor::getPosition() const {
  return cursor_.getPosition();
}

void Cursor::start() {
  blinking_ = true;
  visible_ = true;
  clock_.restart();
}

void Cursor::stop() {
  blinking_ = false;
  visible_ = false;
}

void Cursor::typing() {
  if (!blinking_)
    return;

  visible_ = true;
  clock_.restart();
}

} // namespace ui
