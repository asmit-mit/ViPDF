#include "app.h"

#include "utils/settings.h"
#include "utils/utils.h"

namespace fs = std::filesystem;

App::App()
    : state_dir_(
          utils::ensureParent(
              (std::getenv("HOME") ? fs::path(std::getenv("HOME")) : fs::current_path()) /
              settings::state_dir
          )
      ),
      font_regular_(utils::ensureParent(state_dir_ / settings::font_regular)),
      font_bold_(utils::ensureParent(state_dir_ / settings::font_bold)),
      font_italic_(utils::ensureParent(state_dir_ / settings::font_italic)),
      font_library_({
          {graphics::FontType::Regular,
           utils::ensureParent(state_dir_ / settings::font_regular).c_str()},
          {graphics::FontType::Bold, utils::ensureParent(state_dir_ / settings::font_bold).c_str()},
          {graphics::FontType::Italic,
           utils::ensureParent(state_dir_ / settings::font_italic).c_str()},
          {graphics::FontType::CJK, utils::ensureParent(state_dir_ / settings::font_cjk).c_str()},
          {graphics::FontType::Emoji,
           utils::ensureParent(state_dir_ / settings::font_emoji).c_str()},
      }),
      cmd_history_(utils::ensureParent(state_dir_ / settings::cmd_history)),
      search_history_(utils::ensureParent(state_dir_ / settings::search_history)),
      file_history_(utils::ensureParent(state_dir_ / settings::recent_files)),
      bookmark_manager_(utils::ensureParent(state_dir_ / settings::bookmarks_db)),
      cmd_loader_(utils::ensureParent(state_dir_ / settings::commands_json)),
      cmd_processor_(event_bus_, cmd_loader_, cmd_history_),
      cmd_autocomplete_(cmd_loader_, file_history_, bookmark_manager_), document_(), renderer_(),
      render_scheduler_(document_, renderer_, settings::thread_count_),
      cmdline_textbox_(font_library_, glyph_atlas_, utils::char_size, ":"),
      cmdline_completions_(font_library_, glyph_atlas_, font_regular_, font_italic_, utils::char_size),
      cmdline_(font_regular_, event_bus_, cmd_processor_, cmd_autocomplete_, cmd_history_, search_history_, cmdline_textbox_, cmdline_completions_),
      document_view_(file_history_, bookmark_manager_, document_, render_scheduler_, event_bus_),
      statusbar_(font_library_, glyph_atlas_, font_regular_, event_bus_),
      notifications_(font_library_, glyph_atlas_, font_regular_, font_bold_, event_bus_),
      arrow_cursor_(sf::Cursor::Type::Arrow), text_cursor_(sf::Cursor::Type::Text) {
  font_regular_.setSmooth(false);
  font_bold_.setSmooth(false);
  font_italic_.setSmooth(false);

  file_history_.setSaveUnique(true);

  initUI();
  registerEvents();
}

void App::run() {
  while (window_.isOpen()) {
    processEvents();

    event_bus_.update();
    document_view_.update();
    cmdline_.update();
    statusbar_.update();
    notifications_.update();

    window_.clear(utils::hexToRGB(settings::bg_));
    window_.setView(view_);

    document_view_.draw(window_);
    cmdline_.draw(window_);
    statusbar_.draw(window_);
    notifications_.draw(window_);

    window_.display();
  }
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
  });

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    focus_ = focus;
  });
}

void App::initUI() {
  sf::ContextSettings settings;
  settings.antiAliasingLevel = 8;

  window_ = sf::
      RenderWindow(sf::VideoMode({res_x_, res_y_}), "Vim PDF Reader", (sf::Style::Resize + sf::Style::Close), sf::State::Windowed, settings);
  focus_ = ui::UIElements::PDFView;

  window_.setFramerateLimit(fps_);
  window_.setKeyRepeatEnabled(true);

  view_ = window_.getDefaultView();

  cmdline_.onResize(view_.getSize());
  statusbar_.onResize(view_.getSize());
  document_view_.onResize(view_.getSize());
  window_.setMouseCursor(arrow_cursor_);
}

void App::processEvents() {
  while (const auto event = window_.pollEvent()) {
    if (event->is<sf::Event::Closed>())
      window_.close();

    if (event->is<sf::Event::Resized>()) {
      auto window_size = window_.getSize();
      view_.setSize({(float)window_size.x, (float)window_size.y});
      auto view_size = view_.getSize();
      view_.setCenter({view_size.x / 2, view_size.y / 2});

      cmdline_.onResize(view_.getSize());
      statusbar_.onResize(view_.getSize());
      document_view_.onResize(view_.getSize());
      notifications_.onResize(view_.getSize());
    }

    if (focus_ == ui::UIElements::PDFView) {
      if (const auto *key = event->getIf<sf::Event::KeyPressed>()) {
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

    cmdline_.handleEvent(*event);
    statusbar_.handleEvent(*event);
    document_view_.handleEvent(*event);
    notifications_.handleEvent(*event);
  }
}
