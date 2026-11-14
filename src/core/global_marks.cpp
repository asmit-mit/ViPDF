#include "core/global_marks.h"

namespace core {

namespace {

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

GlobalMarks::GlobalMarks(Storage &storage) : storage_(storage) {}

bool GlobalMarks::addMark(
    const std::string &name,
    const std::string &doc_path,
    int page,
    double x,
    double y,
    double zoom,
    int rotate
) {
  SQLite::Database &db = storage_.db();
  SQLite::Transaction tx(db);

  const int64_t doc_id = storage_.ensureDocument(doc_path);

  SQLite::Statement ins(db, R"(
    INSERT OR IGNORE INTO global_marks (name, doc_id, page, x, y, zoom, rotate)
    VALUES (?, ?, ?, ?, ?, ?, ?)
  )");
  ins.bind(1, name);
  ins.bind(2, doc_id);
  ins.bind(3, page);
  ins.bind(4, x);
  ins.bind(5, y);
  ins.bind(6, zoom);
  ins.bind(7, rotate);

  if (ins.exec() == 0)
    return false;

  tx.commit();
  return true;
}

std::optional<GlobalMark> GlobalMarks::getMark(const std::string &name) const {
  SQLite::Statement query(storage_.db(), std::string(SELECT_MARKS) + " WHERE g.name = ?");
  query.bind(1, name);

  if (query.executeStep())
    return rowToMark(query);
  return std::nullopt;
}

std::vector<GlobalMark> GlobalMarks::getMarks(const std::string &name_filter) const {
  SQLite::Statement
      query(storage_.db(), std::string(SELECT_MARKS) + " WHERE g.name LIKE ? ESCAPE '\\' ORDER BY g.name");
  query.bind(1, "%" + escapeLike(name_filter) + "%");

  std::vector<GlobalMark> result;
  while (query.executeStep())
    result.push_back(rowToMark(query));
  return result;
}

bool GlobalMarks::remove(const std::string &name) {
  SQLite::Statement del(storage_.db(), "DELETE FROM global_marks WHERE name = ?");
  del.bind(1, name);
  return del.exec() > 0;
}

int GlobalMarks::clear() {
  SQLite::Statement del(storage_.db(), "DELETE FROM global_marks");
  return del.exec();
}

GlobalMark GlobalMarks::rowToMark(SQLite::Statement &q) const {
  return GlobalMark{
      q.getColumn(0).getString(),
      q.getColumn(1).getString(),
      q.getColumn(2).getInt(),
      static_cast<float>(q.getColumn(3).getDouble()),
      static_cast<float>(q.getColumn(4).getDouble()),
      static_cast<float>(q.getColumn(5).getDouble()),
      q.getColumn(6).getInt(),
  };
}

} // namespace core
