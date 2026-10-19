#pragma once

#include <SFML/Graphics.hpp>

#include "core/event_bus.h"
#include "core/history_manager.h"
#include "core/input_handler.h"
#include "core/render_scheduler.h"
#include "pdf/pdf_document.h"
#include "pdf/pdf_layout_manager.h"
#include "pdf/pdf_search_controller.h"
#include "pdf/pdf_selection_controller.h"
#include "pdf/pdf_view_controller.h"
#include "ui/page_view.h"

namespace ui {

class DocumentView {
public:
  explicit DocumentView(
      core::HistoryManager &file_history,
      pdf::PDFDocument &document,
      core::RenderScheduler &scheduler,
      core::EventBus &event_bus
  );

  void draw(sf::RenderTarget &window) const;
  void update();
  void handleEvent(const sf::Event &event);

  void onResize(const sf::Vector2f &size);

private:
  void onOpenDocument(const std::string &filepath);
  void onCloseDocument();
  void onSwitchPage(int page_idx);
  void onSearchPage(const std::u32string &text);
  void onAnnotate(std::vector<std::size_t> pages);

  void centerOnSearchResult(
      std::size_t anchor_page,
      std::size_t front_page,
      std::size_t back_page,
      const ui::VisualInfo &view_state
  );
  void renderRequestedPages();
  void requestPage(std::size_t page_idx, float zoom, int rotate, bool recolor);
  void resetView();

  void execute(const core::DocumentCmd &cmd);
  void handleScroll(const sf::Event::MouseWheelScrolled &scroll);
  void panBy(sf::Vector2f delta);
  void zoomBy(float delta);
  void fitPage();
  void fitWidth();
  void gotoPage(std::optional<std::size_t> one_based, std::size_t fallback);
  void stepSearch(bool forward);
  void emitSearchState(std::size_t display_offset);
  void highlight(int color_index);

private:
  std::vector<PageView> pages_;
  pdf::PDFDocument &document_;
  pdf::PDFLayoutManager layout_manager_;
  pdf::PDFViewController view_controller_;
  pdf::PDFSearchController search_controller_;
  pdf::PDFSelectionController selection_controller_;

  core::EventBus &event_bus_;
  core::HistoryManager &file_history_;
  core::RenderScheduler &scheduler_;
  core::InputHandler input_handler_;

  sf::Texture dummy_;

  bool should_take_input_;
  bool update_recolor_;
  bool recolor_;
  bool selecting_;

  sf::Vector2f window_size_;

  static constexpr float scroll_dist_ = 40.f;
};

} // namespace ui
