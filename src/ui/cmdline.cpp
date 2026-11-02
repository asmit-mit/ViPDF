#include <stdexcept>
#include <utf8.h>

#include "ui/cmdline.h"
#include "ui/ui_elements.h"
#include "utils/utils.h"

namespace ui {

Cmdline::Cmdline(
    const sf::Font &font_normal,
    core::EventBus &event_bus,
    core::CmdProcessor &cmd_processor,
    core::CmdAutocomplete &cmd_autocomplete,
    core::HistoryManager &cmd_history,
    core::HistoryManager &search_history,
    ui::Textbox &textbox,
    ui::Completions &completions,
    const core::Config &config

)
    : event_bus_(event_bus), cmd_processor_(cmd_processor), cmd_autocomplete_(cmd_autocomplete),
      cmd_history_(cmd_history), search_history_(search_history),
      label_(font_normal, ":", utils::TEXT_SIZE), textbox_(textbox), completions_(completions) {
  visible_ = false;

  textbox_.setCursorSize({2.f, 24.f});
  textbox_.setTextColor(utils::hexToRGB(config.cmdline_text_color));
  textbox_.setHighlightColor(utils::hexToRGB(config.cmdline_textbox_highlight_color));
  textbox_.setHighlightTextColor(utils::hexToRGB(config.cmdline_highlight_text_color));

  label_.setFillColor(utils::hexToRGB(config.cmdline_text_color));

  display_area_.setFillColor(utils::hexToRGB(config.cmdline_background_color));
  display_area_.setSize({200.f, utils::CMDLINE_HEIGHT});

  completions_.setBackgroundColor(utils::hexToRGB(config.completions_background_color));
  completions_.setLeftTextColor(utils::hexToRGB(config.completions_left_text_color));
  completions_.setRightTextColor(utils::hexToRGB(config.completions_right_text_color));
  completions_.setLeftHighlightColor(utils::hexToRGB(config.completions_highlight_color));
  completions_.setRightHighlightColor(utils::hexToRGB(config.completions_highlight_color));

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    visible_ = focus == ui::UIElements::Cmdline;
    if (visible_) {
      textbox_.reset();
      textbox_.startEditing();
      textbox_.show();
    }
  });

  registerKeybindings();
}

void Cmdline::draw(sf::RenderTarget &window) const {
  if (!visible_)
    return;

  window.draw(display_area_);
  textbox_.draw(window);
  completions_.draw(window);
  window.draw(label_);
}

void Cmdline::update() {
  if (!visible_)
    return;

  completions_.update();
  textbox_.update();
}

void Cmdline::handleEvent(const sf::Event &event) {
  if (!visible_)
    return;

  if (const auto *te = event.getIf<sf::Event::TextEntered>(); te && te->unicode == '\t')
    return;

  const bool cmd_mode = mode_ == CmdlineMode::Cmd;

  if (cmd_mode && completions_.isVisible() && shouldProcessEvent(event))
    hideCompletions();

  auto &input = cmd_mode ? cmd_input_ : search_input_;
  if (const auto cmd = input.handleEvent(event)) {
    const bool consumed = cmd_mode ? executeCmd(cmd->action) : executeSearch(cmd->action);
    if (consumed)
      return;
  }

  textbox_.handleEvent(event);
}

void Cmdline::setMode(CmdlineMode mode) {
  mode_ = mode;
  if (mode_ == CmdlineMode::Cmd)
    label_.setString(":");
  else if (mode_ == CmdlineMode::Search)
    label_.setString("/");
}

void Cmdline::onResize(const sf::Vector2f &size) {
  display_area_.setSize({size.x, utils::CMDLINE_HEIGHT});
  completions_.onResize(size);
  textbox_.setWidth(size.x);

  const float curr_x = 0.f;
  const float curr_y = size.y - utils::CMDLINE_HEIGHT;
  float round_y = std::round(curr_y) + 1.f;

  display_area_.setPosition({curr_x, round_y});
  label_.setPosition({utils::PADDING - 4.f, round_y});

  const auto bounds = label_.getGlobalBounds();
  textbox_.setPosition({bounds.position.x + bounds.size.x + 3.f, round_y});
}

void Cmdline::registerKeybindings() {
  using Key = sf::Keyboard::Key;

  for (auto *handler : {&cmd_input_, &search_input_}) {
    handler->bind({{Key::Escape}}, CmdlineAction::Cancel);
    handler->bind({{Key::Enter}}, CmdlineAction::Submit);
    handler->bind({{Key::P, true}}, CmdlineAction::HistoryPrev);
    handler->bind({{Key::N, true}}, CmdlineAction::HistoryNext);

    handler->bind({{Key::Up}}, CmdlineAction::HistoryPrev);
    handler->bind({{Key::Down}}, CmdlineAction::HistoryNext);
  }

  cmd_input_.bind({{Key::Tab}}, CmdlineAction::CompleteNext);
  cmd_input_.bind({{Key::Tab, false, true}}, CmdlineAction::CompletePrev);
  cmd_input_.bind({{Key::J, true}}, CmdlineAction::SelectNext);
  cmd_input_.bind({{Key::K, true}}, CmdlineAction::SelectPrev);
}

bool Cmdline::shouldProcessEvent(const sf::Event &event) {
  if (const auto *te = event.getIf<sf::Event::TextEntered>())
    return te->unicode >= 0x20 && te->unicode != 0x7F;
  if (const auto *key = event.getIf<sf::Event::KeyPressed>())
    return key->code == sf::Keyboard::Key::Backspace || key->code == sf::Keyboard::Key::Delete;
  return false;
}

bool Cmdline::executeCmd(CmdlineAction action) {
  switch (action) {
  case CmdlineAction::Cancel:
    if (completions_.isVisible()) {
      hideCompletions();
      textbox_.setText(original_string_);
      return true;
    }
    event_bus_.emit("ui.focus", ui::UIElements::PDFView);
    cmd_history_.reset();
    return false;

  case CmdlineAction::Submit:
    if (completions_.isVisible()) {
      textbox_.setText(completions_.getSelectedText());
      hideCompletions();
      return true;
    }
    cmd_history_.add(textbox_.getText());
    try {
      cmd_processor_.runCommand(textbox_.getText());
    } catch (const std::runtime_error &e) {
      event_bus_.emit("notification.msg", std::string(e.what()));
    }
    event_bus_.emit("ui.focus", ui::UIElements::PDFView);
    reset();
    return false;

  case CmdlineAction::HistoryPrev:
    textbox_.setText(cmd_history_.getPrevious());
    return false;
  case CmdlineAction::HistoryNext:
    textbox_.setText(cmd_history_.getNext());
    return false;

  case CmdlineAction::CompleteNext:
  case CmdlineAction::CompletePrev:
    if (!completions_.isVisible()) {
      original_string_ = textbox_.getText();
      refreshCompletions();
    } else {
      stepCompletions(action == CmdlineAction::CompleteNext);
    }
    return true;

  case CmdlineAction::SelectNext:
  case CmdlineAction::SelectPrev:
    if (completions_.isVisible())
      stepCompletions(action == CmdlineAction::SelectNext);
    return true;
  }
  return false;
}

bool Cmdline::executeSearch(CmdlineAction action) {
  switch (action) {
  case CmdlineAction::Cancel:
    event_bus_.emit("ui.focus", ui::UIElements::PDFView);
    break;
  case CmdlineAction::Submit:
    search_history_.add(textbox_.getText());
    event_bus_.emit("cmd.search", textbox_.getTextU32());
    event_bus_.emit("ui.focus", ui::UIElements::PDFView);
    break;
  case CmdlineAction::HistoryPrev:
    textbox_.setText(search_history_.getPrevious());
    break;
  case CmdlineAction::HistoryNext:
    textbox_.setText(search_history_.getNext());
    break;
  default:
    break;
  }
  return false;
}

void Cmdline::stepCompletions(bool down) {
  if (down)
    completions_.moveDown();
  else
    completions_.moveUp();
  textbox_.setText(completions_.getSelectedText());
}

void Cmdline::hideCompletions() {
  completions_.clear();
  completions_.hide();
}

void Cmdline::refreshCompletions() {
  auto list = cmd_autocomplete_.complete(original_string_);

  if (list.empty()) {
    completions_.clear();
    completions_.hide();
    return;
  }

  if (list.size() == 1) {
    textbox_.setText(list[0].cmd);
    completions_.clear();
    completions_.hide();
    return;
  }

  completions_.setCompletionList(std::move(list));
  completions_.show();
  textbox_.setText(completions_.getSelectedText());
}

void Cmdline::reset() {
  textbox_.reset();
  textbox_.stopEditing();
  textbox_.hide();
  completions_.clear();
  completions_.hide();
}

} // namespace ui
