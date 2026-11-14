#pragma once

#include <string>
#include <vector>

namespace containers {

struct TrieNode {
  TrieNode *children[26];
  bool is_terminal;

  TrieNode() {
    is_terminal = false;
    for (int i = 0; i < 26; i++)
      children[i] = nullptr;
  }
};

class Trie {
public:
  Trie();
  ~Trie();

  void insert(const std::string &word);
  std::vector<std::string> complete(const std::string &prefix) const;

private:
  void backtrack(TrieNode *node, std::string &curr, std::vector<std::string> &matches) const;

  void destroy(TrieNode *node);

private:
  TrieNode *head_;
};

} // namespace utils
