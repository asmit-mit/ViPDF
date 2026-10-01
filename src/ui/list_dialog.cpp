#include "ui/list_dialog.h"

namespace ui {

ListDialog::ListDialog(
    const graphics::FontLibrary &font_lib,
    graphics::GlyphAtlas &glyph_atlas,
    const sf::Font &font_regular,
    std::string dialog_heading,
    std::string heading1,
    std::string heading2,
    std::string heading3
)
    : font_lib_(font_lib), glyph_atlas_(glyph_atlas), font_regular_(font_regular),
      dialog_heading_(font_regular, dialog_heading, 16), heading1_(font_regular, heading1, 16),
      heading2_(font_regular, heading2, 16), heading3_(font_regular, heading3, 16),
      textbox_(font_lib, glyph_atlas, 16, "") {
  curr_idx_ = 0;

  for (int i = 0; i < max_items_; i++) {
    col1.emplace_back(font_regular_, "", 16);
    col2.emplace_back(font_lib_, glyph_atlas_, 16);
    col3.emplace_back(font_regular_, "", 16);
  }
}

void ListDialog::draw(sf::RenderTarget &window) const {
  if (!show_dialog_)
    return;

  window.draw(dialog_heading_);

  window.draw(heading1_);
  window.draw(heading2_);
  window.draw(heading3_);

  for (int i = 0; i < max_items_; i++) {
    window.draw(col1[i]);
    window.draw(col2[i]);
    window.draw(col3[i]);
  }
}

void ListDialog::update() {
  if (!show_dialog_)
    return;
}

void ListDialog::handleEvent(const sf::Event &event) {
  if (!show_dialog_)
    return;
}

void ListDialog::setList(std::vector<std::array<std::string, 3>> list) {
  list_ = std::move(list);
}

void ListDialog::show() {
  show_dialog_ = true;
}

void ListDialog::hide() {
  show_dialog_ = false;
}

void ListDialog::layout() {}

} // namespace ui
