#include "core/bookmark_manager.h"
#include "utils/general.h"

namespace core {

namespace {

#ifndef SQLITE_CONSTRAINT_UNIQUE
#define SQLITE_CONSTRAINT 19
#define SQLITE_CONSTRAINT_UNIQUE (SQLITE_CONSTRAINT | (8 << 8))
#endif

} // namespace

BookmarkManager::BookmarkManager(Storage &storage) : storage_(storage) {}

BookmarkManager::AddResult BookmarkManager::addBookmark(
    int page_number, double x, double y, double zoom, int rotate, const std::string &label
) {
  if (!path_set_)
    return AddResult::NoDocument;

  const std::string final_label = label.empty() ? "Page " + std::to_string(page_number) : label;

  SQLite::Database &db = storage_.db();
  SQLite::Transaction tx(db);

  const int64_t doc_id = storage_.ensureDocument(curr_document_path_);

  SQLite::Statement insert(db, R"(
    INSERT INTO bookmarks (doc_id, page, x, y, zoom, rotate, label)
    VALUES (?, ?, ?, ?, ?, ?, ?)
  )");
  insert.bind(1, doc_id);
  insert.bind(2, page_number);
  insert.bind(3, x);
  insert.bind(4, y);
  insert.bind(5, zoom);
  insert.bind(6, rotate);
  insert.bind(7, final_label);

  try {
    insert.exec();
  } catch (const SQLite::Exception &e) {
    if (e.getExtendedErrorCode() == SQLITE_CONSTRAINT_UNIQUE)
      return AddResult::DuplicateLabel;
    throw;
  }

  tx.commit();
  return AddResult::Ok;
}

bool BookmarkManager::removeBookmark(const std::string &label) {
  if (!path_set_)
    return false;

  SQLite::Statement del(storage_.db(), R"(
    DELETE FROM bookmarks
    WHERE label = ?
      AND doc_id = (SELECT id FROM documents WHERE path = ?)
  )");
  del.bind(1, label);
  del.bind(2, curr_document_path_);
  return del.exec() > 0;
}

int BookmarkManager::removeAllBookmarks() {
  if (!path_set_)
    return 0;

  SQLite::Statement del(storage_.db(), R"(
    DELETE FROM bookmarks
    WHERE doc_id = (SELECT id FROM documents WHERE path = ?)
  )");
  del.bind(1, curr_document_path_);
  return del.exec();
}

std::vector<Bookmark> BookmarkManager::getBookmarks(const std::string &label_filter) const {
  if (!path_set_)
    return {};

  SQLite::Statement query(storage_.db(), R"(
    SELECT b.id, b.page, b.x, b.y, b.zoom, b.rotate, b.label
    FROM bookmarks b
    JOIN documents d ON d.id = b.doc_id
    WHERE d.path = ?
      AND b.label LIKE ? ESCAPE '\'
    ORDER BY b.page, b.y, b.x, b.id
  )");
  query.bind(1, curr_document_path_);
  query.bind(2, "%" + utils::escapeLike(label_filter) + "%");

  std::vector<Bookmark> result;
  while (query.executeStep())
    result.push_back(rowToBookmark(query));
  return result;
}

std::optional<Bookmark> BookmarkManager::getBookmarkByLabel(const std::string &label) const {
  if (!path_set_)
    return std::nullopt;

  SQLite::Statement query(storage_.db(), R"(
    SELECT b.id, b.page, b.x, b.y, b.zoom, b.rotate, b.label
    FROM bookmarks b
    JOIN documents d ON d.id = b.doc_id
    WHERE d.path = ?
      AND b.label = ?
  )");
  query.bind(1, curr_document_path_);
  query.bind(2, label);

  if (query.executeStep())
    return rowToBookmark(query);
  return std::nullopt;
}

void BookmarkManager::setDocumentPath(const std::string &doc_path) {
  curr_document_path_ = doc_path;
  path_set_ = true;
}

void BookmarkManager::resetPath() {
  path_set_ = false;
}

Bookmark BookmarkManager::rowToBookmark(SQLite::Statement &q) const {
  return Bookmark{
      q.getColumn(0).getInt64(),
      q.getColumn(1).getInt(),
      static_cast<float>(q.getColumn(2).getDouble()),
      static_cast<float>(q.getColumn(3).getDouble()),
      static_cast<float>(q.getColumn(4).getDouble()),
      q.getColumn(5).getInt(),
      q.getColumn(6).getString(),
  };
}

} // namespace core
