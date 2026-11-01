#pragma once

#include "core/cmd_autocomplete.h"
#include "core/cmd_processor.h"
#include "core/config_reader.h"
#include "core/event_bus.h"
#include "core/history_manager.h"
#include "ui/completions.h"
#include "ui/textbox.h"
#include "ui/widget.h"

namespace ui {

enum class CmdlineMode {
  Cmd = 0,
  Search,
};

class Cmdline : public Widget {
public:
  explicit Cmdline(
      const sf::Font &font_normal,
      core::EventBus &event_bus,
      core::CmdProcessor &cmd_processor,
      core::CmdAutocomplete &cmd_autocomplete,
      core::HistoryManager &cmd_history,
      core::HistoryManager &search_history,
      ui::Textbox &textbox,
      ui::Completions &completions,
      const core::Config &config
  );

  void draw(sf::RenderTarget &window) const override;
  void update() override;
  void handleEvent(const sf::Event &event) override;

  void setMode(CmdlineMode mode);
  void onResize(const sf::Vector2f &size);

private:
  void reset();
  void refreshCompletions();

private:
  core::EventBus &event_bus_;
  core::CmdProcessor &cmd_processor_;
  core::CmdAutocomplete &cmd_autocomplete_;
  core::HistoryManager &cmd_history_;
  core::HistoryManager &search_history_;

  sf::Text label_;
  ui::Textbox &textbox_;
  ui::Completions &completions_;
  sf::RectangleShape display_area_;

  std::string original_string_;

  CmdlineMode mode_;

  bool visible_;
};

} // namespace ui
