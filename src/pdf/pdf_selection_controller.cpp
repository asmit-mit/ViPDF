#include "pdf/pdf_selection_controller.h"

#include <SFML/Graphics/VertexArray.hpp>
#include <algorithm>
#include <optional>

#include "pdf/pdf_page.h"

namespace pdf {

PDFSelectionController::PDFSelectionController(std::vector<ui::PageView> &pages) : pages_(pages) {
  reset();
}

bool PDFSelectionController::isSelecting() const {
  return start_page_ != kNone;
}

void PDFSelectionController::setSelectionStart(
    std::size_t page_idx, const sf::Vector2i &screen_pos, PDFDocument &document
) {
  if (page_idx >= pages_.size())
    return;
  if (!document.getPage(page_idx).isContentLoaded())
    document.loadPageContent(page_idx);
  start_page_ = page_idx;
  start_pos_ = screen_pos;
  end_page_ = page_idx;
  end_pos_ = screen_pos;
}

void PDFSelectionController::setSelectionEnd(
    std::size_t page_idx, const sf::Vector2i &screen_pos, PDFDocument &document
) {
  if (start_page_ == kNone || page_idx >= pages_.size())
    return;
  if (!document.getPage(page_idx).isContentLoaded())
    document.loadPageContent(page_idx);
  end_page_ = page_idx;
  end_pos_ = screen_pos;

  std::size_t first_page = start_page_;
  std::size_t last_page = end_page_;
  sf::Vector2i first_pos = start_pos_;
  sf::Vector2i last_pos = end_pos_;
  if (first_page > last_page) {
    std::swap(first_page, last_page);
    std::swap(first_pos, last_pos);
  }

  const std::optional<sf::Vector2f> p1 =
      pages_[first_page].getReferenceShape().getInverseTransform().transformPoint(
          sf::Vector2f(first_pos)
      );
  const std::optional<sf::Vector2f> p2 =
      pages_[last_page].getReferenceShape().getInverseTransform().transformPoint(
          sf::Vector2f(last_pos)
      );

  for (std::size_t i = 0; i < pages_.size(); ++i) {
    if (i < first_page || i > last_page) {
      pages_[i].setSelections(sf::VertexArray{sf::PrimitiveType::Triangles});
      continue;
    }

    if (!document.getPage(i).isContentLoaded())
      document.loadPageContent(i);
    const PDFPage &page = document.getPage(i);

    if (first_page == last_page)
      pages_[i].setSelections(page.getSelectionBoxes(PDFSelectionDirection::INSIDE, p1, p2));
    else if (i == first_page)
      pages_[i].setSelections(page.getSelectionBoxes(PDFSelectionDirection::DOWN, p1, std::nullopt));
    else if (i == last_page)
      pages_[i].setSelections(page.getSelectionBoxes(PDFSelectionDirection::UP, p2, std::nullopt));
    else
      pages_[i].setSelections(
          page.getSelectionBoxes(PDFSelectionDirection::ALL, std::nullopt, std::nullopt)
      );
  }
}

void PDFSelectionController::showSelection() {
  selection_visible_ = true;
}

bool PDFSelectionController::isShowingSelection() const {
  if (!selection_visible_ || start_page_ == kNone)
    return false;
  return start_pos_ != end_pos_ || start_page_ != end_page_;
}

void PDFSelectionController::reset() {
  selection_visible_ = false;
  start_page_ = kNone;
  end_page_ = kNone;
  start_pos_ = {0, 0};
  end_pos_ = {0, 0};
  for (auto &page : pages_)
    page.setSelections(sf::VertexArray{sf::PrimitiveType::Triangles});
}

} // namespace pdf
