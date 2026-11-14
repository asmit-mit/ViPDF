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
  float x;
  float y;
  std::string label;
};

class BookmarkManager {
public:
  enum class AddResult { Ok, NoDocument, DuplicateLabel };

public:
  explicit BookmarkManager(const std::string &db_path_);

  AddResult addBookmark(int page_number, double x, double y, const std::string &label = "");
  bool removeBookmark(const std::string &label);
  int removeAllBookmarks();

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
