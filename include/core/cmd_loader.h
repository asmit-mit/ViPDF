#pragma once

#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <json.hpp>

namespace core {

using json = nlohmann::json;

enum class CmdCmpType : int {
  Subcommand = 0,
  File,
  Bookmark,
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    CmdCmpType,
    {
        {CmdCmpType::Subcommand, "subcommand"},
        {CmdCmpType::File, "file"},
        {CmdCmpType::Bookmark, "bookmark"},
    }
)

struct Cmd {
  std::string name;
  std::string description;
  std::optional<std::string> event;
  std::vector<size_t> children;
  CmdCmpType cmp_type = CmdCmpType::Subcommand;
  std::optional<int> args;
  std::optional<std::string> default_arg;
};

class CmdLoader {
public:
  explicit CmdLoader(const std::filesystem::path path);

  std::vector<const Cmd *> getRootCmds() const;
  const Cmd *find(const std::string &scoped_key) const;
  std::vector<const Cmd *> childrenOf(const std::string &scoped_key) const;

private:
  std::size_t parseCmd(const json &obj, const std::vector<std::string> &parent_keys, int depth);
  [[noreturn]] void throwJsonReadError(const std::string_view error) const;

  template <typename T> T requireField(const json &obj, const char *key) {
    auto it = obj.find(key);
    if (it == obj.end())
      throwJsonReadError(std::format("missing {} field", key));

    try {
      return it->get<T>();
    } catch (const json::type_error &) {
      throwJsonReadError(std::format("invalid type for {} field", key));
    }
  }

  template <typename T> std::optional<T> optionalField(const json &obj, const char *key) {
    auto it = obj.find(key);
    if (it == obj.end())
      return std::nullopt;

    try {
      return it->get<T>();
    } catch (const json::type_error &) {
      throwJsonReadError(std::format("invalid type for {} field", key));
    }
  }

private:
  std::vector<Cmd> commands_;
  std::vector<std::size_t> roots_;
  std::unordered_map<std::string, std::size_t> index_;
};

} // namespace core
