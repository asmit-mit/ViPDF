#include <utf8.h>

#include "ui/notifications.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace ui {

Notifications::Notifications(
    const graphics::FontLibrary &font_lib,
    graphics::GlyphAtlas &glyph_atlas,
    const sf::Font &font_normal,
    const sf::Font &font_bold,
    core::EventBus &event_bus
)
    : event_bus_(event_bus), font_library_(font_lib), font_normal_(font_normal),
      font_bold_(font_bold), display_header_(font_bold, "Notification", utils::char_size + 2),
      display_msg_(font_lib, glyph_atlas, utils::char_size) {
  visible_ = false;
  hovered_ = false;

  display_msg_.setFillColor(utils::hexToRGB(settings::fg_));
  display_header_.setFillColor(utils::hexToRGB(settings::fg_));

  display_area_.setFillColor(utils::hexToRGB(settings::bg_));
  display_area_.setOutlineThickness(2.f);
  display_area_.setOutlineColor(utils::hexToRGB(settings::notification_outline_bg_));

  idle_color_ = utils::hexToRGB(settings::bg_);
  hover_color_ = utils::hexToRGB(settings::notification_hover_bg_);

  event_bus_.subscribe<const std::string &>("notification.msg", [this](const std::string &msg) {
    show(msg);
  });
}

void Notifications::update() {
  if (!visible_)
    return;

  if (display_timer_.getElapsedTime().asSeconds() >= timeout_) {
    visible_ = false;
    hovered_ = false;
  }

  if (hovered_)
    display_area_.setFillColor(hover_color_);
  else
    display_area_.setFillColor(idle_color_);
}

void Notifications::draw(sf::RenderTarget &window) const {
  if (!visible_)
    return;

  window.draw(display_area_);
  window.draw(display_header_);
  window.draw(display_msg_);
}

void Notifications::handleEvent(const sf::Event &event) {
  if (!visible_)
    return;

  if (const auto *mouse = event.getIf<sf::Event::MouseMoved>())
    hovered_ = display_area_.getGlobalBounds().contains(sf::Vector2f(mouse->position));

  if (const auto *mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouse->button == sf::Mouse::Button::Left &&
        display_area_.getGlobalBounds().contains(sf::Vector2f(mouse->position))) {
      visible_ = false;
    }
  }
}

void Notifications::onResize(const sf::Vector2f &size) {
  window_size_ = size;
  layout();
}

std::string Notifications::wrapText(
    const std::string &raw, const sf::Font &font, unsigned int char_size, float max_width
) {
  std::vector<std::string> tokens;
  std::string current;
  for (char c : raw) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty())
    tokens.push_back(current);

  sf::Text probe(font, "", char_size);
  auto lineWidth = [&](const std::string &s) -> float {
    probe.setString(sf::String(s));
    return probe.getLocalBounds().size.x;
  };

  std::string line, result;

  auto flushLine = [&](const std::string &l) { result += l + '\n'; };

  auto fitPrefix = [&](const std::string &prefix, const std::string &word) -> std::size_t {
    std::string base = prefix + '-';
    std::size_t lo = 1, hi = word.size(), best = 0;
    while (lo <= hi) {
      std::size_t mid = (lo + hi) / 2;
      std::string candidate = base;
      candidate.insert(base.size() - 1, word, 0, mid);
      if (lineWidth(candidate) <= max_width) {
        best = mid;
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }
    return best;
  };

  auto hyphenateWord = [&](const std::string &word) {
    std::string remaining = word;
    bool first = true;

    while (!remaining.empty()) {
      std::string prefix = (first && !line.empty()) ? line + ' ' : "";
      first = false;

      if (lineWidth(prefix + remaining) <= max_width) {
        line = prefix + remaining;
        return;
      }

      std::size_t n = fitPrefix(prefix, remaining);
      if (n == 0) {
        if (!line.empty()) {
          flushLine(line);
          line = "";
          continue;
        }

        n = 1;
      }

      flushLine(prefix + remaining.substr(0, n) + '-');
      line = "";
      remaining = remaining.substr(n);
    }
  };

  for (const std::string &word : tokens) {
    std::string candidate = line.empty() ? word : line + ' ' + word;
    if (lineWidth(candidate) <= max_width) {
      line = candidate;
    } else {
      float word_w = lineWidth(word);
      if (!line.empty() && word_w <= max_width) {
        flushLine(line);
        line = word;
      } else {
        hyphenateWord(word);
      }
    }
  }

  if (!line.empty())
    result += line;
  return result;
}

void Notifications::show(const std::string &msg) {
  const float inner_width = max_width_ - 2.f * padding_;
  std::string wrapped = wrapText(msg, font_normal_, utils::char_size, inner_width);

  display_msg_.setString(wrapped);
  display_timer_.restart();

  visible_ = true;
  layout();
}

void Notifications::layout() {
  if (window_size_.x == 0.f && window_size_.y == 0.f)
    return;

  sf::FloatRect hb = display_header_.getLocalBounds();
  float header_h = hb.size.y;

  float text_w = display_msg_.getSize().x;
  float text_h = display_msg_.getSize().y;

  float box_w = std::min(std::max(text_w, hb.size.x) + 2.f * padding_, max_width_);
  float box_h = padding_ + header_h + padding_ * 1.5f + text_h + padding_ * 1.5f;

  const float margin = 8.f;
  float box_x = window_size_.x - box_w - margin;
  float box_y = margin;

  display_area_.setSize({box_w, box_h});
  display_area_.setPosition({box_x, box_y});

  display_header_.setPosition({box_x + padding_, box_y + padding_});
  display_msg_.setPosition({box_x + padding_, box_y + padding_ + header_h + padding_ * 1.5f});
}

} // namespace ui
