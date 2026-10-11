#pragma once

#include <optional>
#include <vector>

#include "pdf/pdf_document.h"
#include "ui/page_view.h"

namespace pdf {

class PDFSearchController {
public:
  PDFSearchController(std::vector<ui::PageView> &pages);

  std::optional<std::size_t>
  search(const std::u32string &text, std::size_t anchor, pdf::PDFDocument &document);
  std::size_t getCurrSearchResult() const;
  std::size_t getLocalSearchResult() const;
  std::size_t getTotalSearchResult() const;

  bool isShowingSearchResult() const;
  bool isSearchPosDirty() const;

  void clearSearchPosDirty();

  void reset();

  std::optional<int> goNext(std::size_t anchor);
  std::optional<int> goPrev(std::size_t anchor);

private:
  std::vector<ui::PageView> &pages_;

  std::size_t curr_search_result_;
  std::size_t local_search_result_;
  std::size_t total_search_results_;

  std::vector<std::size_t> pages_with_search_results_;

  int selected_page_;
  bool show_search_result_boxes_;
  bool search_pos_dirty_;
};

} // namespace pdf
