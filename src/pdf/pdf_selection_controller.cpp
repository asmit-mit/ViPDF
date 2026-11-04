#include "pdf/pdf_selection_controller.h"

#include <algorithm>
#include <optional>

#include "pdf/pdf_page.h"

namespace pdf {

PDFSelectionController::PDFSelectionController(
    PDFDocument &document_, std::vector<ui::PageView> &pages
)
    : pages_(pages), document_(document_) {
  reset();
}

bool PDFSelectionController::isSelectionDirty() const {
  return selection_dirty_;
}

bool PDFSelectionController::isSelectionEmpty() const {
  return start_page_ != kNone && start_page_ == end_page_ && start_point_ == end_point_;
}

void PDFSelectionController::setSelectedImage(fz_image *img) {
  selected_image_ = img;
}

fz_image *PDFSelectionController::getSelectedImage() const {
  return selected_image_;
}

void PDFSelectionController::setSelectionStart(
    std::size_t page_idx, const sf::Vector2i &screen_pos
) {
  if (page_idx >= pages_.size())
    return;
  document_.loadPageContent(page_idx);

  const sf::Vector2f point =
      pages_[page_idx].getReferenceShape().getInverseTransform().transformPoint(
          sf::Vector2f(screen_pos)
      );

  start_page_ = page_idx;
  start_point_ = point;
  end_page_ = page_idx;
  end_point_ = point;
}

void PDFSelectionController::setSelectionEnd(std::size_t page_idx, const sf::Vector2i &screen_pos) {
  if (page_idx >= pages_.size())
    return;
  document_.loadPageContent(page_idx);

  end_page_ = page_idx;
  end_point_ = pages_[page_idx].getReferenceShape().getInverseTransform().transformPoint(
      sf::Vector2f(screen_pos)
  );

  selection_dirty_ = true;
}

std::pair<std::size_t, sf::Vector2f> PDFSelectionController::getSelectionStart() const {
  return {start_page_, start_point_};
}

std::pair<std::size_t, sf::Vector2f> PDFSelectionController::getSelectionEnd() const {
  return {end_page_, end_point_};
}

void PDFSelectionController::createSelection() {
  if (isSelectionEmpty())
    return;

  std::size_t first_page = start_page_;
  std::size_t last_page = end_page_;
  sf::Vector2f first_point = start_point_;
  sf::Vector2f last_point = end_point_;
  if (first_page > last_page) {
    std::swap(first_page, last_page);
    std::swap(first_point, last_point);
  }

  const std::optional<sf::Vector2f> p1 = first_point;
  const std::optional<sf::Vector2f> p2 = last_point;

  for (std::size_t i = 0; i < pages_.size(); ++i) {
    if (i < first_page || i > last_page) {
      pages_[i].clearSelections();
      continue;
    }

    if (first_page == last_page)
      pages_[i].setSelections(
          document_.getSelectionBoxesFromPage(i, PDFSelectionDirection::INSIDE, p1, p2)
      );
    else if (i == first_page)
      pages_[i].setSelections(
          document_.getSelectionBoxesFromPage(i, PDFSelectionDirection::DOWN, p1)
      );
    else if (i == last_page)
      pages_[i].setSelections(document_.getSelectionBoxesFromPage(i, PDFSelectionDirection::UP, p2));
    else
      pages_[i].setSelections(document_.getSelectionBoxesFromPage(i, PDFSelectionDirection::ALL));
  }

  selection_dirty_ = false;
}

std::string PDFSelectionController::copySelection() {
  if (!isShowingSelection())
    return {};

  std::size_t first_page = start_page_, last_page = end_page_;
  sf::Vector2f first_point = start_point_, last_point = end_point_;
  if (first_page > last_page) {
    std::swap(first_page, last_page);
    std::swap(first_point, last_point);
  }

  const std::optional<sf::Vector2f> p1 = first_point, p2 = last_point;
  std::string result;

  for (std::size_t i = first_page; i <= last_page; ++i) {
    std::string part;
    if (first_page == last_page)
      part = document_.copySelectionFromPage(i, PDFSelectionDirection::INSIDE, p1, p2);
    else if (i == first_page)
      part = document_.copySelectionFromPage(i, PDFSelectionDirection::DOWN, p1);
    else if (i == last_page)
      part = document_.copySelectionFromPage(i, PDFSelectionDirection::UP, p2);
    else
      part = document_.copySelectionFromPage(i, PDFSelectionDirection::ALL);

    if (part.empty())
      continue;
    if (!result.empty())
      result += '\n';
    result += part;
  }

  return result;
}

void PDFSelectionController::showSelection() {
  selection_visible_ = true;
}

bool PDFSelectionController::isShowingSelection() const {
  if (!selection_visible_ || start_page_ == kNone)
    return false;
  return selected_image_ || start_point_ != end_point_ || start_page_ != end_page_;
}

void PDFSelectionController::reset() {
  selection_visible_ = false;
  selection_dirty_ = false;
  start_page_ = kNone;
  end_page_ = kNone;
  start_point_ = {0.f, 0.f};
  end_point_ = {0.f, 0.f};
  selected_image_ = nullptr;
  for (auto &page : pages_)
    page.clearSelections();
  document_.clearSelections();
}

} // namespace pdf
