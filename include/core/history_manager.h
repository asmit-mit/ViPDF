#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace core {

class HistoryManager {
public:
  HistoryManager(const std::filesystem::path path, std::size_t capacity);

  void setSaveUnique(bool unique);
  void reset();
  void clear();

  void add(const std::string &cmd);

  std::string getNext();
  std::string getPrevious();

  std::vector<std::string> getAll() const;
  std::vector<std::string> getAllUnique() const;

private:
  void save();
  void trim();
  void dedupe();

private:
  std::deque<std::string> history_;

  std::size_t capacity_;
  std::size_t curr_idx_;

  std::filesystem::path history_file_;
  bool save_unique_;
};

} // namespace core
