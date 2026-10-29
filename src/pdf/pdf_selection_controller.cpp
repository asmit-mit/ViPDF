#include "pdf/pdf_selection_controller.h"

#include <SFML/Graphics/VertexArray.hpp>
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

bool PDFSelectionController::isSelecting() const {
  return start_page_ != kNone;
}

void PDFSelectionController::setSelectionStart(
    std::size_t page_idx, const sf::Vector2i &screen_pos
) {
  if (page_idx >= pages_.size())
    return;
  if (!document_.getPage(page_idx).isContentLoaded())
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
  if (start_page_ == kNone || page_idx >= pages_.size())
    return;
  if (!document_.getPage(page_idx).isContentLoaded())
    document_.loadPageContent(page_idx);

  end_page_ = page_idx;
  end_point_ = pages_[page_idx].getReferenceShape().getInverseTransform().transformPoint(
      sf::Vector2f(screen_pos)
  );

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
      pages_[i].setSelections(sf::VertexArray{sf::PrimitiveType::Triangles});
      continue;
    }

    if (first_page == last_page)
      pages_[i].setSelections(
          document_.getSelectionBoxesFromPage(i, PDFSelectionDirection::INSIDE, p1, p2)
      );
    else if (i == first_page)
      pages_[i].setSelections(
          document_.getSelectionBoxesFromPage(i, PDFSelectionDirection::DOWN, p1, std::nullopt)
      );
    else if (i == last_page)
      pages_[i].setSelections(
          document_.getSelectionBoxesFromPage(i, PDFSelectionDirection::UP, p2, std::nullopt)
      );
    else
      pages_[i].setSelections(
          document_
              .getSelectionBoxesFromPage(i, PDFSelectionDirection::ALL, std::nullopt, std::nullopt)
      );
  }
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
      part = document_.copySelectionFromPage(i, PDFSelectionDirection::DOWN, p1, std::nullopt);
    else if (i == last_page)
      part = document_.copySelectionFromPage(i, PDFSelectionDirection::UP, p2, std::nullopt);
    else
      part = document_
                 .copySelectionFromPage(i, PDFSelectionDirection::ALL, std::nullopt, std::nullopt);

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
  return start_point_ != end_point_ || start_page_ != end_page_;
}

void PDFSelectionController::reset() {
  selection_visible_ = false;
  start_page_ = kNone;
  end_page_ = kNone;
  start_point_ = {0.f, 0.f};
  end_point_ = {0.f, 0.f};
  for (auto &page : pages_)
    page.setSelections(sf::VertexArray{sf::PrimitiveType::Triangles});
  document_.clearSelections();
}

} // namespace pdf
