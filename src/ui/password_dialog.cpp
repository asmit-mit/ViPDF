#include "ui/password_dialog.h"

#include <cmath>
#include <string>

#include "ui/ui_elements.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace ui {

PasswordDialog::PasswordDialog(const sf::Font &font, core::EventBus &event_bus, ui::Textbox &textbox)
    : event_bus_(event_bus), label_(font, "Enter password:", 16), textbox_(textbox) {
  visible_ = false;

  label_.setFillColor(utils::hexToRGB(settings::fg_));

  display_area_.setFillColor(utils::hexToRGB(settings::bg_hard_));
  display_area_.setOutlineColor(utils::hexToRGB(settings::notification_outline_bg_));
  display_area_.setOutlineThickness(-utils::outline_thickness);
  display_area_.setSize({dialog_width_, dialog_height_});

  password_bg_.setFillColor(utils::hexToRGB(settings::bg_));
  password_bg_.setOutlineColor(utils::hexToRGB(settings::notification_outline_bg_));
  password_bg_.setOutlineThickness(-utils::outline_thickness);
  password_bg_.setSize({dialog_width_ - 2.f * outer_padding_, input_height_});

  textbox_.setCursorSize({2.f, 20.f});
  textbox_.setMask(true);

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    visible_ = focus == ui::UIElements::PasswordDialog;
    if (visible_) {
      textbox_.reset();
      textbox_.startEditing();
      textbox_.show();
    }
  });
}

void PasswordDialog::draw(sf::RenderTarget &window) const {
  if (!visible_)
    return;

  window.draw(display_area_);
  window.draw(label_);
  window.draw(password_bg_);
  textbox_.draw(window);
}

void PasswordDialog::update() {
  if (!visible_)
    return;

  textbox_.update();
}

void PasswordDialog::handleEvent(const sf::Event &event) {
  if (!visible_)
    return;

  if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
    if (key->code == sf::Keyboard::Key::Escape) {
      textbox_.reset();
      textbox_.stopEditing();
      textbox_.hide();
      event_bus_.emit("cmd.close_document", true);
      event_bus_.emit("ui.focus", ui::UIElements::PDFView);
      return;
    }

    if (key->code == sf::Keyboard::Key::Enter) {
      const std::string password = textbox_.getText();
      textbox_.reset();
      textbox_.stopEditing();
      textbox_.hide();
      event_bus_.emit("password.submit", password);
      return;
    }
  }

  textbox_.handleEvent(event);
}

void PasswordDialog::onResize(const sf::Vector2f &size) {
  window_size_ = size;

  const float x = std::round((size.x - dialog_width_) / 2.f);
  const float y = std::round((size.y - dialog_height_) / 2.f);
  display_area_.setPosition({x, y});

  label_.setPosition({x + outer_padding_, y + 12.f});

  const float input_x = x + outer_padding_;
  const float input_y = y + dialog_height_ - outer_padding_ - input_height_;
  password_bg_.setPosition({input_x, input_y});

  textbox_.setWidth(password_bg_.getSize().x - 2.f * utils::padding);
  textbox_.setPosition({input_x + utils::padding, input_y + 5.f});
}

} // namespace ui
