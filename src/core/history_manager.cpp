#include "core/history_manager.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <utf8.h>

namespace core {

HistoryManager::HistoryManager(const std::filesystem::path path, std::size_t capacity)
    : capacity_(capacity), history_file_(path) {
  curr_idx_ = 0;
  save_unique_ = false;

  std::ifstream file(path);

  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty())
      history_.push_back(line);
  }

  trim();
  reset();
}

void HistoryManager::trim() {
  while (history_.size() > capacity_)
    history_.pop_front();
}

void HistoryManager::dedupe() {
  std::unordered_set<std::string> seen;
  std::deque<std::string> result;

  for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
    if (seen.insert(*it).second)
      result.push_front(*it);
  }

  history_ = std::move(result);
}

void HistoryManager::setSaveUnique(bool unique) {
  if (save_unique_ == unique)
    return;

  save_unique_ = unique;

  if (save_unique_) {
    dedupe();
    trim();
  }

  reset();
  save();
}

void HistoryManager::clear() {
  history_.clear();
  reset();
  save();
}

void HistoryManager::reset() {
  curr_idx_ = history_.size();
}

void HistoryManager::add(const std::string &cmd) {
  if (cmd.empty())
    return;

  if (save_unique_) {
    history_.erase(std::remove(history_.begin(), history_.end(), cmd), history_.end());
  } else if (!history_.empty() && history_.back() == cmd) {
    reset();
    return;
  }

  history_.push_back(cmd);
  trim();

  reset();
  save();
}

std::string HistoryManager::getNext() {
  if (history_.empty())
    return "";

  if (curr_idx_ < history_.size())
    ++curr_idx_;

  if (curr_idx_ == history_.size())
    return "";

  return history_[curr_idx_];
}

std::string HistoryManager::getPrevious() {
  if (history_.empty())
    return "";

  if (curr_idx_ > 0)
    --curr_idx_;

  return history_[curr_idx_];
}

std::vector<std::string> HistoryManager::getAll() const {
  return {history_.begin(), history_.end()};
}

std::vector<std::string> HistoryManager::getAllUnique() const {
  if (save_unique_)
    return getAll();

  std::unordered_set<std::string> seen;
  std::vector<std::string> result;

  for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
    if (seen.insert(*it).second)
      result.push_back(*it);
  }
  std::reverse(result.begin(), result.end());

  return result;
}

void HistoryManager::save() {
  std::ofstream file(history_file_, std::ios::trunc);

  if (!file.is_open()) {
    std::cerr << "Cannot write " << history_file_ << '\n';
    return;
  }

  for (const auto &cmd : history_)
    file << cmd << '\n';
}

} // namespace core
