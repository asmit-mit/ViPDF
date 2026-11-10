#include "core/bookmark_manager.h"

namespace core {

namespace {

#define SQLITE_CONSTRAINT 19 /* Abort due to constraint violation */
#define SQLITE_CONSTRAINT_UNIQUE (SQLITE_CONSTRAINT | (8 << 8))

std::string escapeLike(const std::string &s) {
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
      x      REAL    NOT NULL DEFAULT 0.0,
      y      REAL    NOT NULL DEFAULT 0.0,
      label  TEXT    NOT NULL COLLATE NOCASE,
      UNIQUE (doc_id, label)
    );
  )");
}

Bookmark BookmarkManager::rowToBookmark(SQLite::Statement &q) const {
  return Bookmark{
      q.getColumn(0).getInt64(),
      q.getColumn(1).getInt(),
      static_cast<float>(q.getColumn(2).getDouble()),
      static_cast<float>(q.getColumn(3).getDouble()),
      q.getColumn(4).getString(),
  };
}

BookmarkManager::AddResult
BookmarkManager::addBookmark(int page_number, double x, double y, const std::string &label) {
  if (!path_set_)
    return BookmarkManager::AddResult::NoDocument;

  const std::string final_label = label.empty() ? "Page " + std::to_string(page_number) : label;

  SQLite::Transaction tx(db_);

  SQLite::Statement ensure_doc(db_, "INSERT OR IGNORE INTO documents (path) VALUES (?)");
  ensure_doc.bind(1, curr_document_path_);
  ensure_doc.exec();

  SQLite::Statement insert(db_, R"(
    INSERT INTO bookmarks (doc_id, page, x, y, label)
    SELECT id, ?, ?, ?, ? FROM documents WHERE path = ?
  )");
  insert.bind(1, page_number);
  insert.bind(2, x);
  insert.bind(3, y);
  insert.bind(4, final_label);
  insert.bind(5, curr_document_path_);

  try {
    insert.exec();
  } catch (const SQLite::Exception &e) {
    if (e.getExtendedErrorCode() == SQLITE_CONSTRAINT_UNIQUE)
      return BookmarkManager::AddResult::DuplicateLabel;
    throw;
  }

  tx.commit();
  return BookmarkManager::AddResult::Ok;
}

bool BookmarkManager::removeBookmark(const std::string &label) {
  if (!path_set_)
    return false;

  SQLite::Statement del(db_, R"(
    DELETE FROM bookmarks
    WHERE label = ?
      AND doc_id = (SELECT id FROM documents WHERE path = ?)
  )");
  del.bind(1, label);
  del.bind(2, curr_document_path_);
  return del.exec() > 0;
}

std::vector<Bookmark> BookmarkManager::getBookmarks(const std::string &label_filter) const {
  if (!path_set_)
    return {};

  SQLite::Statement query(db_, R"(
    SELECT b.id, b.page, b.x, b.y, b.label
    FROM bookmarks b
    JOIN documents d ON d.id = b.doc_id
    WHERE d.path = ?
      AND b.label LIKE ? ESCAPE '\'
    ORDER BY b.page, b.y, b.x, b.id
  )");
  query.bind(1, curr_document_path_);
  query.bind(2, escapeLike(label_filter) + "%");

  std::vector<Bookmark> result;
  while (query.executeStep())
    result.push_back(rowToBookmark(query));
  return result;
}

std::optional<Bookmark> BookmarkManager::getBookmarkByLabel(const std::string &label) const {
  if (!path_set_)
    return std::nullopt;

  SQLite::Statement query(db_, R"(
    SELECT b.id, b.page, b.x, b.y, b.label
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

} // namespace core
