#include "core/cmd_autocomplete.h"

#include <stdexcept>
#include <utf8.h>

#include "core/cmd_loader.h"
#include "utils/utils.h"

namespace core {

CmdAutocomplete::CmdAutocomplete(
    const CmdLoader &cmd_loader,
    const HistoryManager &file_history,
    const BookmarkManager &bookmark_manager
)
    : cmd_loader_(cmd_loader), file_history_(file_history), bookmark_manager_(bookmark_manager) {
  for (const Cmd *cmd : cmd_loader_.getRootCmds())
    trie_.insert(cmd->name);
}

std::vector<CmdAutocompleteItem> CmdAutocomplete::complete(const std::string &prefix) {
  std::vector<CmdAutocompleteItem> res;
  std::vector<std::string> argv;
  tokenize(prefix, argv);

  auto trailingSpaceIsDelimiter = [](const std::string &s) -> bool {
    if (s.empty() || s.back() != ' ')
      return false;
    int backslashes = 0;
    for (int i = (int)s.size() - 2; i >= 0 && s[i] == '\\'; --i)
      ++backslashes;
    return backslashes % 2 == 0;
  };

  if (argv.empty()) {
    for (const Cmd *cmd : cmd_loader_.getRootCmds())
      push(cmd, cmd->name + " ", res);
    return res;
  }

  if (argv.size() == 1 && !trailingSpaceIsDelimiter(prefix)) {
    for (const std::string &name : trie_.complete(argv[0]))
      if (const Cmd *cmd = cmd_loader_.find(name))
        push(cmd, cmd->name + " ", res);
    return res;
  }

  std::vector<std::string> confirmed;
  std::string typing;

  if (trailingSpaceIsDelimiter(prefix)) {
    confirmed = argv;
    typing = "";
  } else {
    confirmed = std::vector<std::string>(argv.begin(), argv.end() - 1);
    typing = argv.back();
  }

  std::string scoped_key = confirmed[0];
  for (size_t i = 1; i < confirmed.size(); ++i)
    scoped_key += "." + confirmed[i];

  const Cmd *cmd = cmd_loader_.find(scoped_key);
  if (!cmd)
    return res;

  switch (cmd->cmp_type) {
  case CmdCmpType::Subcommand:
    getSubcmdCmp(scoped_key, typing, res);
    break;
  case CmdCmpType::File:
    getFileCmp(scoped_key, typing, res);
    break;
  case CmdCmpType::Bookmark:
    getBookmarkCmp(scoped_key, typing, res);
    break;
  default:
    throw std::runtime_error("Not valid completion type");
  }

  return res;
}

void CmdAutocomplete::tokenize(const std::string &input, std::vector<std::string> &argv) {
  std::string token;
  bool escaping = false;
  char quote = 0;
  bool hasToken = false;

  for (char ch : input) {
    if (escaping) {
      token += ch;
      escaping = false;
    } else if (ch == '\\' && quote != '\'') {
      escaping = true;
      hasToken = true;
    } else if (quote) {
      if (ch == quote)
        quote = 0;
      else
        token += ch;
    } else if (ch == '"' || ch == '\'') {
      quote = ch;
      hasToken = true;
    } else if (std::isspace(static_cast<unsigned char>(ch))) {
      if (hasToken) {
        argv.push_back(token);
        token.clear();
        hasToken = false;
      }
    } else {
      token += ch;
      hasToken = true;
    }
  }

  if (hasToken)
    argv.push_back(token);
}

void CmdAutocomplete::push(
    const Cmd *cmd, const std::string &full_cmd, std::vector<CmdAutocompleteItem> &results
) {
  results.push_back({
      cmd->name,
      full_cmd,
      cmd->description,
  });
}

void CmdAutocomplete::getSubcmdCmp(
    const std::string &scoped_key,
    const std::string &typing,
    std::vector<CmdAutocompleteItem> &results
) {
  for (const Cmd *child : cmd_loader_.childrenOf(scoped_key)) {
    if (child->name.starts_with(typing)) {
      std::string full = scoped_key + " " + child->name;
      std::replace(full.begin(), full.end(), '.', ' ');
      push(child, full + " ", results);
    }
  }
}

void CmdAutocomplete::getFileCmp(
    const std::string &scoped_key,
    const std::string &typing,
    std::vector<CmdAutocompleteItem> &results
) {
  auto escapeSpaces = [](const std::string &s) {
    std::string res;
    for (char c : s) {
      if (c == ' ')
        res += '\\';
      res += c;
    }
    return res;
  };

  std::string resolved = utils::resolvePath(typing);
  std::string top_cmd = scoped_key;
  std::replace(top_cmd.begin(), top_cmd.end(), '.', ' ');

  if (typing.empty()) {
    for (const auto &file : file_history_.getAllUnique())
      results.push_back({file, top_cmd + " " + escapeSpaces(file), ""});
    return;
  }

  std::string dir;
  std::string prefix;

  auto slash = resolved.rfind('/');
  if (slash == std::string::npos) {
    dir = ".";
    prefix = resolved;
  } else {
    dir = resolved.substr(0, slash + 1);
    prefix = resolved.substr(slash + 1);
  }

  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    std::string name = entry.path().filename().string();
    if (name.starts_with(prefix)) {
      if (entry.is_directory()) {
        std::string file = dir + name + '/';
        results.push_back({file, top_cmd + " " + escapeSpaces(file), ""});
      } else if (entry.path().extension() == ".pdf") {
        std::string file = dir + name;
        results.push_back({file, top_cmd + " " + escapeSpaces(file), ""});
      }
    }
  }
}

void CmdAutocomplete::getBookmarkCmp(
    const std::string &scoped_key,
    const std::string &typing,
    std::vector<CmdAutocompleteItem> &results
) {
  const auto bookmarks = bookmark_manager_.getBookmarks(typing);

  for (const auto &bmark : bookmarks) {
    std::string full = scoped_key + " " + std::to_string(bmark.id);
    std::replace(full.begin(), full.end(), '.', ' ');
    results.push_back({bmark.label, full + " ", "Page " + std::to_string(bmark.page_number)});
  }
}

} // namespace core
