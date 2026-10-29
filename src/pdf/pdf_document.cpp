#include <stdexcept>

#include "pdf/pdf_document.h"
#include "pdf/pdf_page.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace pdf {

PDFDocument::PDFDocument() {
  locks_.user = this;
  locks_.lock = &PDFDocument::lockMutex;
  locks_.unlock = &PDFDocument::unlockMutex;

  is_edited_ = false;

  ctx_ = fz_new_context(nullptr, &locks_, FZ_STORE_DEFAULT);
  if (!ctx_)
    throw std::runtime_error("Failed to create MuPDF context");

  fz_try(ctx_) fz_register_document_handlers(ctx_);
  fz_catch(ctx_) {
    fz_drop_context(ctx_);
    ctx_ = nullptr;
    throw std::runtime_error("Failed to register document handlers");
  }

  doc_ = nullptr;
}

void PDFDocument::openDocument(const std::string &filepath) {
  closeDocument();

  fz_try(ctx_) doc_ = fz_open_document(ctx_, filepath.c_str());
  fz_catch(ctx_) throw std::runtime_error("Failed to open document: " + filepath);

  int page_count;
  fz_try(ctx_) page_count = fz_count_pages(ctx_, doc_);
  fz_catch(ctx_) throw std::runtime_error("Failed to count number of pages");

  int max_width = 0;
  page_with_max_width_ = 0;

  pages_.reserve(page_count);
  for (int i = 0; i < page_count; ++i) {
    fz_page *page = nullptr;

    fz_try(ctx_) {
      page = fz_load_page(ctx_, doc_, i);

      fz_rect bounds = fz_bound_page(ctx_, page);

      int width = static_cast<int>(std::abs(bounds.x1 - bounds.x0));

      if (width > max_width) {
        max_width = width;
        page_with_max_width_ = i;
      }

      pages_.emplace_back(i, bounds);
    }
    fz_always(ctx_) fz_drop_page(ctx_, page);
    fz_catch(ctx_) throw std::runtime_error("Failed to inspect PDF page");
  }

  filepath_ = filepath;
  is_edited_ = false;
  loadOutline();
}

void PDFDocument::closeDocument() {
  if (doc_) {
    fz_drop_document(ctx_, doc_);
    doc_ = nullptr;
  }

  pages_.clear();
}

void PDFDocument::reloadDocument() {
  if (!doc_ || filepath_.empty())
    return;
  openDocument(filepath_);
}

std::size_t PDFDocument::pageWithMaxWidth() const {
  if (!doc_)
    return 0;
  return page_with_max_width_;
}

std::string &PDFDocument::getFilePath() {
  return filepath_;
}

PDFPage &PDFDocument::getPage(std::size_t page_idx) {
  return pages_[page_idx];
}

std::vector<fz_rect> PDFDocument::searchPage(std::size_t page_idx, const std::string &needle) {
  std::vector<fz_rect> rects;
  if (!doc_ || page_idx >= pages_.size() || needle.empty())
    return rects;

  PDFPage &page = pages_[page_idx];
  if (!page.isContentLoaded())
    loadPageContent(page_idx);

  const std::vector<fz_quad> quads = page.searchText(ctx_, needle);
  rects.reserve(quads.size());
  for (const fz_quad &q : quads)
    rects.push_back(fz_rect_from_quad(q));

  return rects;
}

sf::VertexArray PDFDocument::getSelectionBoxesFromPage(
    std::size_t page_idx,
    PDFSelectionDirection direction,
    const std::optional<sf::Vector2f> &mouse_pos1,
    const std::optional<sf::Vector2f> &mouse_pos2
) {
  sf::VertexArray boxes{sf::PrimitiveType::Triangles};
  if (!doc_ || page_idx >= pages_.size())
    return boxes;

  PDFPage &page = pages_[page_idx];
  if (!page.isContentLoaded())
    loadPageContent(page_idx);

  const auto pts = selectionPoints(page_idx, direction, mouse_pos1, mouse_pos2);
  if (!pts)
    return boxes;

  const std::vector<fz_quad> quads = page.selectPage(ctx_, pts->first, pts->second);
  const sf::Color color = utils::hexToRGBA(settings::selection_box_color_);

  for (const fz_quad &q : quads) {
    const sf::Vector2f ul{q.ul.x, q.ul.y};
    const sf::Vector2f ur{q.ur.x, q.ur.y};
    const sf::Vector2f lr{q.lr.x, q.lr.y};
    const sf::Vector2f ll{q.ll.x, q.ll.y};

    boxes.append({ul, color});
    boxes.append({ur, color});
    boxes.append({lr, color});

    boxes.append({ul, color});
    boxes.append({lr, color});
    boxes.append({ll, color});
  }

  return boxes;
}

std::string PDFDocument::copySelectionFromPage(
    std::size_t page_idx,
    PDFSelectionDirection direction,
    const std::optional<sf::Vector2f> &mouse_pos1,
    const std::optional<sf::Vector2f> &mouse_pos2
) {
  if (!doc_ || page_idx >= pages_.size())
    return {};

  PDFPage &page = pages_[page_idx];
  if (!page.isContentLoaded())
    loadPageContent(page_idx);

  const auto pts = selectionPoints(page_idx, direction, mouse_pos1, mouse_pos2);
  if (!pts)
    return {};
  return page.copySelection(ctx_, pts->first, pts->second);
}

const std::vector<OutlineEntry> &PDFDocument::getOutline() const {
  return outline_;
}

bool PDFDocument::isOpen() const {
  return doc_;
}

bool PDFDocument::hasChanges() const {
  return is_edited_;
}

void PDFDocument::loadPageContent(std::size_t idx) {
  fz_stext_page *raw = nullptr;
  fz_var(raw);

  fz_try(ctx_) raw = fz_new_stext_page_from_page_number(ctx_, doc_, idx, nullptr);
  fz_catch(ctx_) throw std::runtime_error("Failed to load text from page");

  pages_[idx].loadContent(FzStextPagePtr(raw, FzStextDeleter{ctx_}));
}

void PDFDocument::saveChanges(const std::filesystem::path &path) {
  fz_try(ctx_) {
    pdf_write_options opts = pdf_default_write_options;
    pdf_document *pdf = pdf_specifics(ctx_, doc_);
    if (!pdf)
      return;
    if (path == "")
      pdf_save_document(ctx_, pdf, filepath_.c_str(), &opts);
    else
      pdf_save_document(ctx_, pdf, path.c_str(), &opts);
  }
  fz_catch(ctx_) throw std::runtime_error("Failed to save changes in document");
}

std::vector<std::size_t>
PDFDocument::createAnnotations(enum pdf_annot_type type, const sf::Color &color) {
  std::vector<std::size_t> changed;
  if (!doc_)
    return changed;

  for (std::size_t i = 0; i < pages_.size(); ++i) {
    PDFPage &page = pages_[i];
    if (!page.hasSelections())
      continue;

    const std::vector<fz_quad> &quads = page.getSelectionFzQuads();
    if (!quads.empty() && addAnnotationInPage(i, quads, type, color)) {
      page.clearSelections();
      changed.push_back(i);
    }
  }

  is_edited_ = is_edited_ ? true : !changed.empty();

  return changed;
}

bool PDFDocument::removeAnnotations(std::size_t page_idx, const sf::Vector2f &page_loc) {
  if (!doc_ || page_idx >= pages_.size())
    return false;

  const fz_point p = fz_make_point(page_loc.x, page_loc.y);
  const bool removed = removeAnnotationsInPage(page_idx, p);

  if (removed)
    is_edited_ = true;

  return removed;
}

void PDFDocument::clearSelections() {
  for (auto &page : pages_) {
    if (page.hasSelections())
      page.clearSelections();
  }
}

std::size_t PDFDocument::pageCount() const {
  return pages_.size();
}

FzContextPtr PDFDocument::cloneContext() const {
  return FzContextPtr(fz_clone_context(ctx_), &fz_drop_context);
}

fz_document *PDFDocument::getDoc() const {
  return doc_;
}

PDFDocument::~PDFDocument() {
  closeDocument();

  if (ctx_) {
    fz_drop_context(ctx_);
    ctx_ = nullptr;
  }
}

void PDFDocument::lockMutex(void *user, int lock) {
  static_cast<PDFDocument *>(user)->mutexes_[lock].lock();
}

void PDFDocument::unlockMutex(void *user, int lock) {
  static_cast<PDFDocument *>(user)->mutexes_[lock].unlock();
}

void PDFDocument::loadOutline() {
  outline_.clear();
  fz_outline *root = nullptr;

  fz_try(ctx_) root = fz_load_outline(ctx_, doc_);
  fz_catch(ctx_) return;

  if (!root)
    return;

  std::vector<int> counters;

  std::function<void(fz_outline *, int)> walk = [&](fz_outline *node, int depth) {
    if (depth >= static_cast<int>(counters.size()))
      counters.resize(depth + 1, 0);
    counters.resize(depth + 1);

    while (node) {
      counters[depth]++;

      std::string index;
      for (int i = 0; i <= depth; i++) {
        if (i > 0)
          index += '.';
        index += std::to_string(counters[i]);
      }

      int page_num = fz_page_number_from_location(ctx_, doc_, node->page);
      outline_.emplace_back(index, node->title ? node->title : "(untitled)", page_num);

      if (node->down)
        walk(node->down, depth + 1);

      node = node->next;
    }
  };

  walk(root, 0);
  fz_drop_outline(ctx_, root);
}

bool PDFDocument::addAnnotationInPage(
    std::size_t page_idx,
    const std::vector<fz_quad> &quads,
    enum pdf_annot_type type,
    const sf::Color &color
) {
  pdf_document *pdf = pdf_specifics(ctx_, doc_);
  if (!pdf)
    return false;

  const float rgb[3] = {color.r / 255.f, color.g / 255.f, color.b / 255.f};
  const float opacity = color.a / 255.f;
  const fz_quad *quad_data = quads.data();
  const int n = static_cast<int>(quads.size());
  const int page_no = static_cast<int>(page_idx);

  pdf_page *page = nullptr;
  pdf_annot *annot = nullptr;
  bool ok = false;
  fz_var(page);
  fz_var(annot);
  fz_var(ok);

  fz_try(ctx_) {
    page = pdf_load_page(ctx_, pdf, page_no);
    annot = pdf_create_annot(ctx_, page, type);
    pdf_set_annot_quad_points(ctx_, annot, n, quad_data);
    pdf_set_annot_color(ctx_, annot, 3, rgb);
    pdf_set_annot_opacity(ctx_, annot, opacity);
    pdf_update_annot(ctx_, annot);
    ok = true;
  }
  fz_always(ctx_) {
    if (annot)
      pdf_drop_annot(ctx_, annot);
    if (page)
      fz_drop_page(ctx_, &page->super);
  }
  fz_catch(ctx_) {
    ok = false;
  }

  return ok;
}

std::optional<std::pair<fz_point, fz_point>> PDFDocument::selectionPoints(
    std::size_t page_idx,
    PDFSelectionDirection direction,
    const std::optional<sf::Vector2f> &mouse_pos1,
    const std::optional<sf::Vector2f> &mouse_pos2
) const {
  const fz_rect bounds = pages_[page_idx].getPageBounds();
  fz_point a = fz_make_point(bounds.x0, bounds.y0);
  fz_point b = fz_make_point(bounds.x1, bounds.y1);

  switch (direction) {
  case PDFSelectionDirection::ALL:
    break;
  case PDFSelectionDirection::UP:
    if (!mouse_pos1)
      return std::nullopt;
    b = fz_make_point(mouse_pos1->x, mouse_pos1->y);
    break;
  case PDFSelectionDirection::DOWN:
    if (!mouse_pos1)
      return std::nullopt;
    a = fz_make_point(mouse_pos1->x, mouse_pos1->y);
    break;
  case PDFSelectionDirection::INSIDE:
    if (!mouse_pos1 || !mouse_pos2)
      return std::nullopt;
    a = fz_make_point(mouse_pos1->x, mouse_pos1->y);
    b = fz_make_point(mouse_pos2->x, mouse_pos2->y);
    if (b.y < a.y || (b.y == a.y && b.x < a.x))
      std::swap(a, b);
    break;
  }
  return std::make_pair(a, b);
}

bool PDFDocument::removeAnnotationsInPage(std::size_t page_idx, const fz_point &p) {
  pdf_document *pdf = pdf_specifics(ctx_, doc_);
  if (!pdf)
    return false;

  pdf_page *page = nullptr;
  bool removed = false;
  fz_var(page);
  fz_var(removed);

  fz_try(ctx_) {
    page = pdf_load_page(ctx_, pdf, static_cast<int>(page_idx));

    pdf_annot *target = nullptr;

    for (pdf_annot *a = pdf_first_annot(ctx_, page); a; a = pdf_next_annot(ctx_, a)) {
      const enum pdf_annot_type type = pdf_annot_type(ctx_, a);
      if (type == PDF_ANNOT_WIDGET || type == PDF_ANNOT_POPUP)
        continue;

      const fz_rect bounds = pdf_bound_annot(ctx_, a);
      if (!fz_is_point_inside_rect(p, fz_expand_rect(bounds, 1.0f)))
        continue;

      bool hit = false;
      const int qn = pdf_annot_quad_point_count(ctx_, a);
      if (!qn)
        continue;

      for (int k = 0; k < qn && !hit; ++k)
        hit = fz_is_point_inside_quad(p, pdf_annot_quad_point(ctx_, a, k));

      if (hit) {
        target = a;
        break;
      }
    }

    if (target) {
      pdf_delete_annot(ctx_, page, target);
      removed = true;
    }
  }
  fz_always(ctx_) {
    if (page)
      fz_drop_page(ctx_, &page->super);
  }
  fz_catch(ctx_) removed = false;

  return removed;
}

} // namespace pdf
