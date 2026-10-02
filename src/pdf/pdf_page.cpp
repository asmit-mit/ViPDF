#include "pdf/pdf_page.h"

#include <algorithm>
#include <functional>

namespace pdf {

static char32_t normalizeChar(char32_t cp) {
  if (cp >= U'A' && cp <= U'Z')
    return cp + (U'a' - U'A');
  if (cp == U'\n' || cp == U'\r' || cp == U'\t' || cp == 0x00A0)
    return U' ';
  return cp;
}

static void appendSTextPage(
    std::u32string &text,
    std::u32string &text_normalized,
    std::vector<fz_rect> &rects,
    fz_stext_page *page_text
) {
  auto pushBreak = [&]() {
    if (text_normalized.empty())
      return;
    if (text_normalized.back() == U' ') {
      rects.back() = fz_empty_rect;
      return;
    }
    text.push_back(U'\n');
    text_normalized.push_back(U' ');
    rects.push_back(fz_empty_rect);
  };

  for (fz_stext_block *block = page_text->first_block; block; block = block->next) {
    if (block->type != FZ_STEXT_BLOCK_TEXT)
      continue;

    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
      for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
        const auto cp = static_cast<char32_t>(ch->c);
        const char32_t n = normalizeChar(cp);

        if (n == U' ' && (text_normalized.empty() || text_normalized.back() == U' '))
          continue;

        text.push_back(cp);
        text_normalized.push_back(n);
        rects.push_back(fz_rect_from_quad(ch->quad));
      }
      pushBreak();
    }
    pushBreak();
  }
}

PDFPage::PDFPage(std::size_t idx, fz_rect bounds) : page_idx(idx), page_bounds(bounds) {}

PDFPage::PDFPage(PDFPage &&other) noexcept
    : page_idx(other.page_idx), page_bounds(other.page_bounds), text(std::move(other.text)),
      text_normalized(std::move(other.text_normalized)), glyphs(std::move(other.glyphs)),
      content_loaded(other.content_loaded) {}

PDFPage &PDFPage::operator=(PDFPage &&other) noexcept {
  if (this == &other)
    return *this;
  std::scoped_lock lock(mutex_, other.mutex_);
  page_idx = other.page_idx;
  page_bounds = other.page_bounds;
  text = std::move(other.text);
  text_normalized = std::move(other.text_normalized);
  glyphs = std::move(other.glyphs);
  content_loaded = other.content_loaded;
  return *this;
}

void PDFPage::loadContent(fz_stext_page *page_text) {
  if (!page_text)
    return;

  std::lock_guard lock(mutex_);
  if (content_loaded)
    return;

  appendSTextPage(text, text_normalized, glyphs, page_text);
  content_loaded = true;
}

bool PDFPage::isContentLoaded() const {
  std::lock_guard lock(mutex_);
  return content_loaded;
}

std::vector<fz_rect> PDFPage::searchText(const std::u32string &pattern) const {
  std::vector<fz_rect> results;

  std::u32string normalized_pattern;
  normalized_pattern.reserve(pattern.size());
  for (char32_t cp : pattern) {
    const char32_t n = normalizeChar(cp);
    if (n == U' ' && (normalized_pattern.empty() || normalized_pattern.back() == U' '))
      continue;
    normalized_pattern.push_back(n);
  }

  while (!normalized_pattern.empty() && normalized_pattern.back() == U' ')
    normalized_pattern.pop_back();

  if (normalized_pattern.empty())
    return results;

  std::lock_guard lock(mutex_);

  if (!content_loaded || normalized_pattern.size() > text_normalized.size())
    return results;

  const std::boyer_moore_horspool_searcher
      searcher(normalized_pattern.begin(), normalized_pattern.end());

  std::vector<std::vector<fz_rect>> matches;

  auto it = text_normalized.begin();
  while (true) {
    it = std::search(it, text_normalized.end(), searcher);
    if (it == text_normalized.end())
      break;

    const std::size_t pos = static_cast<std::size_t>(it - text_normalized.begin());
    const std::size_t end = pos + normalized_pattern.size();

    std::vector<fz_rect> segments;
    fz_rect curr = fz_empty_rect;
    for (std::size_t i = pos; i < end; ++i) {
      if (fz_is_empty_rect(glyphs[i])) {
        if (!fz_is_empty_rect(curr))
          segments.push_back(curr);
        curr = fz_empty_rect;
        continue;
      }
      curr = fz_union_rect(curr, glyphs[i]);
    }
    if (!fz_is_empty_rect(curr))
      segments.push_back(curr);

    if (!segments.empty())
      matches.push_back(std::move(segments));

    it += normalized_pattern.size();
  }

  auto rowOf = [](const fz_rect &r) {
    return static_cast<long>(std::lround((r.y0 + r.y1) * 0.5f / 4.0f));
  };

  std::stable_sort(matches.begin(), matches.end(), [&](const auto &a, const auto &b) {
    const long ra = rowOf(a.front());
    const long rb = rowOf(b.front());
    if (ra != rb)
      return ra < rb;
    return a.front().x0 < b.front().x0;
  });

  for (auto &m : matches)
    results.insert(results.end(), m.begin(), m.end());

  return results;
}

} // namespace pdf
