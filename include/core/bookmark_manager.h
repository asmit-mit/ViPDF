#pragma once

#include "core/storage.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace core {

struct Bookmark {
  std::int64_t id;
  int page;
  float x;
  float y;
  float zoom;
  int rotate;
  std::string label;
};

class BookmarkManager {
public:
  enum class AddResult { Ok, DuplicateLabel, NoDocument };

  explicit BookmarkManager(Storage &storage);

  AddResult addBookmark(
      int page_number, double x, double y, double zoom, int rotate, const std::string &label
  );
  bool removeBookmark(const std::string &label);
  int removeAllBookmarks();

  std::vector<Bookmark> getBookmarks(const std::string &label_filter = "") const;
  std::optional<Bookmark> getBookmarkByLabel(const std::string &label) const;

  void setDocumentPath(const std::string &doc_path);
  void resetPath();

private:
  Bookmark rowToBookmark(SQLite::Statement &q) const;

private:
  Storage &storage_;
  std::string curr_document_path_;
  bool path_set_ = false;
};

} // namespace core
