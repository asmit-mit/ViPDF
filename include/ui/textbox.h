#pragma once

#include "graphics/rich_text.h"
#include "ui/cursor.h"
#include "ui/widget.h"

namespace ui {

class Textbox : public Widget {
public:
  explicit Textbox(
      const graphics::FontLibrary &font_lib,
      graphics::GlyphAtlas &glyph_atlas,
      int char_size,
      const std::string &input_string
  );

  void draw(sf::RenderTarget &window) const override;
  void update() override;
  void handleEvent(const sf::Event &event) override;

  void setCursorSize(const sf::Vector2f &cursor_size);
  void setPosition(const sf::Vector2f &pos);
  sf::Vector2f getPosition() const;

  void show();
  void hide();
  void startEditing();
  void stopEditing();

  std::size_t size();
  void clear();
  void reset();

  void setText(const std::string &text);
  void setText(const std::u32string &text);
  const std::string &getText() const;
  const std::u32string &getTextU32() const;

  float getCursorX(std::size_t idx);
  void setCursorPosition(int pos);
  std::size_t getCursorPosition() const;

private:
  std::size_t findWordStart(std::size_t) const;
  std::size_t findWordEnd(std::size_t) const;

  void backspace();
  void del();
  void ctrlBackspace();
  void ctrlDel();
  void ctrlArrowRight();
  void ctrlArrowLeft();
  void selectAll();

  std::size_t getCharacterIndex(const sf::Vector2i &pos);
  std::size_t snapToCluster(std::size_t idx) const; // new
  void createSelectionBox();
  void rebuildDisplayTexts();

  void deleteSelection();
  void clearSelection();
  std::u32string getSelectedText() const;

private:
  struct Selection {
    std::size_t anchor = 0;
    std::size_t caret = 0;

    bool active() const {
      return anchor != caret;
    }

    std::size_t begin() const {
      return std::min(anchor, caret);
    }

    std::size_t end() const {
      return std::max(anchor, caret);
    }
  };

private:
  const graphics::FontLibrary &font_library_;
  sf::RectangleShape selection_box_;

  sf::Vector2f pos_;

  ui::Cursor cursor_;
  sf::Vector2f cursor_size_;
  bool cursor_dirty_;

  graphics::RichText display_text_;
  graphics::RichText display_text_selected_;

  std::string text_;
  std::u32string u32_text_;
  bool text_dirty_;

  Selection selection_;
  bool selection_dirty_;
  bool selecting_;

  bool visible_;
  bool editing_;
};

} // namespace ui
