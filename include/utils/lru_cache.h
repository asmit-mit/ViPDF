#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace utils {

template <typename K, typename V, typename Hash = std::hash<K>> class LRUCache {
public:
  struct Entry {
    const K key;
    V value;
  };

  using iterator = typename std::list<Entry>::iterator;
  using const_iterator = typename std::list<Entry>::const_iterator;

  explicit LRUCache(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0)
      throw std::invalid_argument("LRU cache capacity must be greater than 0.");
  }

  bool contains(const K &key) const {
    return map_.find(key) != map_.end();
  }

  V *get(const K &key) {
    auto it = map_.find(key);
    if (it == map_.end())
      return nullptr;

    cache_.splice(cache_.begin(), cache_, it->second);
    return &it->second->value;
  }

  template <typename Value> void put(const K &key, Value &&value) {
    auto it = map_.find(key);
    if (it != map_.end()) {
      it->second->value = std::forward<Value>(value);
      cache_.splice(cache_.begin(), cache_, it->second);
      return;
    }

    if (cache_.size() >= capacity_)
      evictLRU();

    cache_.emplace_front(Entry{key, std::forward<Value>(value)});
    map_[cache_.front().key] = cache_.begin();
  }

  bool erase(const K &key) {
    auto it = map_.find(key);
    if (it == map_.end())
      return false;

    cache_.erase(it->second);
    map_.erase(it);
    return true;
  }

  void clear() {
    cache_.clear();
    map_.clear();
  }

  std::size_t size() const {
    return cache_.size();
  }

  std::size_t capacity() const {
    return capacity_;
  }

  bool empty() const {
    return cache_.empty();
  }

  iterator begin() {
    return cache_.begin();
  }
  iterator end() {
    return cache_.end();
  }
  const_iterator begin() const {
    return cache_.begin();
  }
  const_iterator end() const {
    return cache_.end();
  }
  const_iterator cbegin() const {
    return cache_.cbegin();
  }
  const_iterator cend() const {
    return cache_.cend();
  }

private:
  void evictLRU() {
    map_.erase(cache_.back().key);
    cache_.pop_back();
  }

  std::size_t capacity_;
  std::list<Entry> cache_;
  std::unordered_map<K, iterator, Hash> map_;
};

} // namespace utils
