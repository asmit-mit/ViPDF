#include "pdf/pdf_view_controller.h"

namespace pdf {

PDFViewController::PDFViewController(
    core::RenderScheduler &scheduler, std::vector<ui::PageView> &pages
)
    : scheduler_(scheduler), pages_(pages) {
  pending_page_update_ = false;
  sync_state_dirty_ = false;
}

void PDFViewController::setZoom(float zoom) {
  const float clamped = std::clamp(zoom, min_zoom_, max_zoom_);
  if (clamped == target_state_.zoom)
    return;

  target_state_.zoom = clamped;
  scale_rot_update_timer_.restart();

  pending_page_update_ = true;
  sync_state_dirty_ = true;
}

void PDFViewController::setRotate(int rotate) {
  const int clamped = ((rotate % 4) + 4) % 4;
  if (clamped == target_state_.rotate)
    return;

  target_state_.rotate = clamped;
  scale_rot_update_timer_.restart();

  pending_page_update_ = true;
  sync_state_dirty_ = true;
}

void PDFViewController::setSyncDirty() {
  sync_state_dirty_ = true;
}

void PDFViewController::setWindowSize(sf::Vector2f window_size) {
  window_size_ = window_size;
}

bool PDFViewController::isSyncDirty() const {
  return sync_state_dirty_;
}

ui::VisualInfo PDFViewController::getState() const {
  return target_state_;
}

bool PDFViewController::canUpdatePages() const {
  return pending_page_update_ &&
         scale_rot_update_timer_.getElapsedTime().asMilliseconds() > scale_rot_debounce_ms_;
}

void PDFViewController::clearPendingUpdates() {
  pending_page_update_ = false;
}

void PDFViewController::clearSyncDirty() {
  sync_state_dirty_ = false;
}

void PDFViewController::syncWithTargetState(
    std::size_t front, std::size_t anchor, std::size_t back
) {
  if (!sync_state_dirty_ || pages_.empty())
    return;

  for (std::size_t i = front; i <= back; ++i) {
    pdf::PDFRenderKey target_key{i, target_state_.zoom, target_state_.rotate, 0};
    auto &page = pages_[i];
    if (!page.hasTexture() ||
        (page.getKey().rotate == target_key.rotate && page.getKey().zoom == target_key.zoom))
      continue;

    const int delta = (target_state_.rotate - page.getKey().rotate + 4) % 4;
    const auto current = page.getTextureSize();
    const auto target = scheduler_.getPageSize(target_key);

    const float target_w = (delta % 2 == 0) ? static_cast<float>(target.x)
                                            : static_cast<float>(target.y);
    const float target_h = (delta % 2 == 0) ? static_cast<float>(target.y)
                                            : static_cast<float>(target.x);

    const float scale_x = current.x ? target_w / static_cast<float>(current.x) : 1.f;
    const float scale_y = current.y ? target_h / static_cast<float>(current.y) : 1.f;

    if (i != anchor) {
      page.setScale({scale_x, scale_y});
      page.setRotation(delta);
      page.syncPageShape(target_state_.rotate);
      continue;
    }

    const sf::Vector2f window_center = {window_size_.x * 0.5f, window_size_.y * 0.5f};
    const auto &local_focus = page.getSprite().getInverseTransform().transformPoint(window_center);

    page.setScale({scale_x, scale_y});
    page.setRotation(delta);
    page.syncPageShape(target_state_.rotate);

    if (delta == 0) {
      const auto &focus_after = page.getSprite().getTransform().transformPoint(local_focus);
      const auto &offset = window_center - focus_after;
      page.move(offset);
    }
  }

  sync_state_dirty_ = false;
}

} // namespace pdf
