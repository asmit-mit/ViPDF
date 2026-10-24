#pragma once

#include <mupdf/fitz.h>

#include <array>
#include <memory>
#include <mupdf/pdf.h>
#include <mutex>
#include <string>
#include <vector>

#include "pdf/pdf_page.h"

namespace pdf {

using FzContextPtr = std::unique_ptr<fz_context, decltype(&fz_drop_context)>;

struct OutlineEntry {
  std::string index;
  std::string title;
  int page;

  OutlineEntry(std::string index, std::string title, int page)
      : index(std::move(index)), title(std::move(title)), page(page) {}
};

class PDFDocument {
public:
  PDFDocument();
  ~PDFDocument();

  PDFDocument(const PDFDocument &) = delete;
  PDFDocument &operator=(const PDFDocument &) = delete;

  void openDocument(const std::string &filepath);
  void closeDocument();
  void reloadDocument();

  std::string &getFilePath();
  PDFPage &getPage(std::size_t page_idx);
  const std::vector<OutlineEntry> &getOutline() const;

  bool isOpen() const;
  bool isAllContentLoaded() const;
  bool hasChanges() const;
  void loadAllContent();
  void loadPageContent(std::size_t idx);
  void saveChanges(const std::filesystem::path &path = "");

  std::vector<std::size_t> createAnnotations(enum pdf_annot_type type, const sf::Color &color);
  std::vector<std::size_t> removeAnnotations();
  void clearSelections();

  std::size_t pageWithMaxWidth() const;
  std::size_t pageCount() const;

  FzContextPtr cloneContext() const;
  fz_document *getDoc() const;

private:
  static void lockMutex(void *user, int lock);
  static void unlockMutex(void *user, int lock);

  void loadOutline();
  bool addAnnotationInPage(
      std::size_t page_idx,
      const std::vector<fz_quad> &quads,
      enum pdf_annot_type type,
      const sf::Color &color
  );

  bool removeAnnotationsInPage(std::size_t page_idx, const std::vector<fz_quad> &quads);

private:
  std::array<std::mutex, FZ_LOCK_MAX> mutexes_;
  fz_locks_context locks_;

  std::string filepath_;

  std::vector<OutlineEntry> outline_;
  std::vector<pdf::PDFPage> pages_;
  std::size_t page_with_max_width_;

  fz_context *ctx_;
  fz_document *doc_;

  bool all_content_loaded_;
  bool is_edited_;
};

} // namespace pdf
