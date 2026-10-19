#pragma once

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

namespace core {

struct KeyStroke {
  sf::Keyboard::Key code = sf::Keyboard::Key::Unknown;
  bool ctrl = false;
  bool shift = false;

  auto operator<=>(const KeyStroke &) const = default;
};

template <typename ActionT>
  requires std::is_enum_v<ActionT>
struct InputCmd {
  ActionT action;
  std::optional<std::size_t> count;
  std::optional<int> arg;

  std::size_t countOr(std::size_t fallback) const {
    return count.value_or(fallback);
  }
};

template <typename ActionT>
  requires std::is_enum_v<ActionT>
class InputHandler {
public:
  using Key = sf::Keyboard::Key;
  using CommandType = InputCmd<ActionT>;

  InputHandler() : root_(std::make_unique<Node>()), current_(root_.get()) {}

  void bind(
      const std::vector<KeyStroke> &sequence, ActionT action, std::optional<int> arg = std::nullopt
  ) {
    Node *node = root_.get();
    for (const KeyStroke &stroke : sequence) {
      auto &child = node->children[stroke];
      if (!child)
        child = std::make_unique<Node>();
      node = child.get();
    }
    node->action = action;
    node->arg = arg;
  }

  std::optional<CommandType> handleEvent(const sf::Event &event) {
    const auto *key = event.getIf<sf::Event::KeyPressed>();
    if (!key || isModifierKey(key->code))
      return std::nullopt;
    return handleKey(*key);
  }

  bool hasPending() const {
    return !pending_.empty() || count_.has_value();
  }

  std::string pendingText() const {
    std::string text;
    if (count_)
      text += std::to_string(*count_);
    for (const KeyStroke &stroke : pending_)
      text += strokeToString(stroke);
    return text;
  }

  void reset() {
    current_ = root_.get();
    pending_.clear();
    count_.reset();
  }

private:
  static constexpr std::size_t max_count_ = 1'000'000;

  struct Node {
    std::optional<ActionT> action;
    std::optional<int> arg;
    std::map<KeyStroke, std::unique_ptr<Node>> children;
  };

  std::optional<CommandType> handleKey(const sf::Event::KeyPressed &key) {
    if (key.code == Key::Escape && hasPending()) {
      const KeyStroke escStroke{key.code, key.control, key.shift};
      const Node *bound = findChild(*root_, escStroke);

      std::optional<CommandType> cmd;
      if (bound && bound->action)
        cmd = makeCommand(*bound);

      reset();
      return cmd;
    }

    const auto digit = digitFromKey(key.code);
    if (digit && !key.control && pending_.empty() && (*digit != 0 || count_)) {
      count_ = std::min(count_.value_or(0) * 10 + *digit, max_count_);
      return std::nullopt;
    }

    const KeyStroke stroke{key.code, key.control, key.shift};
    const Node *next = findChild(*current_, stroke);

    if (!next) {
      reset();
      return std::nullopt;
    }

    current_ = next;
    pending_.push_back(stroke);

    if (current_->children.empty()) {
      const CommandType cmd = makeCommand(*current_);
      reset();
      return cmd;
    }
    return std::nullopt;
  }

  const Node *findChild(const Node &node, const KeyStroke &stroke) const {
    auto it = node.children.find(stroke);

    if (it == node.children.end() && stroke.shift && digitFromKey(stroke.code)) {
      KeyStroke relaxed = stroke;
      relaxed.shift = false;
      it = node.children.find(relaxed);
    }

    return it == node.children.end() ? nullptr : it->second.get();
  }

  CommandType makeCommand(const Node &node) const {
    return CommandType{*node.action, count_, node.arg};
  }

  std::optional<std::size_t> digitFromKey(Key code) const {
    const int value = static_cast<int>(code);
    const int num0 = static_cast<int>(Key::Num0);
    const int num9 = static_cast<int>(Key::Num9);
    const int pad0 = static_cast<int>(Key::Numpad0);
    const int pad9 = static_cast<int>(Key::Numpad9);

    if (value >= num0 && value <= num9)
      return static_cast<std::size_t>(value - num0);
    if (value >= pad0 && value <= pad9)
      return static_cast<std::size_t>(value - pad0);
    return std::nullopt;
  }

  bool isModifierKey(Key code) const {
    switch (code) {
    case Key::LControl:
    case Key::RControl:
    case Key::LShift:
    case Key::RShift:
    case Key::LAlt:
    case Key::RAlt:
    case Key::LSystem:
    case Key::RSystem:
      return true;
    default:
      return false;
    }
  }

  std::string strokeToString(const KeyStroke &stroke) const {
    std::string text;
    if (stroke.ctrl)
      text += "C-";

    const int code = static_cast<int>(stroke.code);
    const int first_letter = static_cast<int>(Key::A);
    const int last_letter = static_cast<int>(Key::Z);

    if (code >= first_letter && code <= last_letter) {
      text += static_cast<char>((stroke.shift ? 'A' : 'a') + (code - first_letter));
    } else if (const auto digit = digitFromKey(stroke.code)) {
      text += std::to_string(*digit);
    } else {
      switch (stroke.code) {
      case Key::Equal:
        text += "=";
        break;
      case Key::Hyphen:
        text += "-";
        break;
      default:
        text += "?";
        break;
      }
    }
    return text;
  }

  std::unique_ptr<Node> root_;
  const Node *current_ = nullptr;
  std::vector<KeyStroke> pending_;
  std::optional<std::size_t> count_;
};

} // namespace core
