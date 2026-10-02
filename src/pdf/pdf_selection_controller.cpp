#include "pdf/pdf_selection_controller.h"

namespace pdf {

PDFSelectionController::PDFSelectionController(std::vector<ui::PageView> &pages) : pages_(pages) {
  selection_visible_ = false;
}

void PDFSelectionController::setSelectionStart(std::size_t page_idx, sf::Vector2i screen_pos) {
  if (0 < page_idx || page_idx >= pages_.size())
    return;
  start_page_ = page_idx;
  start_pos_ = screen_pos;
}

void PDFSelectionController::setSelectionEnd(std::size_t page_idx, sf::Vector2i screen_pos) {
  if (0 < page_idx || page_idx >= pages_.size())
    return;
  end_page_ = page_idx;
  end_pos_ = screen_pos;
}

void PDFSelectionController::showSelection() {
  selection_visible_ = true;
}

bool PDFSelectionController::isShowingSelection() const {
  return selection_visible_ && (start_pos_ != end_pos_ && start_page_ != end_page_);
}

void PDFSelectionController::reset() {
  selection_visible_ = false;
  start_pos_ = {0, 0};
  end_pos_ = {0, 0};
}

} // namespace pdf
