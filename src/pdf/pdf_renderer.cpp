#include <stdexcept>

#include "pdf/pdf_renderer.h"
#include "utils/utils.h"

namespace pdf {

PDFRenderer::PDFRenderer(FzContextPtr ctx, int32_t recolor_dark, int32_t recolor_light)
    : drop_ctx_(std::move(ctx)), display_list_cache_(utils::DISPLAY_LIST_CACHE_SIZE),
      fz_rect_cache_(utils::DISPLAY_LIST_CACHE_SIZE), recolor_dark_(recolor_dark),
      recolor_light_(recolor_light) {}

void PDFRenderer::render(
    fz_context *ctx,
    fz_document *doc,
    const PDFRenderKey &key,
    std::vector<uint8_t> &rgba,
    sf::Image &image
) {
  FzDisplayList list = getOrLoadDisplayList(key.page_idx, ctx, doc);
  FzPixmap pix =
      FzPixmap(getPixmapFromDisplayList(ctx, list.get(), key.zoom, key.rotate), FzPixmapDeleter{ctx});

  if (key.recolor) {
    fz_try(ctx) fz_tint_pixmap(ctx, pix.get(), recolor_dark_, recolor_light_);
    fz_catch(ctx) throw std::runtime_error("Failed to recolor page");
  }
  rastarizeToBitmap(ctx, pix.get(), rgba, image);
}

sf::Vector2u PDFRenderer::getPageSize(const fz_rect &bounds, const PDFRenderKey &key) {
  FzRectKey fz_rect_key{bounds, key};
  {
    std::lock_guard lock(rect_mutex_);
    if (auto *cached = fz_rect_cache_.get(fz_rect_key))
      return *cached;
  }

  fz_matrix ctm = fz_scale(key.zoom, key.zoom);
  ctm = fz_pre_rotate(ctm, key.rotate * 90.f);

  fz_rect transformed = fz_transform_rect(bounds, ctm);
  fz_irect bbox = fz_round_rect(transformed);

  sf::Vector2u
      size{static_cast<unsigned>(bbox.x1 - bbox.x0), static_cast<unsigned>(bbox.y1 - bbox.y0)};

  std::lock_guard lock(rect_mutex_);
  fz_rect_cache_.put(fz_rect_key, size);
  return size;
}

void PDFRenderer::invalidatePage(std::size_t page_idx) {
  std::lock_guard lock(display_list_mutex_);
  display_list_cache_.erase(page_idx);
}

void PDFRenderer::clearCache() {
  std::lock_guard lock(display_list_mutex_);
  display_list_cache_.clear();
}

FzDisplayList
PDFRenderer::getOrLoadDisplayList(std::size_t page_idx, fz_context *ctx, fz_document *doc) {
  std::lock_guard lock(display_list_mutex_);

  if (auto *cached = display_list_cache_.get(page_idx))
    return *cached;

  FzDisplayList
      list(getPageDisplayList(page_idx, ctx, doc), FzDisplayListDeleter{drop_ctx_.get(), &drop_mutex_});
  display_list_cache_.put(page_idx, list);
  return list;
}

fz_display_list *
PDFRenderer::getPageDisplayList(std::size_t page_idx, fz_context *ctx, fz_document *doc) const {
  fz_page *page = nullptr;
  fz_display_list *list = nullptr;
  fz_device *dev = nullptr;
  fz_var(page);
  fz_var(list);
  fz_var(dev);

  fz_try(ctx) {
    page = fz_load_page(ctx, doc, static_cast<int>(page_idx));
    list = fz_new_display_list(ctx, fz_bound_page(ctx, page));
    dev = fz_new_list_device(ctx, list);
    fz_run_page(ctx, page, dev, fz_identity, nullptr);
    fz_close_device(ctx, dev);
  }
  fz_always(ctx) {
    if (dev)
      fz_drop_device(ctx, dev);
    if (page)
      fz_drop_page(ctx, page);
  }
  fz_catch(ctx) {
    if (list)
      fz_drop_display_list(ctx, list);
    throw std::runtime_error("Failed to build display list");
  }

  return list;
}

fz_pixmap *PDFRenderer::getPixmapFromDisplayList(
    fz_context *ctx, fz_display_list *list, float zoom, int rot
) const {
  fz_matrix ctm = fz_scale(zoom, zoom);
  ctm = fz_pre_rotate(ctm, rot * 90.f);

  fz_pixmap *pix = nullptr;
  fz_device *dev = nullptr;
  fz_var(pix);
  fz_var(dev);

  fz_rect bounds = fz_bound_display_list(ctx, list);
  fz_rect transformed = fz_transform_rect(bounds, ctm);
  fz_irect bbox = fz_round_rect(transformed);

  fz_try(ctx) {
    pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 0);
    fz_clear_pixmap_with_value(ctx, pix, 0xFF);

    dev = fz_new_draw_device(ctx, fz_identity, pix);
    fz_run_display_list(ctx, list, dev, ctm, transformed, nullptr);
    fz_close_device(ctx, dev);
  }
  fz_always(ctx) {
    if (dev)
      fz_drop_device(ctx, dev);
  }
  fz_catch(ctx) {
    if (pix)
      fz_drop_pixmap(ctx, pix);
    throw std::runtime_error("Failed to render page");
  }

  return pix;
}

void PDFRenderer::rastarizeToBitmap(
    fz_context *ctx, fz_pixmap *pix, std::vector<uint8_t> &rgba, sf::Image &image
) const {
  const int width = fz_pixmap_width(ctx, pix);
  const int height = fz_pixmap_height(ctx, pix);
  const int stride = fz_pixmap_stride(ctx, pix);
  const unsigned char *samples = fz_pixmap_samples(ctx, pix);

  rgba.resize(width * height * 4);
  for (int y = 0; y < height; ++y) {
    const unsigned char *src = samples + y * stride;
    for (int x = 0; x < width; ++x) {
      rgba[4 * (y * width + x) + 0] = src[3 * x + 0];
      rgba[4 * (y * width + x) + 1] = src[3 * x + 1];
      rgba[4 * (y * width + x) + 2] = src[3 * x + 2];
      rgba[4 * (y * width + x) + 3] = 255;
    }
  }

  image = sf::Image({static_cast<unsigned>(width), static_cast<unsigned>(height)}, rgba.data());
}

} // namespace pdf
