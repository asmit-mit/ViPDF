#pragma once

#include <vector>

#include "ui/page_view.h"

namespace pdf {

class PDFSelectionController {
public:
  PDFSelectionController(std::vector<ui::PageView> &pages);

  bool isSelecting() const;
  void setSelectionStart(std::size_t page_idx, sf::Vector2i screen_pos);
  void setSelectionEnd(std::size_t page_idx, sf::Vector2i screen_pos);

  void showSelection();
  bool isShowingSelection() const;

  void reset();

private:
  std::vector<ui::PageView> &pages_;

  bool selection_visible_;
  std::size_t start_page_;
  std::size_t end_page_;
  sf::Vector2i start_pos_;
  sf::Vector2i end_pos_;
};

} // namespace pdf
