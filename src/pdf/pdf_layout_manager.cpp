#include "pdf/pdf_layout_manager.h"

#include "ui/page_view.h"
#include "utils/utils.h"

namespace pdf {

PDFLayoutManager::PDFLayoutManager(
    core::RenderScheduler &scheduler, std::vector<ui::PageView> &pages
)
    : scheduler_(scheduler), pages_(pages) {
  page_positions_dirty_ = false;
  anchor_page_pos_before_scroll_ = {0.f, 0.f};
  anchor_page_changed_ = false;
  need_initial_pos_ = false;
  scrolled_ = false;
}

void PDFLayoutManager::setAnchorPage(std::size_t idx) {
  anchor_page_ = idx;
  need_initial_pos_ = true;
  page_positions_dirty_ = true;
}

void PDFLayoutManager::setFrontPage(std::size_t idx) {
  front_page_ = idx;
}

void PDFLayoutManager::setBackPage(std::size_t idx) {
  back_page_ = idx;
}

std::size_t PDFLayoutManager::getAnchorPage() const {
  return anchor_page_;
}

std::size_t PDFLayoutManager::getFrontPage() const {
  return front_page_;
}

std::size_t PDFLayoutManager::getBackPage() const {
  return back_page_;
}

void PDFLayoutManager::setPageWithMaxWidth(std::size_t idx) {
  page_with_max_width_ = idx;
}

void PDFLayoutManager::setWindowSize(sf::Vector2f window_size) {
  old_window_size_ = window_size_;
  window_size_ = window_size;
  page_positions_dirty_ = true;
  window_size_changed_ = true;
}

void PDFLayoutManager::setPagePosDirty() {
  page_positions_dirty_ = true;
}

void PDFLayoutManager::panCurrentPage(sf::Vector2f delta) {
  if (!pages_[anchor_page_].hasTexture())
    return;

  if (std::abs(delta.x) < epsilon_) {
    anchor_page_pos_before_scroll_ = pages_[anchor_page_].getPosition();
    scrolled_ = true;
  }

  pages_[anchor_page_].move(delta);
  page_positions_dirty_ = true;
}

void PDFLayoutManager::updatePagePositions(const ui::VisualInfo &visual_state) {
  if (!page_positions_dirty_)
    return;

  if (!pages_[anchor_page_].hasTexture())
    return;

  ui::PageView &curr = pages_[anchor_page_];

  if (window_size_changed_) {
    const sf::Vector2f new_center{window_size_.x * 0.5f, window_size_.y * 0.5f};
    const sf::Vector2f curr_size = curr.getGlobalBounds().size;

    if (curr_size.x <= window_size_.x) {
      curr.setPosition({new_center.x, curr.getPosition().y});
    } else {
      const sf::Vector2f old_center{old_window_size_.x * 0.5f, old_window_size_.y * 0.5f};
      const auto &local_focus = curr.getSprite().getInverseTransform().transformPoint(old_center);
      const auto &focus_after = curr.getSprite().getTransform().transformPoint(local_focus);
      curr.move(new_center - focus_after);
    }

    window_size_changed_ = false;
  }

  clampAnchorHorizontally(visual_state);
  discretizePosition(curr);
  updateNeighbourPositions();

  const sf::Vector2f front_size = pages_[front_page_].getGlobalBounds().size;
  const sf::Vector2f back_size = pages_[back_page_].getGlobalBounds().size;

  const float front_top = pages_[front_page_].getPosition().y - front_size.y * 0.5f;
  const float back_bottom = pages_[back_page_].getPosition().y + back_size.y * 0.5f;
  const float bottom_limit = window_size_.y - utils::cmdline_height_;
  const float curr_y = curr.getPosition().y;

  const float total_height = back_bottom - front_top;
  if (total_height < bottom_limit) {
    const float offset = (bottom_limit * 0.5f) - (front_top + total_height * 0.5f);
    curr.setPosition({curr.getPosition().x, curr_y + offset});
  } else if (front_top > 0.f) {
    curr.setPosition({curr.getPosition().x, curr_y - front_top});
  } else if (back_bottom < bottom_limit) {
    curr.setPosition({curr.getPosition().x, curr_y + (bottom_limit - back_bottom)});
  } else {
    discretizePosition(curr);
    return;
  }

  discretizePosition(curr);
  updateNeighbourPositions();

  page_positions_dirty_ = false;
}

bool PDFLayoutManager::needInitialPos() const {
  return need_initial_pos_;
}

bool PDFLayoutManager::anchorPageChanged() const {
  return anchor_page_changed_;
}

void PDFLayoutManager::updateAnchorPage() {
  anchor_page_changed_ = false;

  const sf::Vector2f window_center = {window_size_.x * 0.5f, window_size_.y * 0.5f};

  std::size_t closest = anchor_page_;
  float closest_dist = std::numeric_limits<float>::max();

  for (std::size_t i = front_page_; i <= back_page_; i++) {
    if (!pages_[i].hasTexture())
      continue;
    const float page_center_y = pages_[i].getPosition().y;
    const float dist = std::abs(page_center_y - window_center.y);
    if (dist < closest_dist) {
      closest_dist = dist;
      closest = i;
    }
  }

  if (closest != anchor_page_ && scrolled_) {
    anchor_page_ = closest;
    anchor_page_changed_ = true;
  }
}

void PDFLayoutManager::setInitialPagePos() {
  if (!need_initial_pos_ || pages_.empty() || !pages_[anchor_page_].hasTexture())
    return;

  const auto size = scheduler_.getPageSize(pages_[anchor_page_].getKey());

  float x = window_size_.x * 0.5f;
  float y = size.y * 0.5f;
  if (size.y <= (window_size_.y - utils::cmdline_height_))
    y = (window_size_.y - utils::cmdline_height_) * 0.5;

  pages_[anchor_page_].setPosition({x, y});

  need_initial_pos_ = false;
  page_positions_dirty_ = true;
}

void PDFLayoutManager::updateNeighbourPositions() {
  if (anchor_page_ > front_page_) {
    sf::Vector2f below_size = pages_[anchor_page_].getGlobalBounds().size;
    for (int i = static_cast<int>(anchor_page_) - 1; i >= static_cast<int>(front_page_); --i) {
      const auto below_pos = pages_[i + 1].getPosition();
      const sf::Vector2f curr_size = pages_[i].getGlobalBounds().size;

      pages_[i].setPosition(
          {below_pos.x, below_pos.y - below_size.y * 0.5f - gap_ - curr_size.y * 0.5f}
      );
      below_size = curr_size;
    }
  }

  if (anchor_page_ < back_page_) {
    sf::Vector2f above_size = pages_[anchor_page_].getGlobalBounds().size;
    for (std::size_t i = anchor_page_ + 1; i <= back_page_; ++i) {
      const auto above_pos = pages_[i - 1].getPosition();
      const sf::Vector2f curr_size = pages_[i].getGlobalBounds().size;

      pages_[i].setPosition(
          {above_pos.x, above_pos.y + above_size.y * 0.5f + gap_ + curr_size.y * 0.5f}
      );
      above_size = curr_size;
    }
  }
}

void PDFLayoutManager::clampAnchorHorizontally(const ui::VisualInfo &visual_state) {
  auto &page = pages_[anchor_page_];
  const auto key = pdf::PDFRenderKey{page_with_max_width_, visual_state.zoom, visual_state.rotate, 0};

  const float window_w = window_size_.x;
  const float page_w = static_cast<float>(scheduler_.getPageSize(key).x);
  const float half_w = page_w * 0.5f;

  sf::Vector2f pos = page.getPosition();

  if (page_w <= window_w) {
    pos.x = window_w * 0.5f;
  } else {
    const float min_x = window_w - half_w;
    const float max_x = half_w;
    pos.x = std::clamp(pos.x, min_x, max_x);
  }

  page.setPosition(pos);
}

void PDFLayoutManager::discretizePosition(ui::PageView &page) {
  const auto &loc = page.getPosition();
  page.setPosition({0.f, 0.f});
  const sf::Vector2f offset = page.getSprite().getTransform().transformPoint({0.f, 0.f});

  const float x = std::round(loc.x + offset.x) - offset.x;
  const float y = std::round(loc.y + offset.y) - offset.y;
  page.setPosition({x, y});
}

} // namespace pdf
