#pragma once

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace core {

enum class Action {
  PanLeft,
  PanRight,
  PanUp,
  PanDown,
  HalfPageUp,
  HalfPageDown,
  PrevPage,
  NextPage,
  FirstPage,
  LastPage,
  ZoomIn,
  ZoomOut,
  FitPage,
  FitWidth,
  RotateCW,
  RotateCCW,
  Recolor,
  SearchNext,
  SearchPrev,
  Highlight,
  Cancel
};

struct DocumentCmd {
  Action action;
  std::optional<std::size_t> count;
  std::optional<int> arg;

  std::size_t countOr(std::size_t fallback) const {
    return count.value_or(fallback);
  }
};

struct KeyStroke {
  sf::Keyboard::Key code = sf::Keyboard::Key::Unknown;
  bool ctrl = false;
  bool shift = false;

  auto operator<=>(const KeyStroke &) const = default;
};

class InputHandler {
public:
  InputHandler();

  std::optional<DocumentCmd> handleEvent(const sf::Event &event);

  bool hasPending() const {
    return !pending_.empty() || count_.has_value();
  }
  std::string pendingText() const;
  void reset();

private:
  struct Node {
    std::optional<Action> action;
    std::optional<int> arg;
    std::map<KeyStroke, std::unique_ptr<Node>> children;
  };

  void
  bind(const std::vector<KeyStroke> &sequence, Action action, std::optional<int> arg = std::nullopt);
  void registerDefaultBindings();

  std::optional<DocumentCmd> handleKey(const sf::Event::KeyPressed &key);
  const Node *findChild(const Node &node, const KeyStroke &stroke) const;
  DocumentCmd makeCommand(const Node &node) const;

  std::optional<std::size_t> digitFromKey(sf::Keyboard::Key code) const;
  bool isModifierKey(sf::Keyboard::Key code) const;
  std::string strokeToString(const KeyStroke &stroke) const;

  std::unique_ptr<Node> root_;
  const Node *current_ = nullptr;
  std::vector<KeyStroke> pending_;
  std::optional<std::size_t> count_;
};

} // namespace core
