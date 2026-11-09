#include "pdf/pdf_page.h"

namespace pdf {

PDFPage::PDFPage(std::size_t idx, fz_rect bounds) : page_idx_(idx), page_bounds_(bounds) {}

PDFPage::PDFPage(PDFPage &&other) noexcept {
  std::lock_guard lock(other.mutex_);
  page_idx_ = other.page_idx_;
  page_bounds_ = other.page_bounds_;
  stext_ = std::move(other.stext_);
  max_reach_ = other.max_reach_;
  text_rects_ = std::move(other.text_rects_);
  link_rects_ = std::move(other.link_rects_);
  img_rects_ = std::move(other.img_rects_);
  selection_quads_ = std::move(other.selection_quads_);
  text_content_loaded_ = other.text_content_loaded_;
  other.text_content_loaded_ = false;
}

PDFPage &PDFPage::operator=(PDFPage &&other) noexcept {
  if (this == &other)
    return *this;

  std::scoped_lock lock(mutex_, other.mutex_);
  page_idx_ = other.page_idx_;
  page_bounds_ = other.page_bounds_;
  stext_ = std::move(other.stext_);
  max_reach_ = other.max_reach_;
  text_rects_ = std::move(other.text_rects_);
  link_rects_ = std::move(other.link_rects_);
  img_rects_ = std::move(other.img_rects_);
  selection_quads_ = std::move(other.selection_quads_);
  text_content_loaded_ = other.text_content_loaded_;
  other.text_content_loaded_ = false;
  return *this;
}

fz_rect PDFPage::getPageBounds() const {
  return page_bounds_;
}

void PDFPage::loadContent(fz_context *ctx, fz_document *doc, FzStextPagePtr stext, fz_link *links) {
  {
    std::lock_guard lock(mutex_);
    if (text_content_loaded_)
      return;
  }

  std::vector<TextRect> text_rects;
  std::vector<ImageRect> img_rects;
  float reach = 0.f;

  for (fz_stext_block *block = stext->first_block; block; block = block->next) {
    if (block->type == FZ_STEXT_BLOCK_TEXT) {
      for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
        const float line_mid_y = (line->bbox.y0 + line->bbox.y1) * 0.5f;

        for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
          const fz_rect r = fz_rect_from_quad(ch->quad);
          if (fz_is_empty_rect(r))
            continue;

          reach = std::max({reach, line_mid_y - r.y0, r.y1 - line_mid_y});
          text_rects.push_back({r, line_mid_y});
        }
      }
    } else if (block->type == FZ_STEXT_BLOCK_IMAGE) {
      img_rects.push_back({block->bbox, block->u.i.image});
    }
  }

  std::stable_sort(text_rects.begin(), text_rects.end(), [](const TextRect &a, const TextRect &b) {
    if (a.line_mid_y != b.line_mid_y)
      return a.line_mid_y < b.line_mid_y;
    return a.rect.x0 < b.rect.x0;
  });

  std::vector<LinkRect> link_rects;

  for (fz_link *l = links; l; l = l->next) {
    if (!l->uri || fz_is_empty_rect(l->rect))
      continue;

    PDFLinkInfo info;
    if (fz_is_external_link(ctx, l->uri)) {
      info.type = PDFLinkType::EXTERNAL;
      info.url = l->uri;
    } else {
      info.type = PDFLinkType::INTERNAL;
      resolveLinkPage(ctx, doc, l->uri, info);
      if (info.target_page < 0)
        continue;
    }

    link_rects.push_back({l->rect, std::move(info)});
  }

  text_rects.shrink_to_fit();
  img_rects.shrink_to_fit();
  link_rects.shrink_to_fit();

  std::lock_guard lock(mutex_);
  if (text_content_loaded_)
    return;
  text_rects_ = std::move(text_rects);
  img_rects_ = std::move(img_rects);
  link_rects_ = std::move(link_rects);
  max_reach_ = reach;
  stext_ = std::move(stext);
  text_content_loaded_ = true;
}

fz_stext_page *PDFPage::stext() const {
  std::lock_guard lock(mutex_);
  return stext_.get();
}

void PDFPage::resetStext() {
  std::lock_guard lock(mutex_);
  stext_.reset();
}

std::vector<fz_quad> PDFPage::searchText(fz_context *ctx, const std::string &needle) const {
  std::lock_guard lock(mutex_);
  if (!stext_ || needle.empty())
    return {};

  std::vector<fz_quad> hits(512);
  int n = 0;
  fz_var(n);

  fz_try(ctx) n =
      fz_search_stext_page(ctx, stext_.get(), needle.c_str(), nullptr, hits.data(), static_cast<int>(hits.size()));
  fz_catch(ctx) return {};

  hits.resize(static_cast<std::size_t>(n));
  return hits;
}

const std::vector<fz_quad> &PDFPage::selectPage(fz_context *ctx, fz_point a, fz_point b) const {
  std::lock_guard lock(mutex_);
  if (!stext_)
    return selection_quads_;

  selection_quads_.resize(1024);
  int n = 0;
  fz_var(n);

  fz_try(ctx) n = fz_highlight_selection(
      ctx, stext_.get(), a, b, selection_quads_.data(), static_cast<int>(selection_quads_.size())
  );
  fz_catch(ctx) return selection_quads_;

  selection_quads_.resize(static_cast<std::size_t>(n));
  return selection_quads_;
}

std::string PDFPage::copySelection(fz_context *ctx, fz_point a, fz_point b) const {
  std::lock_guard lock(mutex_);
  if (!stext_)
    return {};

  char *raw = nullptr;
  fz_var(raw);
  fz_try(ctx) raw = fz_copy_selection(ctx, stext_.get(), a, b, 0);
  fz_catch(ctx) return {};

  if (!raw)
    return {};

  std::string text(raw);
  fz_free(ctx, raw);

  while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
    text.pop_back();
  return text;
}

bool PDFPage::isContentLoaded() const {
  std::lock_guard lock(mutex_);
  return text_content_loaded_;
}

bool PDFPage::hasSelections() const {
  std::lock_guard lock(mutex_);
  return !selection_quads_.empty();
}

void PDFPage::clearSelections() {
  std::lock_guard lock(mutex_);
  selection_quads_.clear();
}

const std::vector<fz_quad> &PDFPage::getSelectionFzQuads() const {
  return selection_quads_;
}

bool PDFPage::mouseInText(sf::Vector2f pos) const {
  std::lock_guard lock(mutex_);
  if (!text_content_loaded_ || text_rects_.empty())
    return false;

  const float lo = pos.y - max_reach_;
  const float hi = pos.y + max_reach_;

  auto it =
      std::lower_bound(text_rects_.begin(), text_rects_.end(), lo, [](const TextRect &g, float y) {
        return g.line_mid_y < y;
      });

  fz_point p = fz_make_point(pos.x, pos.y);
  for (; it != text_rects_.end() && it->line_mid_y <= hi; ++it) {
    const fz_rect &r = it->rect;
    if (fz_is_point_inside_rect(p, r))
      return true;
  }

  return false;
}

std::optional<PDFLinkInfo> PDFPage::mouseInLink(sf::Vector2f pos) const {
  std::lock_guard lock(mutex_);
  fz_point p = fz_make_point(pos.x, pos.y);
  for (const LinkRect &l : link_rects_) {
    if (fz_is_point_inside_rect(p, l.rect))
      return l.info;
  }
  return std::nullopt;
}

fz_image *PDFPage::mouseInImage(sf::Vector2f pos) const {
  std::lock_guard lock(mutex_);
  fz_point p = fz_make_point(pos.x, pos.y);
  for (const ImageRect &img : img_rects_) {
    if (fz_is_point_inside_rect(p, img.rect))
      return img.img;
  }
  return nullptr;
}

void PDFPage::resolveLinkPage(
    fz_context *ctx, fz_document *doc, const char *uri, PDFLinkInfo &info
) {
  int page = -1;
  fz_var(page);

  fz_try(ctx) {
    fz_location loc = fz_resolve_link(ctx, doc, uri, &info.x, &info.y);
    info.target_page = fz_page_number_from_location(ctx, doc, loc);
  }
  fz_catch(ctx) {
    info.target_page = -1;
  }
}

} // namespace pdf
