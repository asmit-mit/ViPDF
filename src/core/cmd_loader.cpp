#include "core/cmd_loader.h"

#include <fstream>
#include <stdexcept>

namespace core {

CmdLoader::CmdLoader(const std::filesystem::path path) {
  std::ifstream file(path);
  if (!file)
    throw std::runtime_error("Failed to load commands from: " + path.string());

  const json data = json::parse(file);
  if (!data.is_array())
    throwJsonReadError("not an array");

  for (const auto &cmd : data)
    parseCmd(cmd, "", 0);
  commands_.shrink_to_fit();
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

std::size_t CmdLoader::parseCmd(const json &obj, const std::string &parent_key, int depth) {
  if (!obj.is_object())
    throwJsonReadError("command entry is not an object");

  Cmd cmd;
  cmd.name = requireField<std::string>(obj, "name");
  cmd.description = requireField<std::string>(obj, "description");
  cmd.args = requireField<int>(obj, "args");

  if (auto it = obj.find("event"); it != obj.end() && it->is_string())
    cmd.event = it->get<std::string>();

  if (auto it = obj.find("cmp_type"); it != obj.end() && it->is_string())
    cmd.cmp_type = it->get<CmdCmpType>();

  std::string alias;
  if (auto it = obj.find("alias"); it != obj.end() && it->is_string())
    alias = it->get<std::string>();

  std::string alias_key;
  std::string scoped_key = parent_key.empty() ? cmd.name : parent_key + "." + cmd.name;
  if (!alias.empty())
    alias_key = parent_key.empty() ? alias : parent_key + "." + alias;

  size_t idx = commands_.size();
  if (depth == 0)
    roots_.push_back(idx);
  commands_.push_back({});

  if (auto it = obj.find("children"); it != obj.end() && it->is_array()) {
    for (const auto &child : *it) {
      size_t child_idx = parseCmd(child, scoped_key, depth + 1);
      commands_[idx].children.push_back(child_idx);
    }
  }

  cmd.children = std::move(commands_[idx].children);
  commands_[idx] = std::move(cmd);
  index_[scoped_key] = idx;
  if (!alias_key.empty())
    index_[alias_key] = idx;
  return idx;
}

void CmdLoader::throwJsonReadError(const std::string_view error) const {
  throw std::runtime_error(std::format("Invalid commands.json: {}", error));
}

} // namespace core
