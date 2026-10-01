#pragma once

#include <SFML/Graphics.hpp>
#include <mupdf/fitz.h>

#include <mutex>

#include "utils/lru_cache.h"

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

class PDFPageDisplayList {
public:
  PDFPageDisplayList() = default;
  PDFPageDisplayList(fz_context *ctx, fz_display_list *display_list);
  ~PDFPageDisplayList();
  PDFPageDisplayList(const PDFPageDisplayList &) = delete;
  PDFPageDisplayList &operator=(const PDFPageDisplayList &) = delete;
  PDFPageDisplayList(PDFPageDisplayList &&other) noexcept;
  PDFPageDisplayList &operator=(PDFPageDisplayList &&other) noexcept;

  fz_display_list *displayList() const;

private:
  void reset();

private:
  fz_context *ctx_ = nullptr;
  fz_display_list *display_list_ = nullptr;
};

class PDFRenderer {
public:
  explicit PDFRenderer();

  void render(
      fz_context *ctx,
      fz_document *doc,
      const PDFRenderKey &key,
      std::vector<uint8_t> &rgba,
      sf::Image &image
  );
  sf::Vector2u getPageSize(const fz_rect &bounds, const PDFRenderKey &key);
  void clearCache();

private:
  fz_display_list *getPageDisplayList(std::size_t page_idx, fz_context *ctx, fz_document *doc);
  fz_display_list *getOrLoadDisplayList(std::size_t page_idx, fz_context *ctx, fz_document *doc);
  fz_pixmap *getPixmapFromDisplayList(fz_context *ctx, fz_display_list *list, float zoom, int rot);
  void
  rastarizeToBitmap(fz_context *ctx, fz_pixmap *pix, std::vector<uint8_t> &rgba, sf::Image &image);

private:
  using PDFPageDisplayListCache = utils::LRUCache<std::size_t, pdf::PDFPageDisplayList>;
  using FzRectCache = utils::LRUCache<FzRectKey, sf::Vector2u, FzRectHash>;

  PDFPageDisplayListCache display_list_cache_;
  FzRectCache fz_rect_cache_;
  std::mutex display_mutex_;
};

} // namespace pdf
