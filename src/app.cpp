#include "app.h"

#include "core/global_marks.h"
#include "ui/document_view.h"
#include "ui/external_link_dialog.h"
#include "utils/constants.h"
#include "utils/filesystem.h"
#include "utils/general.h"
#include <clip.h>

namespace fs = std::filesystem;

App::App()
    : asset_dir_(utils::assetDir()), state_dir_(prepareState(asset_dir_)),
      font_regular_(asset_dir_ / utils::FONT_REGULAR), font_bold_(asset_dir_ / utils::FONT_BOLD),
      font_italic_(asset_dir_ / utils::FONT_ITALITC),
      font_library_({
          {graphics::FontType::Regular, (asset_dir_ / utils::FONT_REGULAR).c_str()},
          {graphics::FontType::Bold, (asset_dir_ / utils::FONT_BOLD).c_str()},
          {graphics::FontType::Italic, (asset_dir_ / utils::FONT_ITALITC).c_str()},
          {graphics::FontType::CJK, (asset_dir_ / utils::FONT_CJK).c_str()},
          {graphics::FontType::Emoji, (asset_dir_ / utils::FONT_EMOJI).c_str()},
      }),
      config_reader_(utils::configPath()),
      cmd_history_(utils::ensureParent(state_dir_ / utils::CMD_HISTORY), config_reader_.getConfig().cmd_history_count),
      search_history_(
          utils::ensureParent(state_dir_ / utils::SEARCH_HISTORY),
          config_reader_.getConfig().search_history_count
      ),
      file_history_(
          utils::ensureParent(state_dir_ / utils::RECENT_FILES),
          config_reader_.getConfig().file_history_count
      ),
      database_(utils::ensureParent(state_dir_ / utils::BOOKMARKS_DB)),
      bookmark_manager_(database_), global_marks_(database_),
      cmd_loader_(utils::ensureParent(asset_dir_ / utils::DEFAULT_COMMANDS)),
      cmd_processor_(event_bus_, cmd_loader_, cmd_history_),
      cmd_autocomplete_(cmd_loader_, file_history_, bookmark_manager_, global_marks_), document_(),
      renderer_(
          document_.cloneContext(),
          config_reader_.getConfig().recolor_dark,
          config_reader_.getConfig().recolor_light
      ),
      render_scheduler_(document_, renderer_, config_reader_.getConfig()), portal_(),
      link_opener_(portal_), file_printer_(portal_),
      password_dialog_textbox_(font_library_, glyph_atlas_, utils::TEXT_SIZE, ""),
      external_link_textbox_(font_library_, glyph_atlas_, utils::TEXT_SIZE, ""),
      cmdline_textbox_(font_library_, glyph_atlas_, utils::TEXT_SIZE, ""),
      cmdline_completions_(font_library_, glyph_atlas_, font_regular_, font_italic_, utils::TEXT_SIZE),
      external_link_dialog_(
          font_regular_, font_bold_, event_bus_, external_link_textbox_, config_reader_.getConfig()
      ),
      password_dialog_(font_bold_, event_bus_, password_dialog_textbox_, config_reader_.getConfig()),
      cmdline_(
          font_regular_,
          event_bus_,
          cmd_processor_,
          cmd_autocomplete_,
          cmd_history_,
          search_history_,
          cmdline_textbox_,
          cmdline_completions_,
          config_reader_.getConfig()
      ),
      document_view_(
          document_,
          file_history_,
          bookmark_manager_,
          global_marks_,
          render_scheduler_,
          event_bus_,
          config_reader_.getConfig()
      ),
      statusbar_(font_library_, glyph_atlas_, font_regular_, event_bus_, config_reader_.getConfig()),
      notifications_(
          font_library_,
          glyph_atlas_,
          font_regular_,
          font_bold_,
          event_bus_,
          config_reader_.getConfig()
      ),
      hand_cursor_(sf::Cursor::Type::Hand), arrow_cursor_(sf::Cursor::Type::Arrow),
      text_cursor_(sf::Cursor::Type::Text) {
  font_regular_.setSmooth(false);
  font_bold_.setSmooth(false);
  font_italic_.setSmooth(false);

  file_history_.setSaveUnique(true);

  initUI();
  registerEvents();
}

void App::markActive() {
  activity_clock_.restart();
}

bool App::needsContinuousFrames() const {
  if (activity_clock_.getElapsedTime() < sf::milliseconds(active_grace_ms_))
    return true;
  return false;
}

void App::run() {
  bool first_frame = true;

  while (window_.isOpen()) {
    const bool busy = needsContinuousFrames();
    const bool had_input = processEvents(busy ? sf::Time::Zero : sf::milliseconds(idle_wait_ms_));

    if (!window_.isOpen())
      break;

    if (pending_open_) {
      markActive();
      document_view_.onOpenDocument(*pending_open_);
      pending_open_.reset();
    }

    if (document_.needsAuth() && focus_ != ui::UIElements::PasswordDialog)
      event_bus_.emit("ui.focus", ui::UIElements::PasswordDialog);

    event_bus_.update();
    document_view_.update();
    cmdline_.update();
    statusbar_.update();
    notifications_.update();
    password_dialog_.update();
    external_link_dialog_.update();

    const bool heartbeat = idle_redraw_clock_.getElapsedTime() >= sf::milliseconds(idle_redraw_ms_);

    if (!(first_frame || had_input || busy || heartbeat))
      continue;

    first_frame = false;
    idle_redraw_clock_.restart();

    window_.clear(app_bg_color_);
    window_.setView(view_);

    document_view_.draw(window_);
    cmdline_.draw(window_);
    statusbar_.draw(window_);
    notifications_.draw(window_);
    password_dialog_.draw(window_);
    external_link_dialog_.draw(window_);

    window_.display();
  }
}

void App::openDocument(std::string path) {
  pending_open_ = path;
  markActive();
}

void App::registerEvents() {
  event_bus_.subscribe<bool>("cmd.clear_files", [this](bool) { file_history_.clear(); });
  event_bus_.subscribe<bool>("cmd.clear_search", [this](bool) { search_history_.clear(); });
  event_bus_.subscribe<bool>("cmd.clear_commands", [this](bool) { cmd_history_.clear(); });

  event_bus_.subscribe<bool>("cmd.quit", [this](bool close) {
    render_scheduler_.quiesce();
    render_scheduler_.clearCache();
    renderer_.clearCache();
    document_.closeDocument();
    window_.close();
  });

  event_bus_.subscribe<const std::string &>("cmd.write", [this](const std::string &filepath) {
    if (!document_.isOpen()) {
      event_bus_.emit("notification.msg", std::string("No document currently open"));
      return;
    }
    if (!document_.hasChanges())
      return;
    event_bus_.emit("statusbar.document_edited", false);

    try {
      if (filepath == "%") {
        document_.saveChanges();
        event_bus_
            .emit("notification.msg", std::string("Document saved in path: ") + document_.getFilePath());
      } else {
        document_.saveChanges(filepath);
        event_bus_.emit("notification.msg", std::string("Document saved in path: ") + filepath);
      }
    } catch (const std::runtime_error &e) {
      event_bus_.emit("notification.msg", std::string(e.what()));
    }
  });

  event_bus_.subscribe<ui::CursorType>("ui.cursor_type", [this](ui::CursorType type) {
    if (type == ui::CursorType::Arrow)
      window_.setMouseCursor(arrow_cursor_);
    else if (type == ui::CursorType::Text)
      window_.setMouseCursor(text_cursor_);
    else if (type == ui::CursorType::Hand)
      window_.setMouseCursor(hand_cursor_);
  });

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    focus_ = focus;
  });

  event_bus_.subscribe<bool>("dbus.print_file", [this](bool) {
    if (!document_.isOpen()) {
      event_bus_.emit("notification.msg", std::string("No document open to print"));
      return;
    }
    if (document_.needsAuth()) {
      event_bus_.emit("notification.msg", std::string("Unlock the document before printing it"));
      return;
    }

    switch (file_printer_.printFile(document_.getFilePath())) {
    case dbus::FilePrinter::PrintStatus::Started:
      break;
    case dbus::FilePrinter::PrintStatus::Busy:
      event_bus_.emit("notification.msg", std::string("A print is already in progress"));
      break;
    case dbus::FilePrinter::PrintStatus::Unavailable:
      event_bus_.emit("notification.msg", std::string("File printing is not available"));
      break;
    }
  });

  event_bus_.subscribe<const std::string &>("dbus.open_link", [this](const std::string &url) {
    if (link_opener_.openLink(url))
      return;

    clip::set_text(url);
    event_bus_.emit("notification.msg", std::string("External link url copied to clipboard"));
  });
}

fs::path App::prepareState(const fs::path &assets) {
  fs::path dir = utils::stateDir();
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir;
}

void App::initUI() {
  sf::ContextSettings settings;
  settings.antiAliasingLevel = 8;

  window_ = sf::
      RenderWindow(sf::VideoMode({res_x_, res_y_}), "Vim PDF Reader", (sf::Style::Resize + sf::Style::Close), sf::State::Windowed, settings);
  focus_ = ui::UIElements::PDFView;

  window_.setFramerateLimit(fps_);
  window_.setKeyRepeatEnabled(true);
  app_bg_color_ = utils::hexToRGB(config_reader_.getConfig().app_background_color);

  view_ = window_.getDefaultView();

  cmdline_.onResize(view_.getSize());
  statusbar_.onResize(view_.getSize());
  document_view_.onResize(view_.getSize());
  password_dialog_.onResize(view_.getSize());
  external_link_dialog_.onResize(view_.getSize());
  window_.setMouseCursor(arrow_cursor_);
}

bool App::processEvents(sf::Time wait) {
  bool any = false;
  auto event = wait > sf::Time::Zero ? window_.waitEvent(wait) : window_.pollEvent();
  while (event) {
    any = true;
    handleEvent(*event);
    event = window_.pollEvent();
  }

  if (any)
    markActive();

  return any;
}

void App::handleEvent(const sf::Event &event) {
  if (event.is<sf::Event::Closed>())
    window_.close();

  if (event.is<sf::Event::Resized>()) {
    auto window_size = window_.getSize();
    view_.setSize({(float)window_size.x, (float)window_size.y});
    auto view_size = view_.getSize();
    view_.setCenter({view_size.x / 2, view_size.y / 2});

    password_dialog_.onResize(view_.getSize());
    cmdline_.onResize(view_.getSize());
    statusbar_.onResize(view_.getSize());
    document_view_.onResize(view_.getSize());
    notifications_.onResize(view_.getSize());
    external_link_dialog_.onResize(view_.getSize());
  }

  if (focus_ == ui::UIElements::PDFView) {
    if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
      if (key->code == sf::Keyboard::Key::Semicolon && key->shift &&
          focus_ != ui::UIElements::Cmdline) {
        event_bus_.emit("ui.focus", ui::UIElements::Cmdline);
        cmdline_.setMode(ui::CmdlineMode::Cmd);
      }
      if (key->code == sf::Keyboard::Key::Slash && focus_ != ui::UIElements::Cmdline) {
        event_bus_.emit("ui.focus", ui::UIElements::Cmdline);
        cmdline_.setMode(ui::CmdlineMode::Search);
      }
    }
  }

  cmdline_.handleEvent(event);
  statusbar_.handleEvent(event);
  document_view_.handleEvent(event);
  notifications_.handleEvent(event);
  password_dialog_.handleEvent(event);
  external_link_dialog_.handleEvent(event);
}
