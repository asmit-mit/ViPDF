#pragma once

#include <SFML/Graphics.hpp>

#include "core/bookmark_manager.h"
#include "core/config_reader.h"
#include "core/event_bus.h"
#include "core/history_manager.h"
#include "core/input_handler.h"
#include "core/render_scheduler.h"
#include "mupdf/pdf/annot.h"
#include "pdf/pdf_document.h"
#include "pdf/pdf_layout_manager.h"
#include "pdf/pdf_search_controller.h"
#include "pdf/pdf_selection_controller.h"
#include "pdf/pdf_view_controller.h"
#include "ui/page_view.h"

namespace ui {

enum class DocumentAction {
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
  CopySelection,
  AnnotateHighlight,
  AnnotateStrikethrough,
  AnnotateUnderline,
  AnnotateRemove,
  Cancel
};

class DocumentView {
public:
  explicit DocumentView(
      pdf::PDFDocument &document,
      core::HistoryManager &file_history,
      core::BookmarkManager &bookmark_manager,
      core::RenderScheduler &scheduler,
      core::EventBus &event_bus,
      const core::Config &config
  );

  void draw(sf::RenderTarget &window) const;
  void update();
  void handleEvent(const sf::Event &event);

  void onResize(const sf::Vector2f &size);

private:
  struct PageRange {
    int front;
    int back;

    bool contains(int i) const {
      return i >= front && i <= back;
    }

    int size() const {
      return back - front + 1;
    }
  };

  enum class ScrollTo {
    LINK,
    SEARCH,
    BOOKMARK,
    RELOAD,
  };

private:
  void onOpenDocument(const std::string &filepath);
  void onCloseDocument();
  void onSwitchPage(int page_idx);
  void onSearchPage(const std::u32string &text);
  void onAnnotate(std::vector<std::size_t> pages);
  void onAuthenticate();

  void registerEvents();
  void registerKeybindings();
  bool canWorkWithDocument() const;

  sf::Vector2f getSearchResultPos(std::size_t anchor_page) const;
  sf::Vector2f getPageOffsetFromCenter(std::size_t page_idx) const;
  void renderRequestedPages();

  void requestVisiblePages(std::size_t page_idx, float zoom, int rotate, bool recolor);
  PageRange computeVisibleRange(std::size_t anchor, float zoom, int rotate) const;
  PageRange computeRetainedRange(PageRange visible) const;
  void
  releaseOutOfWindowPages(PageRange visible, PageRange retained, const pdf::PDFRenderKey &target);

  void resetView();

  void execute(const core::InputCmd<DocumentAction> &cmd);
  void handleScroll(const sf::Event::MouseWheelScrolled &scroll);
  void panBy(sf::Vector2f delta);
  void zoomBy(float delta);
  void fitPage();
  void fitWidth();
  void gotoPage(std::optional<std::size_t> one_based, std::size_t fallback);
  void stepSearch(bool forward);
  void copySelection();
  void addAnnotation(enum pdf_annot_type type, int color_index);
  void removeAnnotation();
  void emitSearchState(std::size_t display_offset);

  bool isPageSwitchAction(DocumentAction action);

private:
  std::vector<PageView> pages_;
  pdf::PDFDocument &document_;
  pdf::PDFLayoutManager layout_manager_;
  pdf::PDFViewController view_controller_;
  pdf::PDFSearchController search_controller_;
  pdf::PDFSelectionController selection_controller_;

  const core::Config &config_;
  core::EventBus &event_bus_;
  core::HistoryManager &file_history_;
  core::BookmarkManager &bookmark_manager_;
  core::RenderScheduler &scheduler_;
  core::InputHandler<DocumentAction> input_handler_;

  sf::Texture dummy_;

  bool focus_update_;
  bool should_take_input_;
  bool update_recolor_;
  bool recolor_;
  bool selecting_;

  sf::Vector2f window_size_;
  sf::Clock page_switch_clock_;
  sf::Time page_switch_delay_ = sf::milliseconds(20);

  const sf::Color selection_box_color_;
  const sf::Color search_box_color_;
  const sf::Color selected_search_box_color_;

  std::optional<int> pending_page_swtich_;
  std::optional<sf::Vector2f> pending_page_scroll_;
  std::optional<float> pending_zoom_update_;
  std::optional<int> pending_rotate_update_;
  std::optional<PageRange> kept_range_;

  ScrollTo scroll_to_;

  static constexpr float scroll_dist_ = 40.f;
  static constexpr int prefetch_count_ = 2;
};

} // namespace ui
