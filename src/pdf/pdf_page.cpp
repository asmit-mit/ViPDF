#include "pdf/pdf_page.h"

namespace pdf {

PDFPage::PDFPage(std::size_t idx, fz_rect bounds) : page_idx_(idx), page_bounds_(bounds) {}

PDFPage::PDFPage(PDFPage &&other) noexcept {
  std::lock_guard lock(other.mutex_);
  page_idx_ = other.page_idx_;
  page_bounds_ = other.page_bounds_;
  stext_ = std::move(other.stext_);
  max_reach_ = other.max_reach_;
  sorted_glyphs_ = std::move(other.sorted_glyphs_);
  selection_quads_ = std::move(other.selection_quads_);
  content_loaded_ = other.content_loaded_;
  other.content_loaded_ = false;
}

PDFPage &PDFPage::operator=(PDFPage &&other) noexcept {
  if (this == &other)
    return *this;

  std::scoped_lock lock(mutex_, other.mutex_);
  page_idx_ = other.page_idx_;
  page_bounds_ = other.page_bounds_;
  stext_ = std::move(other.stext_);
  max_reach_ = other.max_reach_;
  sorted_glyphs_ = std::move(other.sorted_glyphs_);
  selection_quads_ = std::move(other.selection_quads_);
  content_loaded_ = other.content_loaded_;
  other.content_loaded_ = false;
  return *this;
}

fz_rect PDFPage::getPageBounds() const {
  return page_bounds_;
}

void PDFPage::loadContent(FzStextPagePtr stext) {
  if (!stext)
    return;

  {
    std::lock_guard lock(mutex_);
    if (content_loaded_)
      return;
  }

  std::vector<SortedGlyph> glyphs;
  float reach = 0.f;

  for (fz_stext_block *block = stext->first_block; block; block = block->next) {
    if (block->type != FZ_STEXT_BLOCK_TEXT)
      continue;

    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
      const float line_mid_y = (line->bbox.y0 + line->bbox.y1) * 0.5f;

      for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
        const fz_rect r = fz_rect_from_quad(ch->quad);
        if (fz_is_empty_rect(r))
          continue;

        reach = std::max({reach, line_mid_y - r.y0, r.y1 - line_mid_y});
        glyphs.push_back({line_mid_y, r});
      }
    }
  }

  std::stable_sort(glyphs.begin(), glyphs.end(), [](const SortedGlyph &a, const SortedGlyph &b) {
    if (a.line_mid_y != b.line_mid_y)
      return a.line_mid_y < b.line_mid_y;
    return a.rect.x0 < b.rect.x0;
  });

  std::lock_guard lock(mutex_);
  if (content_loaded_)
    return;
  sorted_glyphs_ = std::move(glyphs);
  max_reach_ = reach;
  stext_ = std::move(stext);
  content_loaded_ = true;
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
  return content_loaded_;
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

bool PDFPage::mouseInGlyph(sf::Vector2f pos) const {
  std::lock_guard lock(mutex_);
  if (!content_loaded_ || sorted_glyphs_.empty())
    return false;

  const float lo = pos.y - max_reach_;
  const float hi = pos.y + max_reach_;

  auto it = std::
      lower_bound(sorted_glyphs_.begin(), sorted_glyphs_.end(), lo, [](const SortedGlyph &g, float y) {
        return g.line_mid_y < y;
      });

  for (; it != sorted_glyphs_.end() && it->line_mid_y <= hi; ++it) {
    const fz_rect &r = it->rect;
    if (pos.x >= r.x0 && pos.x <= r.x1 && pos.y >= r.y0 && pos.y <= r.y1)
      return true;
  }

  return false;
}

} // namespace pdf
