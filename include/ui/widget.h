#pragma once

#include <SFML/Graphics.hpp>

class Widget {
public:
  virtual ~Widget() = default;

  virtual void handleEvent(const sf::Event &) {};
  virtual void update() {};
  virtual void draw(sf::RenderTarget &) const = 0;

  virtual bool getAndConsumeDrawFlag() {
    bool flag = draw_flag_;
    draw_flag_ = false;
    return flag;
  }

protected:
  bool draw_flag_{true};
};
