#pragma once

#include <clip.h>
#include <cstdint>
#include <iostream>
#include <mupdf/fitz.h>
#include <vector>

namespace utils {

inline bool copyImageToClipboard(fz_context *ctx, fz_image *img) {
  if (!ctx || !img)
    return false;

  fz_pixmap *pix = nullptr;
  fz_pixmap *rgb = nullptr;
  std::vector<uint8_t> rgba;
  int width = 0, height = 0;
  bool ok = false;

  fz_var(pix);
  fz_var(rgb);

  fz_try(ctx) {
    pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);

    const int has_alpha = fz_pixmap_alpha(ctx, pix);
    rgb =
        fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), nullptr, nullptr, fz_default_color_params, has_alpha);

    width = fz_pixmap_width(ctx, rgb);
    height = fz_pixmap_height(ctx, rgb);
    const int stride = fz_pixmap_stride(ctx, rgb);
    const int n = fz_pixmap_components(ctx, rgb);
    const unsigned char *samples = fz_pixmap_samples(ctx, rgb);

    rgba.resize(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y) {
      const unsigned char *src = samples + static_cast<size_t>(y) * stride;
      uint8_t *dst = rgba.data() + static_cast<size_t>(y) * width * 4;
      for (int x = 0; x < width; ++x) {
        dst[4 * x + 0] = src[n * x + 0];
        dst[4 * x + 1] = src[n * x + 1];
        dst[4 * x + 2] = src[n * x + 2];
        dst[4 * x + 3] = (n == 4) ? src[n * x + 3] : 255;
      }
    }
    ok = true;
  }
  fz_always(ctx) {
    fz_drop_pixmap(ctx, rgb);
    fz_drop_pixmap(ctx, pix);
  }
  fz_catch(ctx) {
    std::cerr << "Error copying image: " << fz_caught_message(ctx) << "\n";
  }

  if (!ok || width <= 0 || height <= 0)
    return false;

  clip::image_spec spec;
  spec.width = width;
  spec.height = height;
  spec.bits_per_pixel = 32;
  spec.bytes_per_row = width * 4;
  spec.red_mask = 0x000000ff;
  spec.red_shift = 0;
  spec.green_mask = 0x0000ff00;
  spec.green_shift = 8;
  spec.blue_mask = 0x00ff0000;
  spec.blue_shift = 16;
  spec.alpha_mask = 0xff000000;
  spec.alpha_shift = 24;

  clip::image clip_img(rgba.data(), spec);
  return clip::set_image(clip_img);
}

} // namespace utils
