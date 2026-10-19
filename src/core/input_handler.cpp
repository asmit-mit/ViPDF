#include "core/input_handler.h"

#include <algorithm>

namespace core {

using Key = sf::Keyboard::Key;

namespace {
constexpr std::size_t max_count = 1'000'000;
}

InputHandler::InputHandler() : root_(std::make_unique<Node>()), current_(root_.get()) {
  registerDefaultBindings();
}

void InputHandler::registerDefaultBindings() {
  bind({{Key::H}}, Action::PanLeft);
  bind({{Key::L}}, Action::PanRight);
  bind({{Key::K}}, Action::PanUp);
  bind({{Key::J}}, Action::PanDown);

  bind({{Key::U}}, Action::PrevPage);
  bind({{Key::D}}, Action::NextPage);
  bind({{Key::U, true}}, Action::HalfPageUp);
  bind({{Key::D, true}}, Action::HalfPageDown);
  bind({{Key::G}, {Key::G}}, Action::FirstPage);
  bind({{Key::G, false, true}}, Action::LastPage);

  bind({{Key::Equal, true}}, Action::ZoomIn);
  bind({{Key::Hyphen, true}}, Action::ZoomOut);
  bind({{Key::F}}, Action::FitPage);
  bind({{Key::W}}, Action::FitWidth);

  bind({{Key::R}}, Action::RotateCW);
  bind({{Key::R, false, true}}, Action::RotateCCW);
  bind({{Key::I}}, Action::Recolor);

  bind({{Key::N}}, Action::SearchNext);
  bind({{Key::N, false, true}}, Action::SearchPrev);

  for (int i = 0; i < 4; ++i) {
    const auto digit = static_cast<Key>(static_cast<int>(Key::Num1) + i);
    bind({{Key::H, false, true}, {digit}}, Action::Highlight, i);
  }
}

void InputHandler::bind(const std::vector<KeyStroke> &sequence, Action action, std::optional<int> arg) {
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

std::optional<DocumentCmd> InputHandler::handleEvent(const sf::Event &event) {
  const auto *key = event.getIf<sf::Event::KeyPressed>();
  if (!key || isModifierKey(key->code))
    return std::nullopt;
  return handleKey(*key);
}

std::optional<DocumentCmd> InputHandler::handleKey(const sf::Event::KeyPressed &key) {
  if (key.code == Key::Escape) {
    reset();
    return DocumentCmd{Action::Cancel};
  }

  const auto digit = digitFromKey(key.code);
  if (digit && !key.control && pending_.empty() && (*digit != 0 || count_)) {
    count_ = std::min(count_.value_or(0) * 10 + *digit, max_count);
    return std::nullopt;
  }

  const KeyStroke stroke{key.code, key.control, key.shift};
  const Node *next = findChild(*current_, stroke);

  if (!next) { // dead end: cancel
    reset();
    return std::nullopt;
  }

  current_ = next;
  pending_.push_back(stroke);

  if (current_->children.empty()) {
    const DocumentCmd cmd = makeCommand(*current_);
    reset();
    return cmd;
  }
  return std::nullopt;
}

const InputHandler::Node *InputHandler::findChild(const Node &node, const KeyStroke &stroke) const {
  auto it = node.children.find(stroke);

  if (it == node.children.end() && stroke.shift && digitFromKey(stroke.code)) {
    KeyStroke relaxed = stroke;
    relaxed.shift = false;
    it = node.children.find(relaxed);
  }

  return it == node.children.end() ? nullptr : it->second.get();
}

DocumentCmd InputHandler::makeCommand(const Node &node) const {
  return DocumentCmd{*node.action, count_, node.arg};
}

void InputHandler::reset() {
  current_ = root_.get();
  pending_.clear();
  count_.reset();
}

std::string InputHandler::pendingText() const {
  std::string text;
  if (count_)
    text += std::to_string(*count_);
  for (const KeyStroke &stroke : pending_)
    text += strokeToString(stroke);
  return text;
}

std::string InputHandler::strokeToString(const KeyStroke &stroke) const {
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
    case Key::Equal:  text += "="; break;
    case Key::Hyphen: text += "-"; break;
    default:          text += "?"; break;
    }
  }
  return text;
}

std::optional<std::size_t> InputHandler::digitFromKey(Key code) const {
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

bool InputHandler::isModifierKey(Key code) const {
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

} // namespace core
