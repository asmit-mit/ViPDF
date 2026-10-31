#pragma once

#include <vector>

#include "core/render_scheduler.h"
#include "ui/page_view.h"

namespace pdf {

class PDFViewController {
public:
  PDFViewController(core::RenderScheduler &scheduler, std::vector<ui::PageView> &pages);

  void setZoom(float zoom);
  void setRotate(int rotate);
  void setSyncDirty();

  void clearPendingUpdates();
  void clearSyncDirty();

  void setWindowSize(sf::Vector2f window_size);

  float getZoom() const;
  int getRotate() const;
  ui::VisualInfo getState() const;

  bool isSyncDirty() const;
  bool canUpdatePages() const;

  void syncWithTargetState(std::size_t front, std::size_t anchor, std::size_t back);

  void reset();

private:
  core::RenderScheduler &scheduler_;
  std::vector<ui::PageView> &pages_;

  ui::VisualInfo target_state_;

  sf::Clock scale_rot_update_timer_;
  sf::Vector2f window_size_;

  bool pending_page_update_;
  bool sync_state_dirty_;

  static constexpr float scale_rot_debounce_ms_ = 300.f;
  static constexpr float min_zoom_ = 0.2f;
  static constexpr float max_zoom_ = 5.f;
};

} // namespace pdf
