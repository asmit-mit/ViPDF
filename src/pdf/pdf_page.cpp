#include "pdf/pdf_page.h"
#include "utils/settings.h"
#include "utils/utils.h"

#include <SFML/Graphics/VertexArray.hpp>
#include <algorithm>
#include <cmath>
#include <mupdf/fitz/geometry.h>

namespace pdf {

using std::stable_sort;

char32_t PDFPage::normalizeChar(char32_t cp) const {
  if (cp >= U'A' && cp <= U'Z')
    return cp + (U'a' - U'A');
  if (cp == U'\n' || cp == U'\r' || cp == U'\t' || cp == 0x00A0)
    return U' ';
  return cp;
}

bool PDFPage::endsWithSpace(const std::u32string &text) {
  return !text.empty() && normalizeChar(text.back()) == U' ';
}

void PDFPage::pushBreak(std::u32string &text, std::vector<fz_rect> &rects) {
  if (text.empty())
    return;
  if (endsWithSpace(text)) {
    rects.back() = fz_empty_rect;
    return;
  }
  text.push_back(U'\n');
  rects.push_back(fz_empty_rect);
}

void PDFPage::appendSTextPage(
    std::u32string &text, std::vector<fz_rect> &rects, fz_stext_page *page_text
) {
  for (fz_stext_block *block = page_text->first_block; block; block = block->next) {
    if (block->type != FZ_STEXT_BLOCK_TEXT)
      continue;

    for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
      for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
        const auto cp = static_cast<char32_t>(ch->c);

        if (normalizeChar(cp) == U' ' && (text.empty() || endsWithSpace(text)))
          continue;

        text.push_back(cp);
        rects.push_back(fz_rect_from_quad(ch->quad));
      }
      pushBreak(text, rects);
    }
    pushBreak(text, rects);
  }

  text.shrink_to_fit();
  rects.shrink_to_fit();
}

PDFPage::PDFPage(std::size_t idx, fz_rect bounds) : page_idx_(idx), page_bounds_(bounds) {}

PDFPage::PDFPage(PDFPage &&other) noexcept
    : page_idx_(other.page_idx_), page_bounds_(other.page_bounds_), text_(std::move(other.text_)),
      glyphs_(std::move(other.glyphs_)), selection_quads_(std::move(other.selection_quads_)),
      content_loaded_(other.content_loaded_) {}

PDFPage &PDFPage::operator=(PDFPage &&other) noexcept {
  if (this == &other)
    return *this;
  std::scoped_lock lock(mutex_, other.mutex_);
  page_idx_ = other.page_idx_;
  page_bounds_ = other.page_bounds_;
  text_ = std::move(other.text_);
  glyphs_ = std::move(other.glyphs_);
  selection_quads_ = std::move(other.selection_quads_);
  content_loaded_ = other.content_loaded_;
  return *this;
}

fz_rect PDFPage::getPageBounds() const {
  return page_bounds_;
}

void PDFPage::loadContent(fz_stext_page *page_text) {
  if (!page_text)
    return;

  {
    std::lock_guard lock(mutex_);
    if (content_loaded_)
      return;
  }

  std::u32string text;
  std::vector<fz_rect> glyphs;
  appendSTextPage(text, glyphs, page_text);

  std::lock_guard lock(mutex_);
  if (content_loaded_)
    return;
  text_ = std::move(text);
  glyphs_ = std::move(glyphs);
  content_loaded_ = true;
}

bool PDFPage::isContentLoaded() const {
  std::lock_guard lock(mutex_);
  return content_loaded_;
}

std::u32string PDFPage::normalizePattern(const std::u32string &pattern) const {
  std::u32string pat;
  pat.reserve(pattern.size());
  for (char32_t cp : pattern) {
    const char32_t n = normalizeChar(cp);
    if (n == U' ' && (pat.empty() || pat.back() == U' '))
      continue;
    pat.push_back(n);
  }
  while (!pat.empty() && pat.back() == U' ')
    pat.pop_back();
  return pat;
}

std::vector<fz_rect> PDFPage::collectSegments(std::size_t pos, std::size_t len) const {
  std::vector<fz_rect> segments;
  fz_rect curr = fz_empty_rect;
  for (std::size_t i = pos; i < pos + len; ++i) {
    if (fz_is_empty_rect(glyphs_[i])) {
      if (!fz_is_empty_rect(curr))
        segments.push_back(curr);
      curr = fz_empty_rect;
      continue;
    }
    curr = fz_union_rect(curr, glyphs_[i]);
  }
  if (!fz_is_empty_rect(curr))
    segments.push_back(curr);
  return segments;
}

std::vector<std::size_t> PDFPage::buildFailure(const std::u32string &pat) const {
  std::vector<std::size_t> fail(pat.size(), 0);
  for (std::size_t i = 1, k = 0; i < pat.size(); ++i) {
    while (k > 0 && pat[i] != pat[k])
      k = fail[k - 1];
    if (pat[i] == pat[k])
      ++k;
    fail[i] = k;
  }
  return fail;
}

std::vector<std::vector<fz_rect>> PDFPage::searchText(const std::u32string &pattern) const {
  const std::u32string pat = normalizePattern(pattern);
  if (pat.empty())
    return {};

  std::lock_guard lock(mutex_);

  if (!content_loaded_ || pat.size() > text_.size())
    return {};

  std::vector<std::vector<fz_rect>> matches;

  const std::size_t m = pat.size();
  const std::vector<std::size_t> fail = buildFailure(pat);
  std::size_t k = 0;

  for (std::size_t i = 0; i < text_.size(); ++i) {
    const char32_t c = normalizeChar(text_[i]);
    while (k > 0 && c != pat[k])
      k = fail[k - 1];
    if (c == pat[k])
      ++k;

    if (k == m) {
      std::vector<fz_rect> segments = collectSegments(i + 1 - m, m);
      if (!segments.empty())
        matches.push_back(std::move(segments));
      k = 0;
    }
  }

  stable_sort(matches.begin(), matches.end(), [](const std::vector<fz_rect> &a, const std::vector<fz_rect> &b) {
    const long ra = static_cast<long>(std::lround((a.front().y0 + a.front().y1) * 0.5f / 4.0f));
    const long rb = static_cast<long>(std::lround((b.front().y0 + b.front().y1) * 0.5f / 4.0f));
    if (ra != rb)
      return ra < rb;
    return a.front().x0 < b.front().x0;
  });

  return matches;
}

bool PDFPage::isBefore(const fz_rect &r, sf::Vector2f p) const {
  if (r.y1 < p.y)
    return true;
  if (r.y0 > p.y)
    return false;
  return (r.x0 + r.x1) * 0.5f < p.x;
}

bool PDFPage::isSelected(
    PDFSelectionDirection direction, const fz_rect &r, sf::Vector2f p1, sf::Vector2f p2
) const {
  switch (direction) {
  case PDFSelectionDirection::ALL:
    return true;
  case PDFSelectionDirection::UP:
    return isBefore(r, p1);
  case PDFSelectionDirection::DOWN:
    return !isBefore(r, p1);
  case PDFSelectionDirection::INSIDE:
    return !isBefore(r, p1) && isBefore(r, p2);
  }
  return false;
}

void PDFPage::flushRun(
    sf::VertexArray &boxes, const fz_rect &run, bool &have, sf::Color color
) const {
  if (have) {
    selection_quads_.push_back(fz_quad_from_rect(run));
    const sf::Vector2f tl{run.x0, run.y0}, tr{run.x1, run.y0};
    const sf::Vector2f br{run.x1, run.y1}, bl{run.x0, run.y1};
    boxes.append({tl, color});
    boxes.append({tr, color});
    boxes.append({br, color});
    boxes.append({tl, color});
    boxes.append({br, color});
    boxes.append({bl, color});
  }
  have = false;
}

sf::VertexArray PDFPage::getSelectionBoxes(
    PDFSelectionDirection direction,
    std::optional<sf::Vector2f> mouse_pos1,
    std::optional<sf::Vector2f> mouse_pos2
) const {
  sf::VertexArray boxes{sf::PrimitiveType::Triangles};

  std::lock_guard lock(mutex_);

  selection_quads_.clear();
  if (!content_loaded_ || glyphs_.empty())
    return boxes;

  const sf::Color color = utils::hexToRGBA(settings::selection_box_color_);

  sf::Vector2f p1{}, p2{};
  if (direction != PDFSelectionDirection::ALL) {
    if (!mouse_pos1)
      return boxes;
    p1 = *mouse_pos1;
  }
  if (direction == PDFSelectionDirection::INSIDE) {
    if (!mouse_pos2)
      return boxes;
    p2 = *mouse_pos2;

    const bool p2_earlier = p2.y < p1.y || (p2.y == p1.y && p2.x < p1.x);
    if (p2_earlier)
      std::swap(p1, p2);
  }

  fz_rect run{};
  bool have = false;

  for (const fz_rect &rect : glyphs_) {
    if (fz_is_empty_rect(rect) || !isSelected(direction, rect, p1, p2)) {
      flushRun(boxes, run, have, color);
      continue;
    }

    const float mid = (rect.y0 + rect.y1) * 0.5f;
    bool continue_run = mid > run.y0 && mid < run.y1 && rect.x0 >= run.x0;

    if (have && continue_run) {
      run = fz_union_rect(run, rect);
    } else {
      flushRun(boxes, run, have, color);
      run = rect;
      have = true;
    }
  }
  flushRun(boxes, run, have, color);

  return boxes;
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
  if (!content_loaded_ || glyphs_.empty())
    return false;

  for (auto &rect : glyphs_) {
    if (pos.x >= rect.x0 && pos.y >= rect.y0 && pos.x <= rect.x1 && pos.y <= rect.y1)
      return true;
  }

  return false;
}

} // namespace pdf
