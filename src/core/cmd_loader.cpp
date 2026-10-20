#include "core/cmd_loader.h"

#include <stdexcept>

namespace core {

CmdLoader::CmdLoader(const std::filesystem::path path) {
  simdjson::dom::parser parser;
  simdjson::dom::element doc;

  auto error = parser.load(path.generic_string()).get(doc);
  if (error)
    throw std::runtime_error(simdjson::error_message(error));

  for (simdjson::dom::element cmd : doc.get_array())
    parseCmd(cmd, "", 0);
}

std::vector<const Cmd *> CmdLoader::getRootCmds() const {
  std::vector<const Cmd *> res;

  res.reserve(roots_.size());
  for (std::size_t idx : roots_)
    res.push_back(&commands_[idx]);

  return res;
}

const Cmd *CmdLoader::find(const std::string &scoped_key) const {
  auto it = index_.find(scoped_key);
  if (it == index_.end())
    return nullptr;

  return &commands_[it->second];
}

std::vector<const Cmd *> CmdLoader::childrenOf(const std::string &scoped_key) const {
  const Cmd *cmd = find(scoped_key);
  if (!cmd)
    return {};

  std::vector<const Cmd *> res;
  res.reserve(cmd->children.size());
  for (size_t idx : cmd->children)
    res.push_back(&commands_[idx]);

  return res;
}

std::size_t
CmdLoader::parseCmd(simdjson::dom::element obj, const std::string &parent_key, int depth) {
  Cmd cmd;
  cmd.name = std::string(obj["name"].get_string().value());
  cmd.description = std::string(obj["description"].get_string().value());
  cmd.args = static_cast<int32_t>(obj["args"].get_int64().value());

  std::string_view event;
  if (obj["event"].get_string().get(event) == simdjson::SUCCESS)
    cmd.event = std::string(event);

  int64_t cmp_type;
  if (obj["cmp_type"].get_int64().get(cmp_type) == simdjson::SUCCESS)
    cmd.cmp_type = static_cast<int32_t>(cmp_type);

  std::string scoped_key = parent_key.empty() ? cmd.name : parent_key + "." + cmd.name;
  size_t idx = commands_.size();
  if (depth == 0)
    roots_.push_back(idx);
  commands_.push_back({});

  simdjson::dom::array children;
  if (obj["children"].get_array().get(children) == simdjson::SUCCESS) {
    for (simdjson::dom::element child : children) {
      size_t child_idx = parseCmd(child, scoped_key, depth + 1);
      commands_[idx].children.push_back(child_idx);
    }
  }

  cmd.children = std::move(commands_[idx].children);
  commands_[idx] = std::move(cmd);
  index_[scoped_key] = idx;
  return idx;
}

} // namespace core
