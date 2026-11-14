#pragma once

#include <SFML/Graphics.hpp>
#include <mupdf/fitz.h>

#include <mutex>

#include "containers/lru_cache.h"
#include "pdf/pdf_document.h"

namespace pdf {

struct PDFRenderKey {
  std::size_t page_idx;
  float zoom;
  int rotate;
  bool recolor;

  PDFRenderKey(std::size_t page_idx, float zoom, int rotate, bool recolor)
      : page_idx(page_idx), zoom(zoom), rotate(rotate), recolor(recolor) {}

  bool operator==(const PDFRenderKey &other) const noexcept {
    return page_idx == other.page_idx && zoom == other.zoom && rotate == other.rotate &&
           recolor == other.recolor;
  }
};

struct PDFRenderKeyHash {
  std::size_t operator()(const PDFRenderKey &key) const noexcept {
    std::size_t seed = 0;

    auto hashCombine = [&seed](std::size_t value) {
      seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    hashCombine(std::hash<std::size_t>{}(key.page_idx));
    hashCombine(std::hash<float>{}(key.zoom));
    hashCombine(std::hash<int>{}(key.rotate));
    hashCombine(std::hash<bool>{}(key.recolor));

    return seed;
  }
};

struct FzRectKey {
  fz_rect bounds;
  PDFRenderKey key;

  bool operator==(const FzRectKey &other) const noexcept {
    return (bounds.x1 - bounds.x0) == (other.bounds.x1 - other.bounds.x0) &&
           (bounds.y1 - bounds.y0) == (other.bounds.y1 - other.bounds.y0) &&
           key.zoom == other.key.zoom && key.rotate == other.key.rotate;
  }
};

struct FzRectHash {
  std::size_t operator()(const FzRectKey &key) const noexcept {
    std::size_t seed = 0;

    auto hashCombine = [&seed](std::size_t value) {
      seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    hashCombine(std::hash<float>{}(key.bounds.x1 - key.bounds.x0));
    hashCombine(std::hash<float>{}(key.bounds.y1 - key.bounds.y0));
    hashCombine(std::hash<float>{}(key.key.zoom));
    hashCombine(std::hash<int>{}(key.key.rotate));

    return seed;
  }
};

struct FzPixmapDeleter {
  fz_context *ctx;
  void operator()(fz_pixmap *pixmap) const noexcept {
    if (pixmap)
      fz_drop_pixmap(ctx, pixmap);
  }
};

using FzPixmap = std::unique_ptr<fz_pixmap, FzPixmapDeleter>;

struct FzDisplayListDeleter {
  fz_context *ctx;
  std::mutex *mtx;

  void operator()(fz_display_list *list) const noexcept {
    if (!list)
      return;
    std::lock_guard lock(*mtx);
    fz_drop_display_list(ctx, list);
  }
};

using FzDisplayList = std::shared_ptr<fz_display_list>;

class PDFRenderer {
public:
  explicit PDFRenderer(FzContextPtr ctx, int32_t recolor_dark, int32_t recolor_light);

  void render(
      fz_context *ctx,
      fz_document *doc,
      const PDFRenderKey &key,
      std::vector<uint8_t> &rgba,
      sf::Image &image
  );
  sf::Vector2u getPageSize(const fz_rect &bounds, const PDFRenderKey &key);
  void invalidatePage(std::size_t page_idx);
  void clearCache();

private:
  fz_display_list *
  getPageDisplayList(std::size_t page_idx, fz_context *ctx, fz_document *doc) const;
  FzDisplayList getOrLoadDisplayList(std::size_t page_idx, fz_context *ctx, fz_document *doc);
  fz_pixmap *
  getPixmapFromDisplayList(fz_context *ctx, fz_display_list *list, float zoom, int rot) const;
  void rastarizeToBitmap(
      fz_context *ctx, fz_pixmap *pix, std::vector<uint8_t> &rgba, sf::Image &image
  ) const;

private:
  using FzDisplayListCache = containers::LRUCache<std::size_t, FzDisplayList>;
  using FzRectCache = containers::LRUCache<FzRectKey, sf::Vector2u, FzRectHash>;

  FzContextPtr drop_ctx_;
  std::mutex drop_mutex_;

  FzDisplayListCache display_list_cache_;
  FzRectCache fz_rect_cache_;
  std::mutex display_list_mutex_;
  std::mutex rect_mutex_;

  int32_t recolor_dark_;
  int32_t recolor_light_;
};

} // namespace pdf
