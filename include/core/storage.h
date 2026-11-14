#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <string>

namespace core {

class Storage {
public:
  explicit Storage(const std::string &db_path);

  Storage(const Storage &) = delete;
  Storage &operator=(const Storage &) = delete;

  SQLite::Database &db();
  int64_t ensureDocument(const std::string &path);

private:
  void initSchema();

  SQLite::Database db_;
};

} // namespace core
