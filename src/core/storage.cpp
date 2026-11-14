#include "core/storage.h"

#include <stdexcept>

namespace core {

Storage::Storage(const std::string &db_path)
    : db_(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
  initSchema();
}

SQLite::Database &Storage::db() {
  return db_;
}

int64_t Storage::ensureDocument(const std::string &path) {
  SQLite::Statement ins(db_, "INSERT OR IGNORE INTO documents (path) VALUES (?)");
  ins.bind(1, path);
  ins.exec();

  SQLite::Statement sel(db_, "SELECT id FROM documents WHERE path = ?");
  sel.bind(1, path);
  if (!sel.executeStep())
    throw std::runtime_error("Storage::ensureDocument: document row missing");
  return sel.getColumn(0).getInt64();
}

void Storage::initSchema() {
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
      zoom   REAL    NOT NULL DEFAULT 1.0,
      rotate INTEGER NOT NULL DEFAULT 0,
      label  TEXT    NOT NULL COLLATE NOCASE,
      UNIQUE (doc_id, label)
    );
  )");

  db_.exec(R"(
    CREATE TABLE IF NOT EXISTS global_marks (
      name   TEXT    PRIMARY KEY,
      doc_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE,
      page   INTEGER NOT NULL,
      x      REAL    NOT NULL DEFAULT 0.0,
      y      REAL    NOT NULL DEFAULT 0.0,
      zoom   REAL    NOT NULL DEFAULT 1.0,
      rotate INTEGER NOT NULL DEFAULT 0
    );
  )");
}

} // namespace core
