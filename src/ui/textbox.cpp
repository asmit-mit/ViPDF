#include "ui/textbox.h"

#include <hb.h>
#include <utf8.h>

#include "utils/constants.h"

namespace ui {

namespace {

bool isExtender(char32_t c) {
  if (c == 0x200D || c == 0x200C)
    return true;
  if (c >= 0x1F3FB && c <= 0x1F3FF)
    return true;
  if (c >= 0xE0020 && c <= 0xE007F)
    return true;

  switch (hb_unicode_general_category(hb_unicode_funcs_get_default(), c)) {
  case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
  case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
  case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
    return true;
  default:
    return false;
  }
}
bool isVirama(char32_t c) {
  return hb_unicode_combining_class(hb_unicode_funcs_get_default(), c) ==
         HB_UNICODE_COMBINING_CLASS_VIRAMA;
}

bool isLetter(char32_t c) {
  return hb_unicode_general_category(hb_unicode_funcs_get_default(), c) ==
         HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER;
}

bool isRegionalIndicator(char32_t c) {
  return c >= 0x1F1E6 && c <= 0x1F1FF;
}

bool isEmojiLike(char32_t c) {
  return (c >= 0x2190 && c <= 0x2BFF) || c >= 0x1F000;
}

bool isWordChar(char32_t c) {
  return c < 128 && (std::isalnum(static_cast<unsigned char>(c)) || c == '_');
}

bool isSpaceChar(char32_t c) {
  return c < 128 && std::isspace(static_cast<unsigned char>(c));
}

std::size_t clusterEnd(const std::u32string &s, std::size_t pos) {
  if (pos >= s.size())
    return s.size();

  std::size_t i = pos + 1;

  if (isRegionalIndicator(s[pos]) && i < s.size() && isRegionalIndicator(s[i]))
    ++i;

  while (i < s.size() && isExtender(s[i])) {
    if (s[i] == 0x200D && i + 1 < s.size() && isEmojiLike(s[i + 1]))
      i += 2;
    else if (isVirama(s[i]) && i + 1 < s.size() && isLetter(s[i + 1]))
      i += 2;
    else
      ++i;
  }
  return i;
}

std::size_t clusterStart(const std::u32string &s, std::size_t pos) {
  if (pos == 0)
    return 0;
  if (pos > s.size())
    pos = s.size();

  std::size_t i = 0;
  while (i < pos) {
    std::size_t e = clusterEnd(s, i);
    if (e >= pos)
      return i;
    i = e;
  }
  return pos;
}

} // namespace

Textbox::Textbox(
    const graphics::FontLibrary &font_lib,
    graphics::GlyphAtlas &glyph_atlas,
    int char_size,
    const std::string &input_string
)
    : font_library_(font_lib), display_text_(font_lib, glyph_atlas, char_size),
      display_text_selected_(font_lib, glyph_atlas, char_size) {
  visible_ = false;
  editing_ = false;
  text_dirty_ = false;
  cursor_dirty_ = false;
  selecting_ = false;
  selection_dirty_ = false;
  masked_ = false;

  width_ = 0.f;
  scroll_x_ = 0.f;

  cursor_size_ = {0.f, 0.f};
  pos_ = {0.f, 0.f};
}

void Textbox::draw(sf::RenderTarget &window) const {
  if (!visible_)
    return;

  const bool clip = width_ > 0.f;
  const sf::View old_view = window.getView();

  if (clip) {
    const auto tl = window.mapCoordsToPixel({pos_.x, pos_.y});
    const auto br = window.mapCoordsToPixel({pos_.x + width_, pos_.y + utils::CMDLINE_HEIGHT});
    const sf::Vector2f target(window.getSize());

    sf::View clipped = old_view;
    clipped.setScissor(
        sf::FloatRect(
            {tl.x / target.x, tl.y / target.y}, {(br.x - tl.x) / target.x, (br.y - tl.y) / target.y}
        )
    );
    window.setView(clipped);
  }

  if (selection_.active())
    window.draw(selection_box_);

  window.draw(display_text_);

  if (selection_.active())
    window.draw(display_text_selected_);

  cursor_.draw(window);

  if (clip)
    window.setView(old_view);
}

void Textbox::update() {
  if (!visible_)
    return;

  cursor_dirty_ |= text_dirty_;
  selection_dirty_ |= text_dirty_;

  if (text_dirty_) {
    display_text_.setString(displayString(u32_text_));
    text_ = utf8::utf32to8(u32_text_);
    text_dirty_ = false;
    draw_flag_ = true;
  }

  if (cursor_dirty_ && updateScroll()) {
    selection_dirty_ = true;
    cursor_dirty_ = true;
    draw_flag_ = true;
  }

  display_text_.setPosition({pos_.x - scroll_x_, pos_.y});

  if (selection_dirty_) {
    rebuildDisplayTexts();
    createSelectionBox();
    selection_dirty_ = false;
    draw_flag_ = true;
  }

  cursor_.update();

  if (cursor_dirty_) {
    cursor_.setPosition({getCursorX(selection_.caret), pos_.y});
    cursor_dirty_ = false;
    draw_flag_ = true;
  }
}

void Textbox::handleEvent(const sf::Event &event) {
  if (!visible_ || !editing_)
    return;

  if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
    switch (key->code) {
    case sf::Keyboard::Key::Backspace:
      if (selection_.active())
        deleteSelection();
      else if (key->control)
        ctrlBackspace();
      else
        backspace();
      break;

    case sf::Keyboard::Key::Delete:
      if (selection_.active())
        deleteSelection();
      else if (key->control)
        ctrlDel();
      else
        del();
      break;

    case sf::Keyboard::Key::A:
      if (key->control)
        selectAll();
      break;

    case sf::Keyboard::Key::C:
      if (key->control && selection_.active() && !masked_)
        sf::Clipboard::setString(getSelectedText());
      break;

    case sf::Keyboard::Key::X:
      if (key->control && selection_.active() && !masked_) {
        sf::Clipboard::setString(getSelectedText());
        deleteSelection();
      }
      break;

    case sf::Keyboard::Key::V:
      if (key->control) {
        if (selection_.active())
          deleteSelection();

        auto clip = sf::Clipboard::getString().toUtf32();
        u32_text_.insert(selection_.caret, clip);

        setCursorPosition(selection_.caret + clip.size());
        clearSelection();

        text_dirty_ = true;
      }
      break;

    case sf::Keyboard::Key::Left:
      if (key->shift) {
        if (key->control)
          ctrlArrowLeft();
        else
          setCursorPosition(prevIndex(selection_.caret));

        selection_dirty_ = true;
      } else if (selection_.active()) {
        setCursorPosition(selection_.begin());
        clearSelection();
      } else if (key->control) {
        ctrlArrowLeft();
      } else {
        setCursorPosition(clusterStart(u32_text_, selection_.caret));
        clearSelection();
      }
      break;

    case sf::Keyboard::Key::Right:
      if (key->shift) {
        if (key->control)
          ctrlArrowRight();
        else
          setCursorPosition(nextIndex(selection_.caret));

        selection_dirty_ = true;
      } else if (selection_.active()) {
        setCursorPosition(selection_.end());
        clearSelection();
      } else if (key->control) {
        ctrlArrowRight();
      } else {
        setCursorPosition(clusterEnd(u32_text_, selection_.caret));
        clearSelection();
      }
      break;

    default:
      break;
    }

    cursor_.typing();
  } else if (const auto *text = event.getIf<sf::Event::TextEntered>()) {
    if (text->unicode >= 32 && text->unicode != 127) {
      if (selection_.active())
        deleteSelection();

      u32_text_.insert(u32_text_.begin() + selection_.caret, static_cast<char32_t>(text->unicode));

      setCursorPosition(selection_.caret + 1);
      clearSelection();

      text_dirty_ = true;
    }

    cursor_.typing();
  }

  if (const auto *mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouse->button == sf::Mouse::Button::Left) {
      auto pos = snapToCluster(getCharacterIndex(mouse->position));

      selection_.anchor = pos;
      setCursorPosition(pos);

      selecting_ = true;
      selection_dirty_ = true;
    }
  }

  if (const auto *move = event.getIf<sf::Event::MouseMoved>()) {
    if (selecting_) {
      setCursorPosition(snapToCluster(getCharacterIndex(move->position)));
      selection_dirty_ = true;
    }
  }

  if (const auto *mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
    if (mouse->button == sf::Mouse::Button::Left)
      selecting_ = false;
  }
}

bool Textbox::getAndConsumeDrawFlag() {
  bool cursor = cursor_.getAndConsumeDrawFlag();
  bool flag = draw_flag_ || (visible_ && cursor);
  draw_flag_ = false;
  return flag;
}

void Textbox::setCursorSize(const sf::Vector2f &cursor_size) {
  cursor_size_ = cursor_size;
  cursor_dirty_ = true;
  draw_flag_ = true;
}

void Textbox::setPosition(const sf::Vector2f &pos) {
  pos_ = pos;
  cursor_dirty_ = true;
  selection_dirty_ = true;
}

sf::Vector2f Textbox::getPosition() const {
  return pos_;
}

void Textbox::show() {
  if (visible_)
    return;
  visible_ = true;
  draw_flag_ = true;
}

void Textbox::hide() {
  if (!visible_)
    return;
  visible_ = false;
  draw_flag_ = true;
}

void Textbox::startEditing() {
  if (editing_)
    return;
  editing_ = true;
  cursor_.start();
  draw_flag_ = true;
}

void Textbox::stopEditing() {
  if (!editing_)
    return;
  editing_ = false;
  cursor_.stop();
  draw_flag_ = true;
}

std::size_t Textbox::size() {
  return u32_text_.size();
}

void Textbox::clear() {
  u32_text_.clear();
  text_dirty_ = true;
}

void Textbox::reset() {
  clear();
  setCursorPosition(0);
  selection_.anchor = 0;
}

void Textbox::setText(const std::string &text) {
  setText(utf8::utf8to32(text));
}

void Textbox::setText(const std::u32string &text) {
  u32_text_ = text;
  setCursorPosition(u32_text_.size());
  clearSelection();
  text_dirty_ = true;
}

const std::string &Textbox::getText() const {
  return text_;
}

const std::u32string &Textbox::getTextU32() const {
  return u32_text_;
}

void Textbox::setMask(bool enabled, char32_t mask_char) {
  masked_ = enabled;
  mask_char_ = mask_char;

  text_dirty_ = true;
  cursor_dirty_ = true;
  selection_dirty_ = true;
}

bool Textbox::isMasked() const {
  return masked_;
}

void Textbox::setTextColor(const sf::Color &color) {
  display_text_.setFillColor(color);
  cursor_.setColor(color);
  draw_flag_ = true;
}

void Textbox::setHighlightColor(const sf::Color &color) {
  selection_box_.setFillColor(color);
  draw_flag_ = true;
}

void Textbox::setHighlightTextColor(const sf::Color &color) {
  display_text_selected_.setFillColor(color);
  draw_flag_ = true;
}

std::u32string Textbox::displayString(const std::u32string &s) const {
  if (!masked_)
    return s;
  return std::u32string(s.size(), mask_char_);
}

std::size_t Textbox::prevIndex(std::size_t pos) const {
  if (masked_)
    return pos > 0 ? pos - 1 : 0;
  return clusterStart(u32_text_, pos);
}

std::size_t Textbox::nextIndex(std::size_t pos) const {
  if (masked_)
    return std::min(pos + 1, u32_text_.size());
  return clusterEnd(u32_text_, pos);
}

void Textbox::setCursorPosition(int pos) {
  selection_.caret = std::clamp(pos, 0, (int)u32_text_.size());
  cursor_dirty_ = true;
}

std::size_t Textbox::getCursorPosition() const {
  return selection_.caret;
}

std::size_t Textbox::findWordStart(std::size_t pos) const {
  if (masked_)
    return 0;

  while (pos > 0 && isSpaceChar(u32_text_[pos - 1]))
    --pos;

  while (pos > 0 && !isWordChar(u32_text_[pos - 1]) && !isSpaceChar(u32_text_[pos - 1]))
    --pos;

  while (pos > 0 && isWordChar(u32_text_[pos - 1]))
    --pos;

  return pos;
}

std::size_t Textbox::findWordEnd(std::size_t pos) const {
  if (masked_)
    return u32_text_.size();

  while (pos < u32_text_.size() && isSpaceChar(u32_text_[pos]))
    ++pos;

  while (pos < u32_text_.size() && !isWordChar(u32_text_[pos]) && !isSpaceChar(u32_text_[pos]))
    ++pos;

  while (pos < u32_text_.size() && isWordChar(u32_text_[pos]))
    ++pos;

  return pos;
}

void Textbox::backspace() {
  if (selection_.caret == 0)
    return;

  std::size_t start = prevIndex(selection_.caret);
  u32_text_.erase(start, selection_.caret - start);
  setCursorPosition(start);
  clearSelection();
  text_dirty_ = true;
}

void Textbox::del() {
  if (selection_.caret >= u32_text_.size()) {
    backspace();
    return;
  }

  std::size_t end = nextIndex(selection_.caret);
  u32_text_.erase(selection_.caret, end - selection_.caret);
  clearSelection();
  text_dirty_ = true;
}

std::size_t Textbox::snapToCluster(std::size_t idx) const {
  idx = std::min(idx, u32_text_.size());
  if (masked_)
    return idx;
  std::size_t s = clusterStart(u32_text_, idx);
  return clusterEnd(u32_text_, s) == idx ? idx : s;
}

void Textbox::ctrlBackspace() {
  if (selection_.caret == 0)
    return;

  std::size_t start = findWordStart(selection_.caret);
  u32_text_.erase(start, selection_.caret - start);
  setCursorPosition(start);
  clearSelection();
  text_dirty_ = true;
}

void Textbox::ctrlDel() {
  if (selection_.caret >= u32_text_.size()) {
    ctrlBackspace();
    return;
  }

  std::size_t end = findWordEnd(selection_.caret);
  u32_text_.erase(selection_.caret, end - selection_.caret);
  clearSelection();
  text_dirty_ = true;
}

void Textbox::ctrlArrowLeft() {
  setCursorPosition(findWordStart(selection_.caret));
  clearSelection();
}

void Textbox::ctrlArrowRight() {
  setCursorPosition(findWordEnd(selection_.caret));
  clearSelection();
}

void Textbox::selectAll() {
  selection_.anchor = 0;
  setCursorPosition(u32_text_.size());

  selection_dirty_ = true;
  cursor_dirty_ = true;
}

std::size_t Textbox::getCharacterIndex(const sf::Vector2i &mouse) {
  const std::size_t n = display_text_.getCaretCount();
  if (n == 0)
    return 0;

  const float x = display_text_.getInverseTransform().transformPoint(sf::Vector2f(mouse)).x;

  std::size_t best = 0;
  float best_d = std::abs(x - display_text_.getCaretX(0));
  for (std::size_t i = 1; i < n; ++i) {
    float d = std::abs(x - display_text_.getCaretX(i));
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

void Textbox::setWidth(float width) {
  width_ = std::max(0.f, width);
  cursor_dirty_ = true;
  selection_dirty_ = true;
}

float Textbox::getCursorX(std::size_t index) {
  return pos_.x - scroll_x_ + getLocalX(index);
}

void Textbox::createSelectionBox() {
  float start_pos = getCursorX(selection_.begin());
  float end_pos = getCursorX(selection_.end());

  const float left = (start_pos < end_pos) ? start_pos : end_pos;
  const float size = std::abs(start_pos - end_pos);

  selection_box_.setSize({size, utils::CMDLINE_HEIGHT});
  selection_box_.setPosition({left, pos_.y});
}

void Textbox::rebuildDisplayTexts() {
  const auto first = selection_.begin();
  const auto last = selection_.end();

  std::u32string selected = u32_text_.substr(first, last - first);
  display_text_selected_.setString(displayString(selected));

  sf::Vector2f pos = {getCursorX(first), pos_.y};
  display_text_selected_.setPosition(pos);
}

void Textbox::deleteSelection() {
  if (!selection_.active())
    return;

  const auto first = selection_.begin();
  const auto last = selection_.end();
  u32_text_.erase(first, last - first);

  setCursorPosition(first);
  clearSelection();

  text_dirty_ = true;
  cursor_dirty_ = true;
}

void Textbox::clearSelection() {
  selection_.anchor = selection_.caret;
  selection_dirty_ = true;
}

std::u32string Textbox::getSelectedText() const {
  const auto first = selection_.begin();
  const auto last = selection_.end();
  return u32_text_.substr(first, last - first);
}

float Textbox::getLocalX(std::size_t index) const {
  return display_text_.getCaretX(index);
}

bool Textbox::updateScroll() {
  const float old = scroll_x_;

  if (width_ <= 0.f) {
    scroll_x_ = 0.f;
    return old != scroll_x_;
  }

  const float cursor_w = cursor_size_.x;
  const float visible = std::max(0.f, width_ - cursor_w);
  const float caret = getLocalX(selection_.caret);
  const float text_w = getLocalX(u32_text_.size());

  if (caret - scroll_x_ > visible)
    scroll_x_ = caret - visible;
  if (caret < scroll_x_)
    scroll_x_ = caret;

  const float max_scroll = std::max(0.f, text_w + cursor_w - width_);
  scroll_x_ = std::clamp(scroll_x_, 0.f, max_scroll);

  return old != scroll_x_;
}

} // namespace ui
