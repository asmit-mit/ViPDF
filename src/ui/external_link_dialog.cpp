#include "ui/external_link_dialog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "ui/ui_elements.h"
#include "utils/utils.h"

namespace ui {

namespace {

std::string normalize(const std::string &s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos)
    return "";
  const auto end = s.find_last_not_of(" \t\r\n");
  std::string out = s.substr(begin, end - begin + 1);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

} // namespace

ExternalLinkDialog::ExternalLinkDialog(
    const sf::Font &font_regular,
    const sf::Font &font_bold,
    core::EventBus &event_bus,
    ui::Textbox &textbox,
    const core::Config &config
)
    : event_bus_(event_bus), title_label_(font_bold, "Warning! External Link", 16),
      message_label_(font_regular, "", 14), textbox_(textbox) {
  visible_ = false;

  title_label_.setFillColor(utils::hexToRGB(config.dialog_heading_text_color));
  title_label_.setStyle(sf::Text::Bold);
  message_label_.setFillColor(utils::hexToRGB(config.dialog_heading_text_color));

  display_area_.setFillColor(utils::hexToRGB(config.dialog_background_color));
  display_area_.setOutlineColor(utils::hexToRGB(config.dialog_outline_color));
  display_area_.setOutlineThickness(-utils::OUTLINE_THICKNESS);
  display_area_.setSize({dialog_width_, dialog_height_});

  input_bg_.setFillColor(utils::hexToRGB(config.dialog_textbox_background_color));
  input_bg_.setOutlineColor(utils::hexToRGB(config.dialog_textbox_outline_color));
  input_bg_.setOutlineThickness(-utils::OUTLINE_THICKNESS);
  input_bg_.setSize({dialog_width_ - 2.f * outer_padding_, input_height_});

  textbox_.setCursorSize({2.f, 20.f});
  textbox_.setMask(false);
  // textbox_.setPlaceholder("Y/n"); // adjust to your Textbox API
  textbox_.setTextColor(utils::hexToRGB(config.dialog_heading_text_color));
  textbox_.setHighlightColor(utils::hexToRGB(config.dialog_textbox_highlight_color));
  textbox_.setHighlightTextColor(utils::hexToRGB(config.dialog_highlight_text_color));

  event_bus_.subscribe<const std::string &>("link.open_external", [this](const std::string &url) {
    setUrl(url);
  });

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    visible_ = focus == ui::UIElements::ExternalLinkDialog;
    if (visible_) {
      textbox_.reset();
      textbox_.startEditing();
      textbox_.show();
    }
  });
}

void ExternalLinkDialog::setUrl(const std::string &url) {
  url_ = url;

  const float max_width = dialog_width_ - 2.f * outer_padding_;
  const std::string prefix = "Do you want to visit: ";
  const std::string suffix = "? [Y/n]";
  constexpr std::size_t extra_trim = 5;

  std::string shown = url_;
  message_label_.setString(prefix + shown + suffix);

  if (message_label_.getLocalBounds().size.x > max_width) {
    while (!shown.empty()) {
      message_label_.setString(prefix + shown + "..." + suffix);
      if (message_label_.getLocalBounds().size.x <= max_width)
        break;
      shown.pop_back();
    }

    shown.erase(shown.size() >= extra_trim ? shown.size() - extra_trim : 0);
    message_label_.setString(prefix + shown + "..." + suffix);
  }
}

void ExternalLinkDialog::close() {
  textbox_.reset();
  textbox_.stopEditing();
  textbox_.hide();
  url_.clear();
  event_bus_.emit("ui.focus", ui::UIElements::PDFView);
}

void ExternalLinkDialog::draw(sf::RenderTarget &window) const {
  if (!visible_)
    return;

  window.draw(display_area_);
  window.draw(title_label_);
  window.draw(message_label_);
  window.draw(input_bg_);
  textbox_.draw(window);
}

void ExternalLinkDialog::update() {
  if (!visible_)
    return;

  textbox_.update();
}

void ExternalLinkDialog::handleEvent(const sf::Event &event) {
  if (!visible_)
    return;

  if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
    if (key->code == sf::Keyboard::Key::Escape) {
      close();
      return;
    }

    if (key->code == sf::Keyboard::Key::Enter) {
      const std::string answer = normalize(textbox_.getText());

      if (answer == "y" || answer == "yes") {
        const std::string url = url_;
        close();
        event_bus_.emit("dbus.open_link", url);
      } else if (answer == "n" || answer == "no") {
        close();
      } else {
        textbox_.reset();
        textbox_.startEditing();
      }
      return;
    }
  }

  textbox_.handleEvent(event);
}

void ExternalLinkDialog::onResize(const sf::Vector2f &size) {
  window_size_ = size;

  const float x = std::round((size.x - dialog_width_) / 2.f);
  const float y = std::round((size.y - dialog_height_) / 2.f);
  display_area_.setPosition({x, y});

  title_label_.setPosition({x + outer_padding_, y + 12.f});
  message_label_.setPosition({x + outer_padding_, y + 44.f});

  const float input_x = x + outer_padding_;
  const float input_y = y + dialog_height_ - outer_padding_ - input_height_;
  input_bg_.setPosition({input_x, input_y});

  textbox_.setWidth(input_bg_.getSize().x - 2.f * utils::PADDING);
  textbox_.setPosition({input_x + utils::PADDING, input_y + 5.f});
}

} // namespace ui
