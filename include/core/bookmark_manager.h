#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <string>
#include <vector>

namespace core {

struct Bookmark {
  std::int64_t id;
  int page_number;
  std::string label;
};

class BookmarkManager {
public:
  explicit BookmarkManager(const std::string &db_path_);

  void addBookmark(int page_number, const std::string &label = "");
  void removeBookmark(std::int64_t id);
  void setDocumentPath(const std::string &doc_path);
  void resetPath();

  std::vector<Bookmark> getBookmarks(const std::string &label_filter = "") const;
  std::optional<Bookmark> getBookmarkById(std::int64_t id) const;

private:
  void initSchema();

private:
  SQLite::Database db_;
  std::string curr_document_path_;
  bool path_set_;
};

} // namespace core
