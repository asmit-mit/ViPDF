#pragma once

#include <SFML/Graphics.hpp>
#include <mupdf/fitz.h>

#include <cstddef>
#include <memory>
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

struct FzStextDeleter {
  fz_context *ctx{nullptr};
  void operator()(fz_stext_page *p) const noexcept {
    if (p && ctx)
      fz_drop_stext_page(ctx, p);
  }
};
using FzStextPagePtr = std::unique_ptr<fz_stext_page, FzStextDeleter>;

class PDFPage {
public:
  PDFPage() = default;
  PDFPage(std::size_t idx, fz_rect bounds);

  PDFPage(const PDFPage &) = delete;
  PDFPage &operator=(const PDFPage &) = delete;
  PDFPage(PDFPage &&) noexcept;
  PDFPage &operator=(PDFPage &&) noexcept;

  fz_rect getPageBounds() const;

  void loadContent(FzStextPagePtr stext);
  bool isContentLoaded() const;

  fz_stext_page *stext() const;
  void resetStext();

  std::vector<fz_quad> searchText(fz_context *ctx, const std::string &needle) const;
  const std::vector<fz_quad> &selectPage(fz_context *ctx, fz_point a, fz_point b) const;

  bool hasSelections() const;
  void clearSelections();
  const std::vector<fz_quad> &getSelectionFzQuads() const;
  bool mouseInGlyph(sf::Vector2f pos) const;

private:
  mutable std::mutex mutex_;
  std::size_t page_idx_{};
  fz_rect page_bounds_{};

  FzStextPagePtr stext_{};

  struct SortedGlyph {
    float line_mid_y;
    fz_rect rect;
  };

  std::vector<SortedGlyph> sorted_glyphs_{};
  float max_reach_{0.f};

  mutable std::vector<fz_quad> selection_quads_{};

  bool content_loaded_{false};
};

} // namespace pdf
