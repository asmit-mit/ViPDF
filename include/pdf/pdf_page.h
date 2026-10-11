#pragma once

#include <SFML/Graphics.hpp>
#include <mupdf/fitz.h>

#include <mutex>
#include <string>
#include <vector>

namespace pdf {

enum class PDFSelectionDirection : int {
  UP,
  DOWN,
  ALL,
  INSIDE,
};

class PDFPage {
public:
  PDFPage() = default;
  PDFPage(std::size_t idx, fz_rect bounds);

  PDFPage(const PDFPage &) = delete;
  PDFPage &operator=(const PDFPage &) = delete;
  PDFPage(PDFPage &&) noexcept;
  PDFPage &operator=(PDFPage &&) noexcept;

  fz_rect getPageBounds() const;
  void loadContent(fz_stext_page *page_text);
  bool isContentLoaded() const;

  std::vector<fz_rect> searchText(const std::u32string &pattern) const;
  sf::VertexArray getSelectionBoxes(
      PDFSelectionDirection direction,
      std::optional<sf::Vector2f> mouse_pos1,
      std::optional<sf::Vector2f> mouse_pos2
  ) const;

private:
  mutable std::mutex mutex_;
  std::size_t page_idx_{};
  fz_rect page_bounds_{};

  std::u32string text_;
  std::u32string text_normalized_;
  std::vector<fz_rect> glyphs_;

  bool content_loaded_{false};
};

} // namespace pdf
