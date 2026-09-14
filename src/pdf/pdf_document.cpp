#include <print>
#include <stdexcept>

#include "pdf/pdf_document.h"

namespace pdf {

PDFDocument::PDFDocument() {
  locks_.user = this;
  locks_.lock = &PDFDocument::lockMutex;
  locks_.unlock = &PDFDocument::unlockMutex;

  all_content_loaded_ = false;

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
  fz_catch(ctx_) {
    throw std::runtime_error("Failed to open document: " + filepath);
  }

  int page_count;
  fz_try(ctx_) page_count = fz_count_pages(ctx_, doc_);
  fz_catch(ctx_) {
    throw std::runtime_error("Failed to count number of pages");
  }

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
  loadOutline();
}

void PDFDocument::closeDocument() {
  if (doc_) {
    fz_drop_document(ctx_, doc_);
    doc_ = nullptr;
  }

  pages_.clear();
  all_content_loaded_ = false;
}

void PDFDocument::reloadDocument() {
  if (filepath_.empty())
    return;
  std::println("reload {}", filepath_);
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

const std::vector<OutlineEntry> &PDFDocument::getOutline() const {
  return outline_;
}

bool PDFDocument::isOpen() const {
  return doc_;
}

bool PDFDocument::isAllContentLoaded() const {
  return all_content_loaded_;
}

void PDFDocument::loadAllContent() {
  if (!doc_)
    return;

  for (std::size_t i = 0; i < pages_.size(); i++)
    loadPageContent(i);
  all_content_loaded_ = true;
}

void PDFDocument::loadPageContent(std::size_t idx) {
  fz_stext_page *text = nullptr;
  fz_try(ctx_) {
    text = fz_new_stext_page_from_page_number(ctx_, doc_, idx, nullptr);
    pages_[idx].loadContent(text);
  }
  fz_always(ctx_) fz_drop_stext_page(ctx_, text);
  fz_catch(ctx_) throw std::runtime_error("Failed to load text from page");
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

} // namespace pdf
