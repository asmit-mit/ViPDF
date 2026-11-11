#pragma once

#include "core/cmd_loader.h"
#include "core/event_bus.h"
#include "core/history_manager.h"

namespace core {

class CmdProcessor {
public:
  CmdProcessor(EventBus &event_bus, const CmdLoader &cmd_loader, HistoryManager &cmd_history);

  void runCommand(const std::string &cmd);

private:
  void tokenize(const std::string &cmd, std::vector<std::string> &argv);
  void emitCommand(const Cmd &cmd, const std::vector<std::string> &argv, std::size_t consumed);

private:
  const CmdLoader &cmd_loader_;

  HistoryManager &cmd_history_;

  core::EventBus &event_bus_;
};

} // namespace core
