#include "core/history_manager.h"

#include <fstream>
#include <iostream>
#include <utf8.h>

namespace core {

HistoryManager::HistoryManager(const std::filesystem::path path) {
  std::ifstream file(path);

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty())
      continue;

    std::string cmd = line;

    if (save_unique_)
      history_set_.insert(cmd);
    else
      history_.push_back(cmd);
  }

  reset();

  history_file_ = std::move(path);
}

void HistoryManager::setSaveUnique(bool unique) {
  if (save_unique_ == unique)
    return;

  save_unique_ = unique;

  if (save_unique_) {
    history_set_.clear();
    for (const auto &cmd : history_)
      history_set_.insert(cmd);
    history_.clear();
  } else {
    history_.clear();
    for (const auto &cmd : history_set_)
      history_.push_back(cmd);
    history_set_.clear();
  }

  reset();
  save();
}

void HistoryManager::clear() {
  history_.clear();
  history_set_.clear();

  reset();
  save();
}

void HistoryManager::reset() {
  curr_idx_ = save_unique_ ? history_set_.size() : history_.size();
}

void HistoryManager::add(const std::string &cmd) {
  if (cmd.empty())
    return;

  if (save_unique_) {
    if (history_set_.size() >= max_size_)
      return;

    auto [it, inserted] = history_set_.insert(cmd);

    if (!inserted)
      return;
  } else {
    if (history_.empty() || history_.back() != cmd)
      history_.push_back(cmd);

    if (history_.size() > max_size_)
      history_.pop_front();
  }

  reset();
  save();
}

std::string HistoryManager::getNext() {
  if (save_unique_)
    return "";

  if (history_.empty())
    return "";

  if (curr_idx_ < history_.size())
    ++curr_idx_;

  if (curr_idx_ == history_.size())
    return "";

  return history_[curr_idx_];
}

std::string HistoryManager::getPrevious() {
  if (save_unique_)
    return "";

  if (history_.empty())
    return "";

  if (curr_idx_ > 0)
    --curr_idx_;

  return history_[curr_idx_];
}

std::vector<std::string> HistoryManager::getAll() const {
  if (save_unique_)
    return {history_set_.begin(), history_set_.end()};

  return {history_.begin(), history_.end()};
}

std::vector<std::string> HistoryManager::getAllUnique() const {
  if (save_unique_)
    return {history_set_.begin(), history_set_.end()};

  std::unordered_set<std::string> unique;

  for (const auto &cmd : history_)
    unique.insert(cmd);

  return {unique.begin(), unique.end()};
}

void HistoryManager::save() {
  std::ofstream file(history_file_, std::ios::trunc);

  if (!file.is_open()) {
    std::cerr << "Cannot write " << history_file_ << '\n';
    return;
  }

  if (save_unique_) {
    for (const auto &cmd : history_set_) {
      file << cmd << '\n';
    }
  } else {
    for (const auto &cmd : history_) {
      file << cmd << '\n';
    }
  }
}

} // namespace core
