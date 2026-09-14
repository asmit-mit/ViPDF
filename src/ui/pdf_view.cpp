#include "ui/pdf_view.h"

#include <algorithm>
#include <stdexcept>
#include <utf8.h>

#include "pdf/pdf_renderer.h"
#include "ui/ui_elements.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace ui {

PDFView::PDFView(
    core::HistoryManager &file_history,
    pdf::PDFDocument &document,
    core::RenderScheduler &scheduler,
    core::EventBus &event_bus
)
    : document_(document), layout_manager_(scheduler, pages_), view_controller_(scheduler, pages_),
      search_controller_(pages_), event_bus_(event_bus), file_history_(file_history),
      scheduler_(scheduler) {
  should_take_input_ = false;

  layout_manager_.setAnchorPage(0);
  layout_manager_.setFrontPage(0);
  layout_manager_.setBackPage(0);
  layout_manager_.setPageWithMaxWidth(0);

  prev_key_ = sf::Keyboard::Key::Unknown;

  event_bus_.subscribe<std::string>("cmd.open_document", [this](const std::string &filepath) {
    onOpenDocument(filepath);
  });

  event_bus_.subscribe<bool>("cmd.reload_document", [this](bool) {
    if (!document_.isOpen()) {
      event_bus_.emit("notification.msg", std::string("No document open to reload"));
      return;
    }

    std::size_t curr_page = layout_manager_.getAnchorPage();
    onOpenDocument(document_.getFilePath());
    onSwitchPage(curr_page);
  });

  event_bus_.subscribe<bool>("cmd.close_document", [this](bool) { onCloseDocument(); });

  event_bus_.subscribe<int>("cmd.switch_page", [this](int page_num) {
    onSwitchPage(page_num - 1);
  });

  event_bus_.subscribe<const std::u32string &>("cmd.search", [this](const std::u32string &text) {
    onSearchPage(text);
  });

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    prev_key_ = sf::Keyboard::Key::Unknown;
    should_take_input_ = focus == ui::UIElements::PDFView;
  });
}

void PDFView::draw(sf::RenderTarget &window) const {
  if (!document_.isOpen())
    return;

  if (layout_manager_.needInitialPos())
    return;

  for (std::size_t i = layout_manager_.getFrontPage(); i <= layout_manager_.getBackPage(); i++)
    pages_[i].draw(window);
}

void PDFView::update() {
  if (!document_.isOpen())
    return;

  std::size_t front_page = layout_manager_.getFrontPage();
  std::size_t anchor_page = layout_manager_.getAnchorPage();
  std::size_t back_page = layout_manager_.getBackPage();
  const VisualInfo view_state = view_controller_.getState();

  if (document_.isOpen() && view_controller_.canUpdatePages()) {
    requestPage(layout_manager_.getAnchorPage(), view_state.zoom, view_state.rotate);
    view_controller_.clearPendingUpdates();
  }

  renderRequestedPages();
  layout_manager_.setInitialPagePos();

  if (view_controller_.isSyncDirty()) {
    view_controller_.syncWithTargetState(front_page, anchor_page, back_page);
    layout_manager_.setPagePosDirty();
  }

  layout_manager_.updatePagePositions(view_state);
  for (std::size_t i = front_page; i <= back_page; i++)
    pages_[i].syncPageShapePos();

  if (search_controller_.isSearchPosDirty() && pages_[anchor_page].hasTexture()) {
    centerOnSearchResult(anchor_page, front_page, back_page, view_state);
    search_controller_.clearSearchPosDirty();
  }

  layout_manager_.updateAnchorPage();
  anchor_page = layout_manager_.getAnchorPage();

  if (layout_manager_.anchorPageChanged()) {
    requestPage(anchor_page, view_state.zoom, view_state.rotate);
    event_bus_.emit("statusbar.page_state", std::make_pair(anchor_page + 1, document_.pageCount()));
  }

  front_page = layout_manager_.getFrontPage();
  back_page = layout_manager_.getBackPage();

  for (std::size_t i = front_page; i <= back_page; i++) {
    if (search_controller_.isShowingSearchResult())
      pages_[i].showSearchResults();
    else
      pages_[i].hideSearchResults();
    pages_[i].update();
  }
}

void PDFView::handleEvent(const sf::Event &event) {
  if (!document_.isOpen())
    return;

  const auto *key = event.getIf<sf::Event::KeyPressed>();
  const auto *scroll = event.getIf<sf::Event::MouseWheelScrolled>();

  std::size_t anchor_page = layout_manager_.getAnchorPage();
  const VisualInfo view_state = view_controller_.getState();

  if (scroll && should_take_input_) {
    if (scroll->wheel == sf::Mouse::Wheel::Vertical) {
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl)) {
        view_controller_.setZoom(view_state.zoom + scroll->delta * settings::delta_zoom_);
        event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
      } else {
        layout_manager_.panCurrentPage({0.f, scroll->delta * scroll_dist_});
      }
    } else if (scroll->wheel == sf::Mouse::Wheel::Horizontal) {
      layout_manager_.panCurrentPage({scroll->delta * scroll_dist_, 0.f});
    }
  }

  if (key && should_take_input_) {
    if (key->code == sf::Keyboard::Key::U) {
      if (key->control)
        layout_manager_.panCurrentPage({0.f, window_size_.y * 0.5f});
      else
        onSwitchPage(std::max(0, (int)anchor_page - 1));
    } else if (key->code == sf::Keyboard::Key::D) {
      if (key->control)
        layout_manager_.panCurrentPage({0.f, -window_size_.y * 0.5f});
      else
        onSwitchPage(std::min(document_.pageCount() - 1, anchor_page + 1));
    } else if (key->code == sf::Keyboard::Key::Equal && key->control) {
      view_controller_.setZoom(view_state.zoom + settings::delta_zoom_);
      event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
    } else if (key->code == sf::Keyboard::Key::Hyphen && key->control) {
      view_controller_.setZoom(view_state.zoom - settings::delta_zoom_);
      event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
    } else if (key->code == sf::Keyboard::Key::H) {
      layout_manager_.panCurrentPage({scroll_dist_, 0.f});
    } else if (key->code == sf::Keyboard::Key::L) {
      layout_manager_.panCurrentPage({-scroll_dist_, 0.f});
    } else if (key->code == sf::Keyboard::Key::K) {
      layout_manager_.panCurrentPage({0.f, scroll_dist_});
    } else if (key->code == sf::Keyboard::Key::J) {
      layout_manager_.panCurrentPage({0.f, -scroll_dist_});
    } else if (key->code == sf::Keyboard::Key::G) {
      if (key->shift)
        onSwitchPage(document_.pageCount() - 1);
      else if (prev_key_ == sf::Keyboard::Key::G)
        onSwitchPage(0);
      else
        prev_key_ = sf::Keyboard::Key::G;
    } else if (key->code == sf::Keyboard::Key::N) {
      int page = -1;

      if (key->shift)
        page = search_controller_.goPrev(layout_manager_.getAnchorPage());
      else
        page = search_controller_.goNext(layout_manager_.getAnchorPage());

      if (page != -1)
        onSwitchPage(page);

      std::size_t curr = search_controller_.getCurrSearchResult();
      std::size_t total = search_controller_.getTotalSearchResult();
      event_bus_.emit("statusbar.search_state", std::make_pair(curr + 1, total));
    } else if (key->code == sf::Keyboard::Key::R) {
      if (key->shift)
        view_controller_.setRotate((view_state.rotate - 1) % 4);
      else
        view_controller_.setRotate((view_state.rotate + 1) % 4);
    } else if (key->code == sf::Keyboard::Key::F) {
      pdf::PDFRenderKey base_key{anchor_page, 1.f, view_state.rotate};
      const auto page_dims = scheduler_.getPageSize(base_key);
      if (page_dims.x > 0 && page_dims.y > 0) {
        const float viewable_h = window_size_.y - utils::cmdline_height_;
        const float viewable_w = window_size_.x;
        const float zoom_x = viewable_w / static_cast<float>(page_dims.x);
        const float zoom_y = viewable_h / static_cast<float>(page_dims.y);
        view_controller_.setZoom(std::min(zoom_x, zoom_y));
        event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
      }
    } else if (key->code == sf::Keyboard::Key::W) {
      pdf::PDFRenderKey base_key{anchor_page, 1.f, view_state.rotate};
      const auto page_dims = scheduler_.getPageSize(base_key);
      if (page_dims.x > 0) {
        const float viewable_w = window_size_.x;
        view_controller_.setZoom(viewable_w / static_cast<float>(page_dims.x));
        event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
      }
    } else if (key->code == sf::Keyboard::Key::Escape) {
      search_controller_.reset();
      std::size_t curr = search_controller_.getCurrSearchResult();
      std::size_t total = search_controller_.getTotalSearchResult();
      event_bus_.emit("statusbar.search_state", std::make_pair(curr, total));
    }

    if (key->code != sf::Keyboard::Key::G)
      prev_key_ = sf::Keyboard::Key::Unknown;
  }
}

void PDFView::onResize(const sf::Vector2f &size) {
  window_size_ = size;
  view_controller_.setWindowSize(size);
  layout_manager_.setWindowSize(size);
}

void PDFView::onOpenDocument(const std::string &filepath) {
  try {
    std::string absolute_path = utils::resolvePath(filepath);
    scheduler_.quiesce();
    document_.openDocument(absolute_path);
    scheduler_.clearCache();
    scheduler_.resume();

    resetView();
    onSwitchPage(0);
    layout_manager_.setPageWithMaxWidth(document_.pageWithMaxWidth());

    event_bus_.emit("statusbar.pdf_path", absolute_path);
    event_bus_.emit("statusbar.page_state", std::make_pair(0, document_.pageCount()));
    event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);

    file_history_.add(absolute_path);
  } catch (const std::runtime_error &e) {
    onCloseDocument();
    event_bus_.emit("notification.msg", std::string(e.what()));
  }
}

void PDFView::onCloseDocument() {
  if (!document_.isOpen())
    return;

  resetView();
  document_.closeDocument();
  event_bus_.emit("statusbar.pdf_path", std::string("[No document open]"));
  event_bus_
      .emit("statusbar.page_state", std::make_pair(static_cast<std::size_t>(0), static_cast<std::size_t>(0)));
}

void PDFView::onSwitchPage(int page_idx) {
  if (!document_.isOpen()) {
    event_bus_.emit("notification.msg", std::string("No document currently open"));
    return;
  }

  if (page_idx < 0 || page_idx >= static_cast<int>(document_.pageCount())) {
    event_bus_.emit("notification.msg", std::string("Page number out of range"));
    return;
  }

  std::size_t anchor_page = static_cast<std::size_t>(page_idx);
  VisualInfo view_state = view_controller_.getState();
  layout_manager_.setAnchorPage(anchor_page);
  requestPage(anchor_page, view_state.zoom, view_state.rotate);
  event_bus_.emit("statusbar.page_state", std::make_pair(anchor_page + 1, document_.pageCount()));
}

void PDFView::onSearchPage(const std::u32string &text) {
  if (!document_.isOpen()) {
    event_bus_.emit("notification.msg", std::string("No document open to search"));
    return;
  }

  int closest_page = search_controller_.search(text, layout_manager_.getAnchorPage(), document_);
  std::size_t curr = search_controller_.getCurrSearchResult();
  std::size_t total = search_controller_.getTotalSearchResult();

  if (closest_page == -1) {
    event_bus_.emit("statusbar.search_state", std::make_pair(curr, total));
    event_bus_.emit("notification.msg", std::string("No match found for: ") + utf8::utf32to8(text));
    return;
  }

  if (closest_page != static_cast<int>(layout_manager_.getAnchorPage()))
    onSwitchPage(closest_page);

  event_bus_.emit("statusbar.search_state", std::make_pair(curr + 1, total));
}

void PDFView::centerOnSearchResult(
    std::size_t anchor_page,
    std::size_t front_page,
    std::size_t back_page,
    const ui::VisualInfo &view_state
) {
  const sf::Vector2f window_center = {window_size_.x * 0.5f, window_size_.y * 0.5f};
  const sf::Vector2f pos = pages_[anchor_page].getSearchResultPosition(
      search_controller_.getLocalSearchResult()
  );
  sf::Vector2f delta = window_center - pos;
  layout_manager_.panCurrentPage(delta);
  layout_manager_.updatePagePositions(view_state);
  for (std::size_t i = front_page; i <= back_page; i++)
    pages_[i].syncPageShapePos();
}

void PDFView::renderRequestedPages() {
  const VisualInfo view_state = view_controller_.getState();
  for (std::size_t i = layout_manager_.getFrontPage(); i <= layout_manager_.getBackPage(); i++) {
    pdf::PDFRenderKey target_key = {i, view_state.zoom, view_state.rotate};
    if (!scheduler_.isReady(target_key))
      continue;

    if (pages_[i].hasTexture() && pages_[i].getKey() == target_key) {
      pages_[i].setScale({1.f, 1.f});
      pages_[i].syncPageShape(view_state.rotate);
      continue;
    }

    pages_[i].setTexture(*scheduler_.getTexture(target_key));
    pages_[i].setKey(target_key);

    pages_[i].setScale({1.f, 1.f});
    pages_[i].setRotation(0);

    view_controller_.setSyncDirty();
    layout_manager_.setPagePosDirty();
  }
}

void PDFView::requestPage(std::size_t page_idx, float zoom, int rotate) {
  if (!document_.isOpen())
    return;

  pdf::PDFRenderKey center_key{page_idx, zoom, rotate};
  const sf::Vector2u page_dims = scheduler_.getPageSize(center_key);

  int total_height = static_cast<int>(page_dims.y);
  int front = static_cast<int>(page_idx);
  int back = static_cast<int>(page_idx);

  while (total_height <= window_size_.y) {
    bool expanded = false;

    if (back + 1 < static_cast<int>(document_.pageCount())) {
      back++;
      pdf::PDFRenderKey key{static_cast<size_t>(back), zoom, rotate};
      total_height += scheduler_.getPageSize(key).y;
      expanded = true;
    }

    if (front > 0) {
      --front;
      pdf::PDFRenderKey key{static_cast<size_t>(front), zoom, rotate};
      total_height += scheduler_.getPageSize(key).y;
      expanded = true;
    }

    if (!expanded)
      break;
  }

  for (int i = 0; i < 3; i++) {
    if (back + 1 < static_cast<int>(document_.pageCount()))
      ++back;

    if (front > 0)
      --front;
  }

  for (std::size_t i = layout_manager_.getFrontPage(); i <= layout_manager_.getBackPage(); ++i) {
    if (static_cast<int>(i) < front || static_cast<int>(i) > back)
      pages_[i].reset();
  }

  for (int i = front; i <= back; ++i) {
    pdf::PDFRenderKey key{static_cast<std::size_t>(i), zoom, rotate};
    if (pages_[i].hasTexture() && pages_[i].getKey() == key)
      continue;

    scheduler_.request(key);
    if (search_controller_.isShowingSearchResult())
      pages_[i].showSearchResults();
  }

  layout_manager_.setFrontPage(static_cast<std::size_t>(front));
  layout_manager_.setBackPage(static_cast<std::size_t>(back));
}

void PDFView::resetView() {
  pages_.clear();
  pages_.resize(document_.pageCount(), dummy_);
  for (std::size_t i = 0; i < document_.pageCount(); i++) {
    const auto &bounds = document_.getPage(i).page_bounds;
    float width = bounds.x1 - bounds.x0;
    float height = bounds.y1 - bounds.y0;
    pages_[i].setPageShapeSize({width, height});
  }

  layout_manager_.setPageWithMaxWidth(0);
  layout_manager_.setFrontPage(0);
  layout_manager_.setAnchorPage(0);
  layout_manager_.setBackPage(0);

  view_controller_.setZoom(1.f);
  view_controller_.setRotate(0);
  view_controller_.clearSyncDirty();

  search_controller_.reset();
}

} // namespace ui
