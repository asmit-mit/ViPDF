#include "core/bookmark_manager.h"

#include <cstdint>

namespace core {

namespace {
static std::string escapeLike(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == '%' || c == '_' || c == '\\')
      out += '\\';
    out += c;
  }
  return out;
}
} // namespace

BookmarkManager::BookmarkManager(const std::string &db_path_)
    : db_(db_path_, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
  path_set_ = false;
  initSchema();
}

void BookmarkManager::initSchema() {
  db_.exec("PRAGMA foreign_keys = ON;");

  db_.exec(R"(
    CREATE TABLE IF NOT EXISTS documents (
      id   INTEGER PRIMARY KEY,
      path TEXT NOT NULL UNIQUE
    );
  )");

  db_.exec(R"(
    CREATE TABLE IF NOT EXISTS bookmarks (
      id     INTEGER PRIMARY KEY AUTOINCREMENT,
      doc_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE,
      page   INTEGER NOT NULL,
      label  TEXT    NOT NULL DEFAULT ''
    );
  )");

  db_.exec("CREATE INDEX IF NOT EXISTS idx_bookmarks_doc ON bookmarks(doc_id);");
}

void BookmarkManager::addBookmark(int page_number, const std::string &label) {
  if (!path_set_)
    return;

  SQLite::Transaction tx(db_);

  SQLite::Statement ensure_doc(db_, "INSERT OR IGNORE INTO documents (path) VALUES (?)");
  ensure_doc.bind(1, curr_document_path_);
  ensure_doc.exec();

  SQLite::Statement insert(db_, R"(
    INSERT INTO bookmarks (doc_id, page, label)
    SELECT id, ?, ? FROM documents WHERE path = ?
  )");
  insert.bind(1, page_number);
  if (label.empty())
    insert.bind(2, "Page " + std::to_string(page_number));
  else
    insert.bind(2, label);
  insert.bind(3, curr_document_path_);
  insert.exec();

  tx.commit();
}

void BookmarkManager::removeBookmark(std::int64_t id) {
  if (!path_set_)
    return;

  SQLite::Statement del(db_, R"(
    DELETE FROM bookmarks
    WHERE id = ?
  )");
  del.bind(1, id);
  del.exec();
}

std::vector<Bookmark> BookmarkManager::getBookmarks(const std::string &label_filter) const {
  if (!path_set_)
    return {};

  SQLite::Statement query(db_, R"(
    SELECT b.id, b.page, b.label
    FROM bookmarks b
    JOIN documents d ON d.id = b.doc_id
    WHERE d.path = ?
      AND b.label LIKE ? ESCAPE '\'
    ORDER BY b.id
  )");
  query.bind(1, curr_document_path_);
  query.bind(2, "%" + escapeLike(label_filter) + "%");

  std::vector<Bookmark> result;
  while (query.executeStep()) {
    result.push_back({
        query.getColumn(0).getInt64(),
        query.getColumn(1).getInt(),
        query.getColumn(2).getString(),
    });
  }
  return result;
}

std::optional<Bookmark> BookmarkManager::getBookmarkById(std::int64_t id) const {
  if (!path_set_)
    return std::nullopt;

  SQLite::Statement query(db_, R"(
    SELECT b.id, b.page, b.label
    FROM bookmarks b
    JOIN documents d ON d.id = b.doc_id
    WHERE b.id = ?
      AND d.path = ?
  )");
  query.bind(1, id);
  query.bind(2, curr_document_path_);

  if (query.executeStep()) {
    return Bookmark{
        query.getColumn(0).getInt64(),
        query.getColumn(1).getInt(),
        query.getColumn(2).getString(),
    };
  }
  return std::nullopt;
}

void BookmarkManager::setDocumentPath(const std::string &doc_path) {
  curr_document_path_ = doc_path;
  path_set_ = true;
}

void BookmarkManager::resetPath() {
  path_set_ = false;
}

} // namespace core
