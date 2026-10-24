#include "ui/statusbar.h"

#include "ui/ui_elements.h"
#include "utils/settings.h"
#include "utils/utils.h"
#include <cmath>
#include <utf8.h>

namespace ui {

Statusbar::Statusbar(
    graphics::FontLibrary &font_lib,
    graphics::GlyphAtlas &glyph_atlas,
    const sf::Font &font,
    core::EventBus &event_bus
)
    : font_(font), event_bus_(event_bus),
      display_filepath_(font_lib, glyph_atlas, settings::status_font_size_),
      display_search_state_(font_, "[]", settings::status_font_size_),
      display_page_state_(font_, "[]", settings::status_font_size_),
      display_zoom_(font_, "[]", settings::status_font_size_),
      display_pending_keys_(font_, "", settings::status_font_size_),
      display_modified_(font_, "[+]", settings::status_font_size_) {
  display_area_.setSize({200.0, utils::cmdline_height_});

  const auto fg = utils::hexToRGB(settings::fg_);

  filepath_ = U"[No document open]";
  last_available_width_ = -1.f;
  max_filepath_width_ = 0.f;

  search_num_ = 0;
  total_search_results_ = 0;
  search_details_changed_ = false;

  page_num_ = 0;
  total_pages_ = 0;
  page_details_changed_ = false;

  document_edited_ = false;

  move_up_ = false;
  curr_x_ = 0.f;
  curr_y_ = 0.f;

  display_area_.setFillColor(utils::hexToRGB(settings::status_bg_));
  display_filepath_.setFillColor(fg);
  display_search_state_.setFillColor(fg);
  display_page_state_.setFillColor(fg);
  display_zoom_.setFillColor(fg);
  display_pending_keys_.setFillColor(fg);
  display_modified_.setFillColor(fg);

  setFilepath(filepath_);
  subscribeToEvents();
}

void Statusbar::subscribeToEvents() {
  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    move_up_ = focus == ui::UIElements::Cmdline;
  });

  event_bus_.subscribe<std::string>("statusbar.pdf_path", [this](const std::string &filepath) {
    setFilepath(utf8::utf8to32(filepath));
  });

  event_bus_.subscribe<
      std::pair<size_t, size_t>>("statusbar.search_state", [this](std::pair<size_t, size_t> state) {
    search_num_ = state.first;
    total_search_results_ = state.second;
    search_details_changed_ = true;
  });

  event_bus_.subscribe<
      std::pair<size_t, size_t>>("statusbar.page_state", [this](std::pair<size_t, size_t> state) {
    page_num_ = state.first;
    total_pages_ = state.second;
    page_details_changed_ = true;
  });

  event_bus_.subscribe<float>("statusbar.page_zoom", [this](float zoom) {
    display_zoom_.setString("[" + std::to_string(static_cast<int>(zoom * 100)) + "%]");
  });

  event_bus_.subscribe<std::string>("statusbar.pending_keys", [this](std::string pending_keys) {
    display_pending_keys_.setString(pending_keys);
  });

  event_bus_.subscribe<bool>("statusbar.document_edited", [this](bool edited) {
    document_edited_ = edited;
  });
}

void Statusbar::draw(sf::RenderTarget &window) const {
  window.draw(display_area_);
  window.draw(display_filepath_);

  if (shouldShowPendingKeys())
    window.draw(display_pending_keys_);

  if (shouldShowPageInfo()) {
    window.draw(display_page_state_);
    window.draw(display_zoom_);
  }

  if (shouldShowSearch())
    window.draw(display_search_state_);

  if (shouldShowModified())
    window.draw(display_modified_);
}

void Statusbar::update() {
  refreshTexts();

  float y = std::round(curr_y_);
  if (move_up_)
    y = (y - utils::cmdline_height_) + 1.f;
  display_area_.setPosition({curr_x_, y});

  const float filepath_h = display_filepath_.getSize().y;
  const float text_y = std::round(y + (utils::cmdline_height_ - filepath_h) / 2.f) - 3.f;

  layoutRightWidgets(text_y);

  fitFilepath(max_filepath_width_);
  display_filepath_.setPosition({utils::padding, text_y});

  if (shouldShowModified()) {
    const float x = utils::padding + display_filepath_.getSize().x + utils::padding;
    display_modified_.setPosition({std::round(x), text_y + 1.f});
  }
}

bool Statusbar::shouldShowPageInfo() const {
  return total_pages_ != 0;
}

bool Statusbar::shouldShowSearch() const {
  return total_search_results_ != 0;
}

bool Statusbar::shouldShowPendingKeys() const {
  return !display_pending_keys_.getString().isEmpty();
}

bool Statusbar::shouldShowModified() const {
  return document_edited_;
}

void Statusbar::refreshTexts() {
  if (page_details_changed_) {
    display_page_state_.setString(
        "[" + std::to_string(page_num_) + "/" + std::to_string(total_pages_) + "]"
    );
    page_details_changed_ = false;
  }

  if (search_details_changed_) {
    display_search_state_.setString(
        "[Search " + std::to_string(search_num_) + "/" + std::to_string(total_search_results_) + "]"
    );
    search_details_changed_ = false;
  }
}

float Statusbar::placeLeftOf(sf::Text &text, float right_edge, float y) {
  const float x = right_edge - text.getLocalBounds().size.x - utils::padding;
  text.setPosition({std::round(x), y});
  return x;
}

void Statusbar::layoutRightWidgets(float y) {
  const float widget_y = y + 1.f;
  float edge = display_area_.getSize().x;

  if (shouldShowSearch())
    edge = placeLeftOf(display_search_state_, edge, widget_y);

  if (shouldShowPageInfo()) {
    edge = placeLeftOf(display_page_state_, edge, widget_y);
    edge = placeLeftOf(display_zoom_, edge, widget_y);
  }

  if (shouldShowPendingKeys())
    edge = placeLeftOf(display_pending_keys_, edge, widget_y);

  max_filepath_width_ = edge - utils::padding * 2.f;
  if (shouldShowModified())
    max_filepath_width_ -= display_modified_.getLocalBounds().size.x + utils::padding;
}

void Statusbar::setFilepath(std::u32string filepath) {
  filepath_ = std::move(filepath);
  last_truncated_ = filepath_;
  last_available_width_ = -1.f;
  display_filepath_.setString(filepath_);
}

void Statusbar::fitFilepath(float max_width) {
  if (max_width == last_available_width_)
    return;
  last_available_width_ = max_width;

  display_filepath_.setString(filepath_);
  if (display_filepath_.getSize().x <= max_width) {
    last_truncated_ = filepath_;
  } else {
    const std::u32string ellipsis(3, U'.');
    int lo = 0, hi = static_cast<int>(filepath_.size());
    while (lo < hi) {
      const int mid = (lo + hi + 1) / 2;
      display_filepath_.setString(filepath_.substr(0, mid) + ellipsis);
      if (display_filepath_.getSize().x <= max_width)
        lo = mid;
      else
        hi = mid - 1;
    }
    last_truncated_ = filepath_.substr(0, lo) + ellipsis;
  }

  display_filepath_.setString(last_truncated_);
}

void Statusbar::onResize(const sf::Vector2f &size) {
  curr_x_ = 0;
  curr_y_ = size.y - utils::cmdline_height_;
  display_area_.setSize({size.x, utils::cmdline_height_});
}

} // namespace ui
