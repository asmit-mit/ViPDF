#include "ui/statusbar.h"

#include "ui/ui_elements.h"
#include "utf8.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace ui {

Statusbar::Statusbar(
    graphics::FontLibrary &font_lib,
    graphics::GlyphAtlas &glyph_atlas,
    const sf::Font &font,
    core::EventBus &event_bus
)
    : font_(font), display_filepath_(font_lib, glyph_atlas, 16),
      display_search_state_(font, "[]", 16), display_page_state_(font_, "[]", 16),
      display_zoom_(font_, "[]", 16), event_bus_(event_bus) {
  display_area_.setFillColor(utils::hexToRGB(settings::status_bg_));
  display_area_.setSize({200.0, utils::cmdline_height_});

  display_filepath_.setFillColor(utils::hexToRGB(settings::fg_));
  display_search_state_.setFillColor(utils::hexToRGB(settings::fg_));
  display_page_state_.setFillColor(utils::hexToRGB(settings::fg_));
  display_zoom_.setFillColor(utils::hexToRGB(settings::fg_));

  filepath_ = U"[No document open]";
  last_truncated_ = filepath_;
  last_available_width_ = -1.f;
  display_filepath_.setString(filepath_);

  search_num_ = 0;
  total_search_results_ = 0;
  search_details_changed_ = false;

  page_num_ = 0;
  total_pages_ = 0;
  page_details_changed_ = false;

  move_up_ = false;

  curr_x_ = 0.f;
  curr_y_ = 0.f;

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    move_up_ = focus == ui::UIElements::Cmdline;
  });

  event_bus_.subscribe<std::string>("statusbar.pdf_path", [this](const std::string &filepath) {
    filepath_ = utf8::utf8to32(filepath);
    last_truncated_ = filepath_;
    last_available_width_ = -1.f;
    display_filepath_.setString(filepath_);
  });

  event_bus_.subscribe<std::pair<
      size_t,
      size_t>>("statusbar.search_state", [this](std::pair<size_t, size_t> search_state) {
    search_num_ = search_state.first;
    total_search_results_ = search_state.second;
    search_details_changed_ = true;
  });

  event_bus_.subscribe<
      std::pair<size_t, size_t>>("statusbar.page_state", [this](std::pair<size_t, size_t> page_state) {
    page_num_ = page_state.first;
    total_pages_ = page_state.second;
    page_details_changed_ = true;
  });

  event_bus_.subscribe<float>("statusbar.page_zoom", [this](float zoom) {
    display_zoom_.setString("[" + std::to_string((int)(zoom * 100)) + "%]");
  });
}

void Statusbar::draw(sf::RenderTarget &window) const {
  window.draw(display_area_);
  window.draw(display_filepath_);

  if (total_pages_ != 0) {
    window.draw(display_page_state_);
    window.draw(display_zoom_);
  }

  if (total_search_results_ != 0)
    window.draw(display_search_state_);
}

void Statusbar::update() {
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

  float y = std::round(curr_y_);
  if (move_up_)
    y = (y - utils::cmdline_height_) + 1.f;

  display_area_.setPosition({curr_x_, y});

  const float filepath_h = display_filepath_.getSize().y;
  const float filepath_y = std::round(y + (utils::cmdline_height_ - filepath_h) / 2.f) - 3.f;

  float search_state_x = display_area_.getSize().x;
  const auto &search_state_bounds = display_search_state_.getLocalBounds();
  if (total_search_results_ != 0)
    search_state_x = search_state_x - search_state_bounds.size.x - utils::padding;

  const auto &page_state_bounds = display_page_state_.getLocalBounds();
  const float page_state_x = search_state_x - page_state_bounds.size.x - utils::padding;

  const auto &zoom_bounds = display_zoom_.getLocalBounds();
  const float zoom_x = page_state_x - zoom_bounds.size.x - utils::padding;

  const float right_widgets_start = (total_pages_ != 0) ? zoom_x : display_area_.getSize().x;
  const float max_filepath_width = right_widgets_start - utils::padding * 2.f;

  if (max_filepath_width != last_available_width_) {
    last_available_width_ = max_filepath_width;

    display_filepath_.setString(filepath_);
    if (display_filepath_.getSize().x <= max_filepath_width) {
      last_truncated_ = filepath_;
    } else {
      const std::u32string ellipsis(3, '.');
      int lo = 0, hi = static_cast<int>(filepath_.size());
      while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        display_filepath_.setString(filepath_.substr(0, mid) + ellipsis);
        if (display_filepath_.getSize().x <= max_filepath_width)
          lo = mid;
        else
          hi = mid - 1;
      }
      last_truncated_ = filepath_.substr(0, lo) + ellipsis;
    }

    display_filepath_.setString(last_truncated_);
  }

  display_filepath_.setPosition({utils::padding, filepath_y});
  display_search_state_.setPosition({std::round(search_state_x), filepath_y + 1.f});
  display_page_state_.setPosition({std::round(page_state_x), filepath_y + 1.f});
  display_zoom_.setPosition({std::round(zoom_x), filepath_y + 1.f});
}

void Statusbar::onResize(const sf::Vector2f &size) {
  curr_x_ = 0;
  curr_y_ = size.y - utils::cmdline_height_;
  display_area_.setSize({size.x, utils::cmdline_height_});
}

} // namespace ui
