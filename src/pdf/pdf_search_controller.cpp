#include "pdf/pdf_search_controller.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <optional>

namespace pdf {

PDFSearchController::PDFSearchController(std::vector<ui::PageView> &pages) : pages_(pages) {
  search_pos_dirty_ = false;
  show_search_result_boxes_ = false;
  selected_page_ = -1;

  curr_search_result_ = 0;
  local_search_result_ = 0;
  total_search_results_ = 0;
}

std::optional<std::size_t> PDFSearchController::search(
    const std::u32string &text, std::size_t anchor, pdf::PDFDocument &document
) {
  if (!document.isOpen())
    return std::nullopt;

  if (!document.isAllContentLoaded())
    document.loadAllContent();

  pages_with_search_results_.clear();
  int closest_page = -1;
  int closest_distance = std::numeric_limits<int>::max();

  std::size_t global_start = 0;
  for (std::size_t i = 0; i < document.pageCount(); i++) {
    pages_[i].clearSearchResults();
    ui::PDFSearchResult res;
    res.start = global_start;
    res.local_rects = document.getPage(i).searchText(text);
    global_start += res.local_rects.size();

    if (!res.local_rects.empty()) {
      pages_with_search_results_.push_back(i);
      int distance = std::abs(static_cast<int>(i) - static_cast<int>(anchor));
      if (distance < closest_distance) {
        closest_distance = distance;
        closest_page = static_cast<int>(i);
      }
    }

    pages_[i].setSearchResults(std::move(res));
  }

  if (closest_page == -1) {
    curr_search_result_ = 0;
    local_search_result_ = 0;
    total_search_results_ = 0;
    selected_page_ = -1;

    search_pos_dirty_ = false;
    show_search_result_boxes_ = false;

    return std::nullopt;
  }

  curr_search_result_ = pages_[closest_page].getSearchResults().start;
  local_search_result_ = 0;
  total_search_results_ = global_start;
  pages_[closest_page].setSelectedSearchResult(local_search_result_);
  selected_page_ = closest_page;

  search_pos_dirty_ = true;
  show_search_result_boxes_ = true;

  return closest_page;
}

std::size_t PDFSearchController::getCurrSearchResult() const {
  return curr_search_result_;
}

std::size_t PDFSearchController::getLocalSearchResult() const {
  return local_search_result_;
}

std::size_t PDFSearchController::getTotalSearchResult() const {
  return total_search_results_;
}

bool PDFSearchController::isShowingSearchResult() const {
  return show_search_result_boxes_;
}

bool PDFSearchController::isSearchPosDirty() const {
  return search_pos_dirty_;
}

void PDFSearchController::clearSearchPosDirty() {
  search_pos_dirty_ = false;
}

void PDFSearchController::reset() {
  if (selected_page_ >= 0)
    pages_[selected_page_].setSelectedSearchResult(-1);
  selected_page_ = -1;

  curr_search_result_ = 0;
  local_search_result_ = 0;
  total_search_results_ = 0;
  show_search_result_boxes_ = false;
  for (auto &page : pages_)
    page.clearSearchResults();
}

std::optional<int> PDFSearchController::goNext(std::size_t anchor) {
  if (total_search_results_ == 0)
    return std::nullopt;

  const std::size_t page = selected_page_ >= 0 ? static_cast<std::size_t>(selected_page_) : anchor;
  pages_[page].setSelectedSearchResult(-1);

  if (local_search_result_ + 1 < pages_[page].getSearchResults().local_rects.size()) {
    curr_search_result_++;
    local_search_result_++;
    pages_[page].setSelectedSearchResult(local_search_result_);
    selected_page_ = static_cast<int>(page);

    search_pos_dirty_ = true;
    show_search_result_boxes_ = true;
    return std::nullopt;
  }

  auto it =
      std::upper_bound(pages_with_search_results_.begin(), pages_with_search_results_.end(), page);
  if (it == pages_with_search_results_.end())
    it = pages_with_search_results_.begin();

  const std::size_t next = *it;
  curr_search_result_ = pages_[next].getSearchResults().start;
  local_search_result_ = 0;
  pages_[next].setSelectedSearchResult(local_search_result_);
  selected_page_ = static_cast<int>(next);

  search_pos_dirty_ = true;
  show_search_result_boxes_ = true;

  return static_cast<int>(next);
}

std::optional<int> PDFSearchController::goPrev(std::size_t anchor) {
  if (total_search_results_ == 0)
    return std::nullopt;

  const std::size_t page = selected_page_ >= 0 ? static_cast<std::size_t>(selected_page_) : anchor;
  pages_[page].setSelectedSearchResult(-1);

  if (local_search_result_ > 0) {
    curr_search_result_--;
    local_search_result_--;
    pages_[page].setSelectedSearchResult(local_search_result_);
    selected_page_ = static_cast<int>(page);

    search_pos_dirty_ = true;
    show_search_result_boxes_ = true;
    return std::nullopt;
  }

  auto it =
      std::lower_bound(pages_with_search_results_.begin(), pages_with_search_results_.end(), page);
  if (it == pages_with_search_results_.begin())
    it = pages_with_search_results_.end();
  --it;

  const std::size_t prev = *it;
  const std::size_t last_local = pages_[prev].getSearchResults().local_rects.size() - 1;
  curr_search_result_ = pages_[prev].getSearchResults().start + last_local;
  local_search_result_ = last_local;
  pages_[prev].setSelectedSearchResult(local_search_result_);
  selected_page_ = static_cast<int>(prev);

  search_pos_dirty_ = true;
  show_search_result_boxes_ = true;
  return static_cast<int>(prev);
}

} // namespace pdf
