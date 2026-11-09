#pragma once

#include <SFML/Graphics.hpp>
#include <mupdf/fitz.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pdf {

enum class PDFSelectionDirection {
  UP,
  DOWN,
  ALL,
  INSIDE,
};

enum class PDFLinkType {
  INTERNAL,
  EXTERNAL,
};

struct PDFLinkInfo {
  PDFLinkType type;
  int target_page{-1};
  float x, y;
  std::string url;
};

struct FzStextDeleter {
  fz_context *ctx{nullptr};
  void operator()(fz_stext_page *stext) const noexcept {
    if (stext && ctx)
      fz_drop_stext_page(ctx, stext);
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

  void loadContent(fz_context *ctx, fz_document *doc, FzStextPagePtr stext, fz_link *links);
  bool isContentLoaded() const;

  fz_stext_page *stext() const;
  void resetStext();

  std::vector<fz_quad> searchText(fz_context *ctx, const std::string &needle) const;
  const std::vector<fz_quad> &selectPage(fz_context *ctx, fz_point a, fz_point b) const;
  std::string copySelection(fz_context *ctx, fz_point a, fz_point b) const;

  bool hasSelections() const;
  void clearSelections();
  const std::vector<fz_quad> &getSelectionFzQuads() const;
  bool mouseInText(sf::Vector2f pos) const;
  std::optional<PDFLinkInfo> mouseInLink(sf::Vector2f pos) const;
  fz_image *mouseInImage(sf::Vector2f pos) const;

private:
  struct TextRect {
    fz_rect rect;
    float line_mid_y;
  };

  struct ImageRect {
    fz_rect rect;
    fz_image *img;
  };

  struct LinkRect {
    fz_rect rect;
    PDFLinkInfo info;
  };

private:
  void resolveLinkPage(fz_context *ctx, fz_document *doc, const char *uri, PDFLinkInfo &info);

private:
  mutable std::mutex mutex_;
  std::size_t page_idx_{};
  fz_rect page_bounds_{};

  FzStextPagePtr stext_{};

  std::vector<TextRect> text_rects_{};
  std::vector<LinkRect> link_rects_{};
  std::vector<ImageRect> img_rects_{};
  float max_reach_{0.f};

  mutable std::vector<fz_quad> selection_quads_{};

  bool text_content_loaded_{false};
};

} // namespace pdf
