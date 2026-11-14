#pragma once

#include "core/storage.h"

#include <optional>
#include <string>
#include <vector>

namespace core {

class Storage;

struct GlobalMark {
  std::string label;
  std::string doc_path;
  int page;
  float x, y;
  float zoom;
  int rotate;
};

class GlobalMarks {
public:
  explicit GlobalMarks(Storage &storage);

  bool addMark(
      const std::string &name,
      const std::string &doc_path,
      int page,
      double x,
      double y,
      double zoom,
      int rotate
  );

  std::optional<GlobalMark> getMark(const std::string &name) const;
  std::vector<GlobalMark> getMarks(const std::string &name_filter = "") const;

  bool remove(const std::string &name);
  int clear();

private:
  GlobalMark rowToMark(SQLite::Statement &q) const;

private:
  Storage &storage_;

  static constexpr const char *SELECT_MARKS = R"(
    SELECT g.name, d.path, g.page, g.x, g.y, g.zoom, g.rotate
    FROM global_marks g
    JOIN documents d ON d.id = g.doc_id
  )";
};

} // namespace core
