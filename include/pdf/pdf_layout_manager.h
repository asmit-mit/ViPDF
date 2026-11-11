#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

#include "core/render_scheduler.h"
#include "ui/page_view.h"

namespace pdf {

class PDFLayoutManager {
public:
  PDFLayoutManager(core::RenderScheduler &scheduler, std::vector<ui::PageView> &pages);

  void setAnchorPage(std::size_t idx);
  void setFrontPage(std::size_t idx);
  void setBackPage(std::size_t idx);

  std::size_t getAnchorPage() const;
  std::size_t getFrontPage() const;
  std::size_t getBackPage() const;

  void setPageWithMaxWidth(std::size_t idx);
  void setWindowSize(sf::Vector2f window_size);
  void setPagePosDirty();

  void panCurrentPage(sf::Vector2f delta);
  void setInitialPagePos(const ui::VisualInfo &visual_state);
  void updatePagePositions(const ui::VisualInfo &visual_state);
  void updateAnchorPage();

  bool needInitialPos() const;
  bool isLayoutPending() const;
  bool anchorPageChanged() const;

  void reset();

private:
  void clampPageVertically(const ui::VisualInfo &visual_state);
  void clampPageHorizontally(const ui::VisualInfo &visual_state);
  void updateNeighbourPositions(const ui::VisualInfo &visual_state);
  void discretizePosition(ui::PageView &page);
  sf::Vector2f pageSize(std::size_t page_idx, const ui::VisualInfo &vs) const;

private:
  core::RenderScheduler &scheduler_;
  std::vector<ui::PageView> &pages_;

  std::size_t anchor_page_;
  std::size_t front_page_;
  std::size_t back_page_;

  std::size_t page_with_max_width_;

  bool page_positions_dirty_;
  bool scrolled_;
  bool window_size_changed_;

  bool need_initial_pos_;
  bool anchor_page_changed_;

  sf::Vector2f window_size_;
  sf::Vector2f old_window_size_;
  sf::Vector2f anchor_page_pos_before_scroll_;

  static constexpr float gap_ = 1.f;
  static constexpr float epsilon_ = 1e-5f;
};

} // namespace pdf
