#include <stdexcept>
#include <utf8.h>

#include "ui/cmdline.h"
#include "ui/ui_elements.h"
#include "utils/settings.h"
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
    ui::Completions &completions
)
    : event_bus_(event_bus), cmd_processor_(cmd_processor), cmd_autocomplete_(cmd_autocomplete),
      cmd_history_(cmd_history), search_history_(search_history), font_(font_normal),
      label_(font_normal, ":", utils::char_size), textbox_(textbox), completions_(completions) {
  visible_ = false;

  textbox_.setCursorSize({2.f, 24.f});

  label_.setFillColor(utils::hexToRGB(settings::fg_));

  display_area_.setFillColor(utils::hexToRGB(settings::cmd_bg_));
  display_area_.setSize({200.f, utils::cmdline_height_});

  completions_.setCmdColor(utils::hexToRGB(settings::fg_));
  completions_.setDescColor(utils::hexToRGB(settings::completions_desc_fg_));

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    visible_ = focus == ui::UIElements::Cmdline;
    if (visible_) {
      textbox_.reset();
      textbox_.startEditing();
      textbox_.show();
    }
  });
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

  if (const auto *te = event.getIf<sf::Event::TextEntered>()) {
    if (te->unicode == '\t')
      return;
  }

  const auto *key = event.getIf<sf::Event::KeyPressed>();

  if (mode_ == CmdlineMode::Cmd) {
    if (completions_.isVisible()) {
      bool is_edit = false;
      if (const auto *te = event.getIf<sf::Event::TextEntered>()) {
        // Ignore control characters (< 0x20) — Ctrl+J sends \n, Ctrl+K sends \v
        is_edit = te->unicode >= 0x20 && te->unicode != 0x7F;
      }
      if (!is_edit && key)
        is_edit = key->code == sf::Keyboard::Key::Backspace ||
                  key->code == sf::Keyboard::Key::Delete;
      if (is_edit) {
        completions_.clear();
        completions_.hide();
      }
    }

    if (key) {
      if (key->code == sf::Keyboard::Key::Escape) {
        if (completions_.isVisible()) {
          completions_.clear();
          completions_.hide();
          textbox_.setText(original_string_);
          return;
        }
        event_bus_.emit("ui.focus", ui::UIElements::PDFView);
        cmd_history_.reset();
      } else if (key->code == sf::Keyboard::Key::Enter) {
        cmd_history_.add(textbox_.getText());
        try {
          cmd_processor_.runCommand(textbox_.getText());
        } catch (const std::runtime_error &e) {
          event_bus_.emit("notification.msg", std::string(e.what()));
        }
        event_bus_.emit("ui.focus", ui::UIElements::PDFView);
        reset();
      } else if (key->code == sf::Keyboard::Key::P && key->control) {
        textbox_.setText(cmd_history_.getPrevious());
      } else if (key->code == sf::Keyboard::Key::N && key->control) {
        textbox_.setText(cmd_history_.getNext());
      } else if (key->code == sf::Keyboard::Key::Tab) {
        original_string_ = textbox_.getText();
        refreshCompletions();
        return;
      } else if (key->code == sf::Keyboard::Key::J && key->control) {
        if (completions_.isVisible()) {
          completions_.moveDown();
          textbox_.setText(completions_.getSelectedText());
        }
        return;
      } else if (key->code == sf::Keyboard::Key::K && key->control) {
        if (completions_.isVisible()) {
          completions_.moveUp();
          textbox_.setText(completions_.getSelectedText());
        }
        return;
      }
    }
  } else {
    if (key) {
      if (key->code == sf::Keyboard::Key::Escape) {
        event_bus_.emit("ui.focus", ui::UIElements::PDFView);
      } else if (key->code == sf::Keyboard::Key::Enter) {
        search_history_.add(textbox_.getText());
        event_bus_.emit("cmd.search", textbox_.getTextU32());
        event_bus_.emit("ui.focus", ui::UIElements::PDFView);
      } else if (key->code == sf::Keyboard::Key::P && key->control) {
        textbox_.setText(search_history_.getPrevious());
      } else if (key->code == sf::Keyboard::Key::N && key->control) {
        textbox_.setText(search_history_.getNext());
      }    
    }
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
  window_size_ = size;
  display_area_.setSize({size.x, utils::cmdline_height_});
  completions_.onResize(size);

  const float curr_x_ = 0.f;
  const float curr_y_ = window_size_.y - utils::cmdline_height_;
  float round_y = std::round(curr_y_) + 1.f;

  display_area_.setPosition({curr_x_, round_y});
  label_.setPosition({utils::padding - 4.f, round_y});

  const auto bounds = label_.getGlobalBounds();
  textbox_.setPosition({bounds.position.x + bounds.size.x + 3.f, round_y});
}

void Cmdline::reset() {
  textbox_.reset();
  textbox_.stopEditing();
  textbox_.hide();
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

} // namespace ui
