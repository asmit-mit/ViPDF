#include "app.h"
#include "graphics/font_library.h"
#include "ui/ui_elements.h"
#include "utils/settings.h"
#include "utils/utils.h"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

App::App()
    : document_(), renderer_(),
      cmd_loader_(fs::path(std::getenv("HOME")) / settings::state_dir / settings::commands_json),
      cmd_processor_(event_bus_, cmd_loader_, cmd_history_),
      cmd_autocomplete_(cmd_loader_, file_history_),
      render_scheduler_(document_, renderer_, settings::thread_count_),
      cmdline_textbox_(font_library_, glyph_atlas_, utils::char_size, ":"),
      cmdline_completions_(font_library_, glyph_atlas_, font_regular_, font_italic_, utils::char_size),
      cmdline_(font_regular_, event_bus_, cmd_processor_, cmd_autocomplete_, cmd_history_, search_history_, cmdline_textbox_, cmdline_completions_),
      pdf_view_(file_history_, document_, render_scheduler_, event_bus_),
      statusbar_(font_library_, glyph_atlas_, font_regular_, event_bus_),
      notifications_(font_library_, glyph_atlas_, font_regular_, font_bold_, notification_history_, event_bus_),
      arrow_cursor_(sf::Cursor::Type::Arrow), text_cursor_(sf::Cursor::Type::Text) {
  const char *home = std::getenv("HOME");
  fs::path base = home ? fs::path(home) : fs::current_path();
  fs::path state_dir = base / settings::state_dir;

  std::error_code ec;
  fs::create_directories(state_dir, ec);
  if (ec)
    std::cerr << "Cannot create " << state_dir << ": " << ec.message() << '\n';

  initFonts(state_dir);
  initHistory(state_dir);
  initWindow();
  initUI();

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

void App::run() {
  while (window_.isOpen()) {
    processEvents();

    event_bus_.update();
    pdf_view_.update();
    cmdline_.update();
    statusbar_.update();
    notifications_.update();

    window_.clear(utils::hexToRGB(settings::bg_));
    window_.setView(view_);

    pdf_view_.draw(window_);
    cmdline_.draw(window_);
    statusbar_.draw(window_);
    notifications_.draw(window_);

    window_.display();
  }
}

void App::initHistory(const std::filesystem::path &state_dir) {
  fs::path history_dir = state_dir / "history";

  std::error_code ec;
  fs::create_directories(history_dir, ec);
  if (ec)
    std::cerr << "Cannot create " << history_dir << ": " << ec.message() << '\n';

  cmd_history_.setPath((state_dir / settings::cmd_history).string());
  file_history_.setPath((state_dir / settings::recent_files).string());
  search_history_.setPath((state_dir / settings::search_history).string());

  file_history_.setSaveUnique(true);

  event_bus_.subscribe<bool>("cmd.clear_files", [this](bool) { file_history_.clear(); });
  event_bus_.subscribe<bool>("cmd.clear_search", [this](bool) { search_history_.clear(); });
  event_bus_.subscribe<bool>("cmd.clear_commands", [this](bool) { cmd_history_.clear(); });
}

void App::initWindow() {
  sf::ContextSettings settings;
  settings.antiAliasingLevel = 8;

  window_ = sf::
      RenderWindow(sf::VideoMode({res_x_, res_y_}), "Vim PDF Reader", (sf::Style::Resize + sf::Style::Close), sf::State::Windowed, settings);
  focus_ = ui::UIElements::PDFView;

  window_.setFramerateLimit(fps_);
  window_.setKeyRepeatEnabled(true);

  view_ = window_.getDefaultView();
}

void App::initUI() {
  cmdline_.onResize(view_.getSize());
  statusbar_.onResize(view_.getSize());
  pdf_view_.onResize(view_.getSize());
  window_.setMouseCursor(arrow_cursor_);
}

void App::initFonts(const std::filesystem::path &state_dir) {
  fs::path font_path_regular = state_dir / settings::font_regular;
  fs::path font_path_bold = state_dir / settings::font_bold;
  fs::path font_path_italic = state_dir / settings::font_italic;
  fs::path font_path_emoji = state_dir / settings::font_emoji;
  fs::path font_path_cjk = state_dir / settings::font_cjk;

  font_library_.tryLoadFont(graphics::FontType::Regular, font_path_regular.string().data());
  font_library_.tryLoadFont(graphics::FontType::Bold, font_path_bold.string().data());
  font_library_.tryLoadFont(graphics::FontType::Italic, font_path_italic.string().data());
  font_library_.tryLoadFont(graphics::FontType::Emoji, font_path_emoji.string().data());
  font_library_.tryLoadFont(graphics::FontType::CJK, font_path_cjk.string().data());

  if (!font_regular_.openFromFile(font_path_regular))
    throw std::runtime_error("Can't load font: " + font_path_regular.string());
  if (!font_bold_.openFromFile(font_path_bold))
    throw std::runtime_error("Can't load font: " + font_path_bold.string());
  if (!font_italic_.openFromFile(font_path_italic))
    throw std::runtime_error("Can't load font: " + font_path_italic.string());

  font_regular_.setSmooth(false);
  font_bold_.setSmooth(false);
  font_italic_.setSmooth(false);
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
      pdf_view_.onResize(view_.getSize());
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
    pdf_view_.handleEvent(*event);
    notifications_.handleEvent(*event);
  }
}
