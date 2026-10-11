#pragma once

#include <SFML/Graphics.hpp>

#include "core/cmd_autocomplete.h"
#include "core/cmd_processor.h"
#include "core/event_bus.h"
#include "core/history_manager.h"
#include "core/render_scheduler.h"
#include "graphics/font_library.h"
#include "graphics/glyph_atlas.h"
#include "ui/cmdline.h"
#include "ui/notifications.h"
#include "ui/document_view.h"
#include "ui/statusbar.h"
#include "ui/ui_elements.h"

class App {
public:
  App();

  void run();

private:
  void initHistory();
  void initWindow();
  void initUI();
  void initFonts();
  void processEvents();

private:
  sf::RenderWindow window_;
  sf::View view_;
  sf::Font font_regular_;
  sf::Font font_bold_;
  sf::Font font_italic_;

  graphics::FontLibrary font_library_;
  graphics::GlyphAtlas glyph_atlas_;

  pdf::PDFDocument document_;
  pdf::PDFRenderer renderer_;

  core::EventBus event_bus_;
  core::CmdLoader cmd_loader_;
  core::CmdProcessor cmd_processor_;
  core::CmdAutocomplete cmd_autocomplete_;
  core::HistoryManager cmd_history_;
  core::HistoryManager search_history_;
  core::HistoryManager file_history_;
  core::HistoryManager notification_history_;
  core::RenderScheduler render_scheduler_;

  ui::Textbox cmdline_textbox_;
  ui::Completions cmdline_completions_;

  ui::Cmdline cmdline_;
  ui::DocumentView pdf_view_;
  ui::Statusbar statusbar_;
  ui::Notifications notifications_;
  ui::UIElements focus_;

  const unsigned int res_x_ = 640;
  const unsigned int res_y_ = 480;
  const unsigned int fps_ = 60;
};
