#pragma once

#include <vector>

#include "pdf/pdf_document.h"
#include "ui/page_view.h"

namespace pdf {

class PDFSelectionController {
public:
  PDFSelectionController(PDFDocument &document, std::vector<ui::PageView> &pages);

  bool isSelecting() const;
  void setSelectionStart(std::size_t page_idx, const sf::Vector2i &screen_pos);
  void setSelectionEnd(std::size_t page_idx, const sf::Vector2i &screen_pos);
  std::string copySelection();

  void showSelection();
  bool isShowingSelection() const;

  void reset();

private:
  static constexpr std::size_t kNone = static_cast<std::size_t>(-1);
  std::vector<ui::PageView> &pages_;
  PDFDocument &document_;

  bool selection_visible_;
  std::size_t start_page_;
  std::size_t end_page_;
  sf::Vector2f start_point_;
  sf::Vector2f end_point_;
};

} // namespace pdf
