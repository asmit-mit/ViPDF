#include <stdexcept>
#include <utf8.h>
#include <vector>

#include "core/cmd_processor.h"
#include "utils/utils.h"

namespace core {

CmdProcessor::CmdProcessor(
    EventBus &event_bus, const CmdLoader &cmd_loader, HistoryManager &cmd_history
)
    : cmd_loader_(cmd_loader), cmd_history_(cmd_history), event_bus_(event_bus) {}

void CmdProcessor::runCommand(const std::string &cmd) {
  if (cmd.empty())
    return;

  std::vector<std::string> argv;
  tokenize(cmd, argv);
  if (argv.empty())
    return;

  if (utils::isNumber(argv[0])) {
    event_bus_.emit("cmd.switch_page", std::stoi(argv[0]));
    return;
  }

  std::string name = argv[0];
  const Cmd *cmd_ptr = cmd_loader_.find(name);
  if (!cmd_ptr)
    throw std::runtime_error("Not a valid command: " + name);

  std::size_t consumed = 1;
  while (!cmd_ptr->event) {
    if (consumed >= argv.size())
      throw std::runtime_error("Incomplete command: " + name);

    name += "." + argv[consumed++];
    cmd_ptr = cmd_loader_.find(name);
    if (!cmd_ptr)
      throw std::runtime_error("Not a valid subcommand: " + name);
  }

  emitCommand(*cmd_ptr, argv, consumed);
}

void CmdProcessor::emitCommand(
    const Cmd &cmd, const std::vector<std::string> &argv, std::size_t consumed
) {
  const std::string &event = cmd.event.value();

  if (argv.size() < (size_t)*cmd.args) {
    if (cmd.default_arg) {
      event_bus_.emit(event, *cmd.default_arg);
      return;
    }
    throw std::runtime_error(
        "Provided " + std::to_string(argv.size() - 1) + " argument(s), but required " +
        std::to_string(*cmd.args - 1)
    );
  }

  if (consumed < argv.size())
    event_bus_.emit(event, argv[consumed]);
  else if (consumed == 1 && cmd.args != 1)
    event_bus_.emit(event, std::string{});
  else
    event_bus_.emit(event, true);
}

void CmdProcessor::tokenize(const std::string &input, std::vector<std::string> &argv) {
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

} // namespace core
