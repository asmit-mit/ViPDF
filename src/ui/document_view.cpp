#include "ui/document_view.h"

#include <algorithm>
#include <stdexcept>
#include <utf8.h>

#include "core/input_handler.h"
#include "pdf/pdf_renderer.h"
#include "pdf/pdf_selection_controller.h"
#include "ui/ui_elements.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace ui {

DocumentView::DocumentView(
    core::HistoryManager &file_history,
    pdf::PDFDocument &document,
    core::RenderScheduler &scheduler,
    core::EventBus &event_bus
)
    : document_(document), layout_manager_(scheduler, pages_), view_controller_(scheduler, pages_),
      search_controller_(pages_), selection_controller_(document_, pages_), event_bus_(event_bus),
      file_history_(file_history), scheduler_(scheduler) {
  should_take_input_ = false;
  recolor_ = settings::default_recolor;
  update_recolor_ = false;
  selecting_ = false;
  focus_update_ = false;

  layout_manager_.setAnchorPage(0);
  layout_manager_.setFrontPage(0);
  layout_manager_.setBackPage(0);
  layout_manager_.setPageWithMaxWidth(0);

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
    bool prev = should_take_input_;
    should_take_input_ = focus == ui::UIElements::PDFView;
    focus_update_ = prev != should_take_input_;
  });

  registerKeybindings();
}

void DocumentView::draw(sf::RenderTarget &window) const {
  if (!document_.isOpen())
    return;

  if (layout_manager_.needInitialPos())
    return;

  for (std::size_t i = layout_manager_.getFrontPage(); i <= layout_manager_.getBackPage(); i++)
    pages_[i].draw(window);
}

void DocumentView::update() {
  if (!document_.isOpen())
    return;

  std::size_t front_page = layout_manager_.getFrontPage();
  std::size_t anchor_page = layout_manager_.getAnchorPage();
  std::size_t back_page = layout_manager_.getBackPage();
  const VisualInfo view_state = view_controller_.getState();

  if (update_recolor_) {
    recolor_ = !recolor_;
    requestPage(layout_manager_.getAnchorPage(), view_state.zoom, view_state.rotate, recolor_);
    update_recolor_ = false;
  }

  if (document_.isOpen() && view_controller_.canUpdatePages()) {
    requestPage(layout_manager_.getAnchorPage(), view_state.zoom, view_state.rotate, recolor_);
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
    requestPage(anchor_page, view_state.zoom, view_state.rotate, recolor_);
    event_bus_.emit("statusbar.page_state", std::make_pair(anchor_page + 1, document_.pageCount()));
  }

  front_page = layout_manager_.getFrontPage();
  back_page = layout_manager_.getBackPage();

  for (std::size_t i = front_page; i <= back_page; i++)
    pages_[i].update();

  if (focus_update_ && !should_take_input_) {
    input_handler_.reset();
    event_bus_.emit("statusbar.pending_keys", std::string(""));
    focus_update_ = false;
  }
}

void DocumentView::handleEvent(const sf::Event &event) {
  if (!document_.isOpen())
    return;

  const std::size_t front_page = layout_manager_.getFrontPage();
  const std::size_t back_page = layout_manager_.getBackPage();

  if (should_take_input_) {
    if (const auto cmd = input_handler_.handleEvent(event))
      execute(*cmd);

    if (event.is<sf::Event::KeyPressed>())
      event_bus_.emit("statusbar.pending_keys", input_handler_.pendingText());

    if (const auto *scroll = event.getIf<sf::Event::MouseWheelScrolled>())
      handleScroll(*scroll);
  }

  if (const auto *mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouse->button == sf::Mouse::Button::Left) {
      if (selection_controller_.isShowingSelection())
        selection_controller_.reset();
      selecting_ = true;
      for (std::size_t i = front_page; i <= back_page; i++) {
        if (pages_[i].containsMouse(mouse->position)) {
          selection_controller_.setSelectionStart(i, mouse->position);
          break;
        }
      }
      selection_controller_.showSelection();
    }
  }

  if (const auto *move = event.getIf<sf::Event::MouseMoved>()) {
    auto cursor = ui::CursorType::Arrow;
    for (std::size_t i = front_page; i <= back_page; i++) {
      if (!document_.getPage(i).isContentLoaded())
        document_.loadPageContent(i);
      const sf::Vector2f point = pages_[i].getReferenceShape().getInverseTransform().transformPoint(
          sf::Vector2f(move->position)
      );

      if (pages_[i].containsMouse(move->position) && document_.getPage(i).mouseInGlyph(point)) {
        cursor = ui::CursorType::Text;
        break;
      }
    }
    event_bus_.emit("ui.cursor_type", cursor);

    if (selecting_) {
      for (std::size_t i = front_page; i <= back_page; i++) {
        if (pages_[i].containsMouse(move->position)) {
          selection_controller_.setSelectionEnd(i, move->position);
          break;
        }
      }
    }
  }

  if (const auto *mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
    if (mouse->button == sf::Mouse::Button::Left)
      selecting_ = false;
  }
}

void DocumentView::handleScroll(const sf::Event::MouseWheelScrolled &scroll) {
  if (scroll.wheel == sf::Mouse::Wheel::Vertical) {
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl))
      zoomBy(scroll.delta * settings::delta_zoom_);
    else
      panBy({0.f, scroll.delta * scroll_dist_});
  } else if (scroll.wheel == sf::Mouse::Wheel::Horizontal) {
    panBy({scroll.delta * scroll_dist_, 0.f});
  }
}

void DocumentView::execute(const core::InputCmd<DocumentAction> &cmd) {
  const std::size_t times = cmd.countOr(1);
  const float n = static_cast<float>(times);
  const std::size_t anchor_page = layout_manager_.getAnchorPage();
  const std::size_t last_page = document_.pageCount() - 1;
  const auto rotate = view_controller_.getState().rotate;

  switch (cmd.action) {
  case DocumentAction::PanLeft:
    panBy({scroll_dist_ * n, 0.f});
    break;
  case DocumentAction::PanRight:
    panBy({-scroll_dist_ * n, 0.f});
    break;
  case DocumentAction::PanUp:
    panBy({0.f, scroll_dist_ * n});
    break;
  case DocumentAction::PanDown:
    panBy({0.f, -scroll_dist_ * n});
    break;
  case DocumentAction::HalfPageUp:
    panBy({0.f, window_size_.y * 0.5f * n});
    break;
  case DocumentAction::HalfPageDown:
    panBy({0.f, -window_size_.y * 0.5f * n});
    break;

  case DocumentAction::PrevPage:
    onSwitchPage(anchor_page > times ? anchor_page - times : 0);
    break;
  case DocumentAction::NextPage:
    onSwitchPage(std::min(last_page, anchor_page + times));
    break;
  case DocumentAction::FirstPage:
    gotoPage(cmd.count, 0);
    break;
  case DocumentAction::LastPage:
    gotoPage(cmd.count, last_page);
    break;

  case DocumentAction::ZoomIn:
    zoomBy(settings::delta_zoom_);
    break;
  case DocumentAction::ZoomOut:
    zoomBy(-settings::delta_zoom_);
    break;
  case DocumentAction::FitPage:
    fitPage();
    break;
  case DocumentAction::FitWidth:
    fitWidth();
    break;

  case DocumentAction::RotateCW:
    view_controller_.setRotate((rotate + 1) % 4);
    break;
  case DocumentAction::RotateCCW:
    view_controller_.setRotate((rotate + 3) % 4);
    break;
  case DocumentAction::Recolor:
    update_recolor_ = true;
    break;

  case DocumentAction::SearchNext:
    stepSearch(true);
    break;
  case DocumentAction::SearchPrev:
    stepSearch(false);
    break;

  case DocumentAction::AnnotateHighlight:
    if (cmd.arg)
      addAnnotation(PDF_ANNOT_HIGHLIGHT, *cmd.arg);
    break;
  case DocumentAction::AnnotateStrikethrough:
    if (cmd.arg)
      addAnnotation(PDF_ANNOT_STRIKE_OUT, *cmd.arg);
    break;
  case DocumentAction::AnnotateUnderline:
    if (cmd.arg)
      addAnnotation(PDF_ANNOT_UNDERLINE, *cmd.arg);
    break;
  case DocumentAction::AnnotateRemove:
    removeAnnotation();
    break;

  case DocumentAction::Cancel:
    selection_controller_.reset();
    search_controller_.reset();
    emitSearchState(0);
    event_bus_.emit("statusbar.pending_keys", std::string(""));
    break;
  }
}

void DocumentView::panBy(sf::Vector2f delta) {
  layout_manager_.panCurrentPage(delta);
}

void DocumentView::zoomBy(float delta) {
  view_controller_.setZoom(view_controller_.getState().zoom + delta);
  event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
}

void DocumentView::fitPage() {
  const pdf::PDFRenderKey
      base_key{layout_manager_.getAnchorPage(), 1.f, view_controller_.getState().rotate, recolor_};
  const auto page_dims = scheduler_.getPageSize(base_key);
  if (page_dims.x <= 0 || page_dims.y <= 0)
    return;

  const float viewable_h = window_size_.y - utils::cmdline_height_;
  const float viewable_w = window_size_.x;
  const float zoom_x = viewable_w / static_cast<float>(page_dims.x);
  const float zoom_y = viewable_h / static_cast<float>(page_dims.y);
  view_controller_.setZoom(std::min(zoom_x, zoom_y));
  event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
}

void DocumentView::fitWidth() {
  const pdf::PDFRenderKey
      base_key{layout_manager_.getAnchorPage(), 1.f, view_controller_.getState().rotate, recolor_};
  const auto page_dims = scheduler_.getPageSize(base_key);
  if (page_dims.x <= 0)
    return;

  view_controller_.setZoom(window_size_.x / static_cast<float>(page_dims.x));
  event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
}

void DocumentView::gotoPage(std::optional<std::size_t> one_based, std::size_t fallback) {
  if (!one_based) {
    onSwitchPage(fallback);
    return;
  }
  const std::size_t index = *one_based > 0 ? *one_based - 1 : 0;
  onSwitchPage(std::min(index, document_.pageCount() - 1));
}

void DocumentView::stepSearch(bool forward) {
  const std::size_t anchor_page = layout_manager_.getAnchorPage();
  const auto page = forward ? search_controller_.goNext(anchor_page)
                            : search_controller_.goPrev(anchor_page);
  if (page)
    onSwitchPage(*page);
  emitSearchState(1);
}

void DocumentView::emitSearchState(std::size_t display_offset) {
  const std::size_t curr = search_controller_.getCurrSearchResult();
  const std::size_t total = search_controller_.getTotalSearchResult();
  event_bus_.emit("statusbar.search_state", std::make_pair(curr + display_offset, total));
}

void DocumentView::addAnnotation(enum pdf_annot_type type, int color_index) {
  switch (color_index) {
  case 0:
    onAnnotate(
        document_.createAnnotations(type, utils::hexToRGB(settings::highlight_color_yellow_))
    );
    break;
  case 1:
    onAnnotate(
        document_.createAnnotations(type, utils::hexToRGB(settings::highlight_color_orange_))
    );
    break;
  case 2:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(settings::highlight_color_red_)));
    break;
  case 3:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(settings::highlight_color_blue_)));
    break;
  case 4:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(settings::highlight_color_green_)));
    break;
  default:
    break;
  }
}

void DocumentView::removeAnnotation() {
  onAnnotate(document_.removeAnnotations());
}

void DocumentView::onResize(const sf::Vector2f &size) {
  window_size_ = size;
  view_controller_.setWindowSize(size);
  layout_manager_.setWindowSize(size);
}

void DocumentView::onOpenDocument(const std::string &filepath) {
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

void DocumentView::onCloseDocument() {
  if (!document_.isOpen())
    return;

  resetView();
  document_.closeDocument();
  event_bus_.emit("statusbar.pdf_path", std::string("[No document open]"));
  event_bus_
      .emit("statusbar.page_state", std::make_pair(static_cast<std::size_t>(0), static_cast<std::size_t>(0)));
}

void DocumentView::onSwitchPage(int page_idx) {
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
  requestPage(anchor_page, view_state.zoom, view_state.rotate, recolor_);
  event_bus_.emit("statusbar.page_state", std::make_pair(anchor_page + 1, document_.pageCount()));
}

void DocumentView::onSearchPage(const std::u32string &text) {
  if (!document_.isOpen()) {
    event_bus_.emit("notification.msg", std::string("No document open to search"));
    return;
  }

  auto closest_page = search_controller_.search(text, layout_manager_.getAnchorPage(), document_);
  std::size_t curr = search_controller_.getCurrSearchResult();
  std::size_t total = search_controller_.getTotalSearchResult();

  if (!closest_page) {
    event_bus_.emit("statusbar.search_state", std::make_pair(curr, total));
    event_bus_.emit("notification.msg", std::string("No match found for: ") + utf8::utf32to8(text));
    return;
  }

  if (closest_page != static_cast<int>(layout_manager_.getAnchorPage()))
    onSwitchPage(closest_page.value());

  event_bus_.emit("statusbar.search_state", std::make_pair(curr + 1, total));
}

void DocumentView::onAnnotate(std::vector<std::size_t> pages) {
  if (!document_.isOpen())
    return;

  for (auto page : pages)
    pages_[page].reset();
  scheduler_.refreshPages(pages);
  selection_controller_.reset();
  event_bus_.emit("statusbar.document_edited", document_.hasChanges());
}

void DocumentView::registerKeybindings() {
  using Key = sf::Keyboard::Key;

  input_handler_.bind({{Key::H}}, DocumentAction::PanLeft);
  input_handler_.bind({{Key::L}}, DocumentAction::PanRight);
  input_handler_.bind({{Key::K}}, DocumentAction::PanUp);
  input_handler_.bind({{Key::J}}, DocumentAction::PanDown);

  input_handler_.bind({{Key::U}}, DocumentAction::PrevPage);
  input_handler_.bind({{Key::D}}, DocumentAction::NextPage);
  input_handler_.bind({{Key::U, true}}, DocumentAction::HalfPageUp);
  input_handler_.bind({{Key::D, true}}, DocumentAction::HalfPageDown);
  input_handler_.bind({{Key::G}, {Key::G}}, DocumentAction::FirstPage);
  input_handler_.bind({{Key::G, false, true}}, DocumentAction::LastPage);

  input_handler_.bind({{Key::Equal, true}}, DocumentAction::ZoomIn);
  input_handler_.bind({{Key::Hyphen, true}}, DocumentAction::ZoomOut);
  input_handler_.bind({{Key::F}}, DocumentAction::FitPage);
  input_handler_.bind({{Key::W}}, DocumentAction::FitWidth);

  input_handler_.bind({{Key::R}}, DocumentAction::RotateCW);
  input_handler_.bind({{Key::R, false, true}}, DocumentAction::RotateCCW);
  input_handler_.bind({{Key::I}}, DocumentAction::Recolor);

  input_handler_.bind({{Key::N}}, DocumentAction::SearchNext);
  input_handler_.bind({{Key::N, false, true}}, DocumentAction::SearchPrev);

  for (int i = 0; i <= 4; ++i) {
    const auto digit = static_cast<Key>(static_cast<int>(Key::Num1) + i);
    input_handler_.bind({{Key::A}, {Key::H}, {digit}}, DocumentAction::AnnotateHighlight, i);
    input_handler_.bind({{Key::A}, {Key::S}, {digit}}, DocumentAction::AnnotateStrikethrough, i);
    input_handler_.bind({{Key::A}, {Key::U}, {digit}}, DocumentAction::AnnotateUnderline, i);
  }
  input_handler_.bind({{Key::A}, {Key::D}}, DocumentAction::AnnotateRemove);

  input_handler_.bind({{Key::Escape}}, DocumentAction::Cancel);
}

void DocumentView::centerOnSearchResult(
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

void DocumentView::renderRequestedPages() {
  const VisualInfo view_state = view_controller_.getState();
  for (std::size_t i = layout_manager_.getFrontPage(); i <= layout_manager_.getBackPage(); i++) {
    pdf::PDFRenderKey target_key = {i, view_state.zoom, view_state.rotate, recolor_};
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

void DocumentView::requestPage(std::size_t page_idx, float zoom, int rotate, bool recolor) {
  if (!document_.isOpen())
    return;

  pdf::PDFRenderKey center_key{page_idx, zoom, rotate, 0};
  const sf::Vector2u page_dims = scheduler_.getPageSize(center_key);

  int total_height = static_cast<int>(page_dims.y);
  int front = static_cast<int>(page_idx);
  int back = static_cast<int>(page_idx);

  while (total_height <= window_size_.y) {
    bool expanded = false;

    if (back + 1 < static_cast<int>(document_.pageCount())) {
      back++;
      pdf::PDFRenderKey key{static_cast<size_t>(back), zoom, rotate, 0};
      total_height += scheduler_.getPageSize(key).y;
      expanded = true;
    }

    if (front > 0) {
      --front;
      pdf::PDFRenderKey key{static_cast<size_t>(front), zoom, rotate, 0};
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
    pdf::PDFRenderKey key{static_cast<std::size_t>(i), zoom, rotate, recolor};
    if (pages_[i].hasTexture() && pages_[i].getKey() == key)
      continue;

    scheduler_.request(key);
  }

  layout_manager_.setFrontPage(static_cast<std::size_t>(front));
  layout_manager_.setBackPage(static_cast<std::size_t>(back));
}

void DocumentView::resetView() {
  pages_.clear();
  pages_.resize(document_.pageCount(), dummy_);
  for (std::size_t i = 0; i < document_.pageCount(); i++) {
    const auto &bounds = document_.getPage(i).getPageBounds();
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
  selection_controller_.reset();
}

} // namespace ui
