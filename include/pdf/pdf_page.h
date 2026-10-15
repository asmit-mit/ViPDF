#pragma once

#include <SFML/Graphics.hpp>
#include <mupdf/fitz.h>

#include <cstddef>
#include <mutex>
#include <optional>
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

  std::vector<std::vector<fz_rect>> searchText(const std::u32string &pattern) const;
  sf::VertexArray getSelectionBoxes(
      PDFSelectionDirection direction,
      std::optional<sf::Vector2f> mouse_pos1,
      std::optional<sf::Vector2f> mouse_pos2
  ) const;

private:
  char32_t normalizeChar(char32_t cp) const;
  bool endsWithSpace(const std::u32string &text);
  void pushBreak(std::u32string &text, std::vector<fz_rect> &rects);
  void appendSTextPage(std::u32string &text, std::vector<fz_rect> &rects, fz_stext_page *page_text);

  std::u32string normalizePattern(const std::u32string &pattern) const;
  std::vector<fz_rect> collectSegments(std::size_t pos, std::size_t len) const;
  std::vector<std::size_t> buildFailure(const std::u32string &pat) const;

  bool isBefore(const fz_rect &r, sf::Vector2f p) const;
  bool isSelected(
      PDFSelectionDirection direction, const fz_rect &r, sf::Vector2f p1, sf::Vector2f p2
  ) const;
  void flushRun(sf::VertexArray &boxes, const fz_rect &run, bool &have, sf::Color color) const;

  mutable std::mutex mutex_;
  std::size_t page_idx_{};
  fz_rect page_bounds_{};

  std::u32string text_;
  std::vector<fz_rect> glyphs_;

  bool content_loaded_{false};
};

} // namespace pdf
