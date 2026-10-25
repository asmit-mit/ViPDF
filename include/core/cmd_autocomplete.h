#pragma once

#include "core/cmd_loader.h"

#include <string>
#include <vector>

#include "core/bookmark_manager.h"
#include "core/history_manager.h"
#include "utils/trie.h"

namespace core {

struct CmdAutocompleteItem {
  std::string name;
  std::string cmd;
  std::string description;
};

class CmdAutocomplete {
public:
  explicit CmdAutocomplete(
      const CmdLoader &cmd_loader,
      const HistoryManager &file_history,
      const BookmarkManager &bookmark_manager
  );

  std::vector<CmdAutocompleteItem> complete(const std::string &prefix);

private:
  void tokenize(const std::string &input, std::vector<std::string> &argv);
  void push(const Cmd *cmd, const std::string &full_cmd, std::vector<CmdAutocompleteItem> &results);

  void getSubcmdCmp(
      const std::string &scoped_key,
      const std::string &typing,
      std::vector<CmdAutocompleteItem> &results
  );

  void getFileCmp(
      const std::string &scoped_key,
      const std::string &typing,
      std::vector<CmdAutocompleteItem> &results
  );

  void getBookmarkCmp(
      const std::string &scoped_key,
      const std::string &typing,
      std::vector<CmdAutocompleteItem> &results
  );

private:
  utils::Trie trie_;
  const CmdLoader &cmd_loader_;
  const HistoryManager &file_history_;
  const BookmarkManager &bookmark_manager_;
};

} // namespace core
