#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace core {

struct Bookmark {
  std::int64_t id;
  int page_number;
  double x;
  double y;
  std::string label;
};

enum class BmarkAddResult { Ok, NoDocument, DuplicateLabel };

class BookmarkManager {
public:
  explicit BookmarkManager(const std::string &db_path_);

  BmarkAddResult addBookmark(int page_number, double x, double y, const std::string &label = "");
  bool removeBookmark(const std::string &label);

  void setDocumentPath(const std::string &doc_path);
  void resetPath();

  std::vector<Bookmark> getBookmarks(const std::string &label_filter = "") const;
  std::optional<Bookmark> getBookmarkByLabel(const std::string &label) const;

private:
  void initSchema();
  Bookmark rowToBookmark(SQLite::Statement &q) const;

private:
  SQLite::Database db_;
  std::string curr_document_path_;
  bool path_set_;
};

} // namespace core
