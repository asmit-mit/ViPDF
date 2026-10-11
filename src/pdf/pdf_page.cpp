#include "pdf/pdf_page.h"
#include "utils/settings.h"
#include "utils/utils.h"

#include <SFML/Graphics/VertexArray.hpp>
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

PDFPage::PDFPage(std::size_t idx, fz_rect bounds) : page_idx_(idx), page_bounds_(bounds) {}

PDFPage::PDFPage(PDFPage &&other) noexcept
    : page_idx_(other.page_idx_), page_bounds_(other.page_bounds_), text_(std::move(other.text_)),
      text_normalized_(std::move(other.text_normalized_)), glyphs_(std::move(other.glyphs_)),
      content_loaded_(other.content_loaded_) {}

PDFPage &PDFPage::operator=(PDFPage &&other) noexcept {
  if (this == &other)
    return *this;
  std::scoped_lock lock(mutex_, other.mutex_);
  page_idx_ = other.page_idx_;
  page_bounds_ = other.page_bounds_;
  text_ = std::move(other.text_);
  text_normalized_ = std::move(other.text_normalized_);
  glyphs_ = std::move(other.glyphs_);
  content_loaded_ = other.content_loaded_;
  return *this;
}

fz_rect PDFPage::getPageBounds() const {
  return page_bounds_;
}

void PDFPage::loadContent(fz_stext_page *page_text) {
  if (!page_text)
    return;

  std::lock_guard lock(mutex_);
  if (content_loaded_)
    return;

  appendSTextPage(text_, text_normalized_, glyphs_, page_text);
  content_loaded_ = true;
}

bool PDFPage::isContentLoaded() const {
  std::lock_guard lock(mutex_);
  return content_loaded_;
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

  if (!content_loaded_ || normalized_pattern.size() > text_normalized_.size())
    return results;

  const std::boyer_moore_horspool_searcher
      searcher(normalized_pattern.begin(), normalized_pattern.end());

  std::vector<std::vector<fz_rect>> matches;

  auto it = text_normalized_.begin();
  while (true) {
    it = std::search(it, text_normalized_.end(), searcher);
    if (it == text_normalized_.end())
      break;

    const std::size_t pos = static_cast<std::size_t>(it - text_normalized_.begin());
    const std::size_t end = pos + normalized_pattern.size();

    std::vector<fz_rect> segments;
    fz_rect curr = fz_empty_rect;
    for (std::size_t i = pos; i < end; ++i) {
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

sf::VertexArray PDFPage::getSelectionBoxes(
    PDFSelectionDirection direction,
    std::optional<sf::Vector2f> mouse_pos1,
    std::optional<sf::Vector2f> mouse_pos2
) const {
  sf::VertexArray boxes{sf::PrimitiveType::Triangles};

  std::lock_guard lock(mutex_);
  if (!content_loaded_ || glyphs_.empty())
    return boxes;

  const sf::Color color = utils::hexToRGBA(settings::selection_box_color_);

  auto isBefore = [](const fz_rect &r, sf::Vector2f p) {
    if (r.y1 < p.y)
      return true;
    if (r.y0 > p.y)
      return false;
    return (r.x0 + r.x1) * 0.5f < p.x;
  };

  auto addRect = [&](const fz_rect &r) {
    const sf::Vector2f tl{r.x0, r.y0}, tr{r.x1, r.y0};
    const sf::Vector2f br{r.x1, r.y1}, bl{r.x0, r.y1};
    boxes.append({tl, color});
    boxes.append({tr, color});
    boxes.append({br, color});
    boxes.append({tl, color});
    boxes.append({br, color});
    boxes.append({bl, color});
  };

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

  for (const fz_rect &r : glyphs_) {
    if (fz_is_empty_rect(r))
      continue;

    switch (direction) {
    case PDFSelectionDirection::ALL:
      addRect(r);
      break;
    case PDFSelectionDirection::UP:
      if (isBefore(r, p1))
        addRect(r);
      break;
    case PDFSelectionDirection::DOWN:
      if (!isBefore(r, p1))
        addRect(r);
      break;
    case PDFSelectionDirection::INSIDE:
      if (!isBefore(r, p1) && isBefore(r, p2))
        addRect(r);
      break;
    }
  }

  return boxes;
}

} // namespace pdf
