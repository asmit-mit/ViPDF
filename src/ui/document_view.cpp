#include "ui/document_view.h"

#include <algorithm>
#include <clip.h>
#include <cmath>
#include <stdexcept>
#include <utf8.h>

#include "core/global_marks.h"
#include "core/input_handler.h"
#include "pdf/pdf_renderer.h"
#include "pdf/pdf_selection_controller.h"
#include "ui/ui_elements.h"
#include "utils/constants.h"
#include "utils/filesystem.h"
#include "utils/general.h"
#include "utils/render.h"

namespace ui {

DocumentView::DocumentView(
    pdf::PDFDocument &document,
    core::HistoryManager &file_history,
    core::BookmarkManager &bookmark_manager,
    core::GlobalMarks &global_marks,
    core::RenderScheduler &scheduler,
    core::EventBus &event_bus,
    const core::Config &config
)
    : document_(document), layout_manager_(scheduler, pages_), view_controller_(scheduler, pages_),
      search_controller_(pages_), selection_controller_(document_, pages_), config_(config),
      event_bus_(event_bus), file_history_(file_history), bookmark_manager_(bookmark_manager),
      global_marks_(global_marks), scheduler_(scheduler),
      selection_box_color_(utils::hexToRGBA(config.selection_highlight_color)),
      search_box_color_(utils::hexToRGBA(config.search_box_color)),
      selected_search_box_color_(utils::hexToRGBA(config.search_highlight_color)) {
  should_take_input_ = true;
  recolor_ = config_.default_recolor;
  update_recolor_ = false;
  selecting_ = false;
  focus_update_ = false;

  registerKeybindings();
  registerEvents();
}

void DocumentView::draw(sf::RenderTarget &window) const {
  if (!canWorkWithDocument())
    return;

  if (!layout_manager_.needInitialPos())
    pages_[layout_manager_.getAnchorPage()].draw(window);

  if (layout_manager_.isLayoutPending())
    return;

  for (std::size_t i = layout_manager_.getFrontPage(); i <= layout_manager_.getBackPage(); i++) {
    if (i != layout_manager_.getAnchorPage())
      pages_[i].draw(window);
  }
}

void DocumentView::update() {
  if (!canWorkWithDocument())
    return;

  // pending updates
  if (pending_zoom_update_) {
    view_controller_.setZoom(*pending_zoom_update_, false);
    pending_zoom_update_.reset();
  }

  if (pending_rotate_update_) {
    view_controller_.setRotate(*pending_rotate_update_, false);
    pending_rotate_update_.reset();
  }

  if (pending_page_swtich_) {
    onSwitchPage(*pending_page_swtich_);
    pending_page_swtich_.reset();
  }

  std::size_t front_page = layout_manager_.getFrontPage();
  std::size_t anchor_page = layout_manager_.getAnchorPage();
  std::size_t back_page = layout_manager_.getBackPage();
  const VisualInfo view_state = view_controller_.getState();

  // instantly update colors
  if (update_recolor_) {
    recolor_ = !recolor_;
    requestVisiblePages(layout_manager_.getAnchorPage(), view_state.zoom, view_state.rotate, recolor_);
    update_recolor_ = false;
  }

  // make new selections
  if (selection_controller_.isSelectionDirty())
    selection_controller_.createSelection();

  // update zoom and rotation if timer up
  if (view_controller_.canUpdatePages()) {
    requestVisiblePages(layout_manager_.getAnchorPage(), view_state.zoom, view_state.rotate, recolor_);
    event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
    view_controller_.clearPendingUpdates();
  }

  // render requested pages
  renderRequestedPages();

  // visually change page sprite shapEncrypt and protect PDF with a passwe and size via setScale and
  // setRotate before requesting for page texture with higher resolution and equivalent scale and
  // rotation until timer runs out
  if (view_controller_.isSyncDirty()) {
    view_controller_.syncWithTargetState(front_page, anchor_page, back_page);
    layout_manager_.setPagePosDirty();
  }

  // set base positions for anchor pages and neighbors
  // (after page switching, or window size change)
  layout_manager_.setInitialPagePos(view_state);

  // request for page panning for new search result focus
  if (search_controller_.isSearchPosDirty()) {
    pending_page_scroll_ = getSearchResultPos(*search_controller_.getSelectedPage());
    scroll_to_ = ScrollTo::SEARCH;
    search_controller_.clearSearchPosDirty();
  }

  // pan pages and update neighbors *only after base anchor position is set*
  if (pending_page_scroll_ && pages_[anchor_page].hasTexture() &&
      !layout_manager_.needInitialPos()) {
    pages_[anchor_page].syncPageShapePos();
    pages_[anchor_page].syncPageShape();

    switch (scroll_to_) {
    case ScrollTo::RELOAD:
    case ScrollTo::BOOKMARK:
    case ScrollTo::LINK:
      layout_manager_.panCurrentPage(-pages_[anchor_page].transformPos(*pending_page_scroll_));
      break;
    case ScrollTo::SEARCH:
      layout_manager_.panCurrentPage(
          window_size_ * 0.5f - pages_[anchor_page].transformPos(*pending_page_scroll_)
      );
      break;
    default:
      break;
    }
    pending_page_scroll_.reset();
  }

  // update all pages after initial page setup
  // (clamp, neighbor page positions etc...)
  layout_manager_.updatePagePositions(view_state);

  // update anchor page and request for
  // new pages if anchor page is changed due to scrolling
  layout_manager_.updateAnchorPage();
  anchor_page = layout_manager_.getAnchorPage();
  if (layout_manager_.anchorPageChanged()) {
    requestVisiblePages(anchor_page, view_state.zoom, view_state.rotate, recolor_);
    event_bus_.emit("statusbar.page_state", std::make_pair(anchor_page + 1, document_.pageCount()));
  }

  // update shape and position of anchor page sprits's reference shape
  for (std::size_t i = front_page; i <= back_page; i++) {
    pages_[i].syncPageShapePos();
    pages_[i].syncPageShape();
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
  if (!canWorkWithDocument())
    return;

  const std::size_t front_page = layout_manager_.getFrontPage();
  const std::size_t back_page = layout_manager_.getBackPage();

  if (should_take_input_) {
    if (const auto cmd = input_handler_.handleEvent(event)) {
      if (!isPageSwitchAction(cmd->action)) {
        execute(*cmd);
      } else if (page_switch_clock_.getElapsedTime() >= page_switch_delay_) {
        page_switch_clock_.restart();
        execute(*cmd);
      }
    }

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
      document_.loadPageContent(i);

      if (!pages_[i].containsMouse(move->position))
        continue;

      if (selecting_)
        selection_controller_.setSelectionEnd(i, move->position);

      const auto point = pages_[i].inverseTransformPos(sf::Vector2f(move->position));

      if (document_.getPage(i).mouseInLink(point))
        cursor = ui::CursorType::Hand;
      else if (document_.getPage(i).mouseInText(point))
        cursor = ui::CursorType::Text;

      break;
    }

    event_bus_.emit("ui.cursor_type", cursor);
  }

  if (const auto *mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
    const auto location = selection_controller_.getSelectionStart();

    if (selection_controller_.isSelectionEmpty()) {
      const auto link = document_.getPage(location.first).mouseInLink(location.second);
      const auto img = document_.getPage(location.first).mouseInImage(location.second);

      if (link) {
        if (link->type == pdf::PDFLinkType::INTERNAL) {
          pending_page_swtich_ = link->target_page;
          if (!std::isnan(link->x) && !std::isnan(link->y)) {
            pending_page_scroll_ = {link->x, link->y};
            scroll_to_ = ScrollTo::LINK;
          }
        } else {
          event_bus_.emit("link.open_external", link->url);
          event_bus_.emit("ui.focus", ui::UIElements::ExternalLinkDialog);
        }
      }

      if (img) {
        selection_controller_.setSelectedImage(img);
        event_bus_.emit("notification.msg", std::string("Image has been selected"));
      }
    }

    if (mouse->button == sf::Mouse::Button::Left)
      selecting_ = false;
  }
}

void DocumentView::handleScroll(const sf::Event::MouseWheelScrolled &scroll) {
  if (scroll.wheel == sf::Mouse::Wheel::Vertical) {
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl))
      zoomBy(scroll.delta * config_.delta_zoom);
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
    pending_page_swtich_ = anchor_page > times ? anchor_page - times : 0;
    break;
  case DocumentAction::NextPage:
    pending_page_swtich_ = std::min(last_page, anchor_page + times);
    break;
  case DocumentAction::FirstPage:
    gotoPage(cmd.count, 0);
    break;
  case DocumentAction::LastPage:
    gotoPage(cmd.count, last_page);
    break;

  case DocumentAction::ZoomIn:
    zoomBy(config_.delta_zoom);
    break;
  case DocumentAction::ZoomOut:
    zoomBy(-config_.delta_zoom);
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

  case DocumentAction::CopySelection:
    copySelection();
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
    search_controller_.hideSearchResults();
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

  const float viewable_h = window_size_.y - utils::CMDLINE_HEIGHT;
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
    pending_page_swtich_ = fallback;
    return;
  }
  const std::size_t index = *one_based > 0 ? *one_based - 1 : 0;
  pending_page_swtich_ = std::min(index, document_.pageCount() - 1);
}

void DocumentView::stepSearch(bool forward) {
  const std::size_t anchor_page = layout_manager_.getAnchorPage();
  const auto page = forward ? search_controller_.goNext(anchor_page)
                            : search_controller_.goPrev(anchor_page);
  if (page)
    pending_page_swtich_ = page;
  emitSearchState(1);
}

void DocumentView::copySelection() {
  if (!selection_controller_.isShowingSelection())
    return;

  if (selection_controller_.isSelectionEmpty()) {
    fz_image *img = selection_controller_.getSelectedImage();
    if (img) {
      const bool ok = utils::copyImageToClipboard(document_.cloneContext().get(), img);
      event_bus_
          .emit("notification.msg", std::string(ok ? "Copied image to clipboard" : "Failed to copy image"));
    }
  } else {
    const std::string text = selection_controller_.copySelection();
    if (!text.empty()) {
      clip::set_text(text);
      event_bus_.emit("notification.msg", std::string("Copied selected text to clipboard"));
    }
  }

  selection_controller_.reset();
}

void DocumentView::emitSearchState(std::size_t display_offset) {
  const std::size_t curr = search_controller_.getCurrSearchResult();
  const std::size_t total = search_controller_.getTotalSearchResult();
  event_bus_.emit("statusbar.search_state", std::make_pair(curr + display_offset, total));
}

void DocumentView::addAnnotation(enum pdf_annot_type type, int color_index) {
  switch (color_index) {
  case 0:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(config_.highlight_color_1)));
    break;
  case 1:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(config_.highlight_color_2)));
    break;
  case 2:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(config_.highlight_color_3)));
    break;
  case 3:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(config_.highlight_color_4)));
    break;
  case 4:
    onAnnotate(document_.createAnnotations(type, utils::hexToRGB(config_.highlight_color_5)));
    break;
  default:
    break;
  }
}

void DocumentView::removeAnnotation() {
  if (!selection_controller_.isSelectionEmpty())
    return;

  const auto location = selection_controller_.getSelectionStart();
  if (document_.removeAnnotations(location.first, location.second))
    onAnnotate({location.first});
}

void DocumentView::onResize(const sf::Vector2f &size) {
  window_size_ = size;
  view_controller_.setWindowSize(size);
  layout_manager_.setWindowSize(size);
}

void DocumentView::onOpenDocument(const std::string &filepath) {
  std::string absolute_path = utils::resolvePath(filepath);
  scheduler_.quiesce();
  try {
    document_.openDocument(absolute_path);
    scheduler_.clearCache();

    resetView();
    pending_zoom_update_.reset();
    pending_rotate_update_.reset();
    pending_page_swtich_ = 0;

    event_bus_.emit("statusbar.pdf_path", absolute_path);
    event_bus_.emit("statusbar.page_state", std::make_pair(0, document_.pageCount()));
    event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);

    file_history_.add(absolute_path);
    bookmark_manager_.setDocumentPath(absolute_path);
  } catch (const std::runtime_error &e) {
    scheduler_.resume();
    onCloseDocument();
    event_bus_.emit("notification.msg", std::string(e.what()));
  }

  scheduler_.resume();
}

void DocumentView::onCloseDocument() {
  if (!document_.isOpen())
    return;

  resetView();
  document_.closeDocument();
  bookmark_manager_.resetPath();
  event_bus_.emit("statusbar.pdf_path", std::string("[No document open]"));
  event_bus_
      .emit("statusbar.page_state", std::make_pair(static_cast<std::size_t>(0), static_cast<std::size_t>(0)));
}

void DocumentView::onSwitchPage(int page_idx) {
  if (!canWorkWithDocument()) {
    event_bus_.emit("notification.msg", std::string("No document currently open"));
    return;
  }

  if (page_idx < 0 || page_idx >= static_cast<int>(document_.pageCount())) {
    event_bus_.emit("notification.msg", std::string("Page number out of range"));
    return;
  }

  std::size_t anchor_page = static_cast<std::size_t>(page_idx);
  VisualInfo view_state = view_controller_.getState();
  requestVisiblePages(anchor_page, view_state.zoom, view_state.rotate, recolor_);
  layout_manager_.setAnchorPage(anchor_page);
  event_bus_.emit("statusbar.page_state", std::make_pair(anchor_page + 1, document_.pageCount()));
}

void DocumentView::onSearchPage(const std::u32string &text) {
  if (!canWorkWithDocument()) {
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
    pending_page_swtich_ = (*closest_page);

  event_bus_.emit("statusbar.search_state", std::make_pair(curr + 1, total));
}

void DocumentView::onAnnotate(std::vector<std::size_t> pages) {
  if (!canWorkWithDocument())
    return;

  for (auto page : pages)
    pages_[page].reset(false);
  scheduler_.refreshPages(pages);
  selection_controller_.reset();
  event_bus_.emit("statusbar.document_edited", document_.hasChanges());
}

void DocumentView::onAuthenticate() {
  resetView();

  event_bus_.emit("ui.focus", ui::UIElements::PDFView);
  event_bus_.emit("statusbar.page_state", std::make_pair(0, document_.pageCount()));
  event_bus_.emit("statusbar.page_zoom", view_controller_.getState().zoom);
}

void DocumentView::registerEvents() {
  event_bus_.subscribe<std::string>("cmd.open_document", [this](const std::string &filepath) {
    onOpenDocument(filepath);
  });

  event_bus_.subscribe<bool>("cmd.reload_document", [this](bool) {
    if (!document_.isOpen()) {
      event_bus_.emit("notification.msg", std::string("No document open to reload"));
      return;
    }

    const auto view_state = view_controller_.getState();
    std::size_t curr_page = layout_manager_.getAnchorPage();
    const auto scroll = getPageOffsetFromCenter(curr_page);
    onOpenDocument(document_.getFilePath());
    pending_zoom_update_ = view_state.zoom;
    pending_rotate_update_ = view_state.rotate % 4;
    pending_page_swtich_ = curr_page;
    pending_page_scroll_ = scroll;
    scroll_to_ = ScrollTo::RELOAD;
    event_bus_.emit("statusbar.document_edited", document_.hasChanges());
  });

  event_bus_.subscribe<bool>("cmd.close_document", [this](bool) { onCloseDocument(); });

  event_bus_.subscribe<int>("cmd.switch_page", [this](int page_num) {
    pending_page_swtich_ = page_num - 1;
  });

  event_bus_.subscribe<bool>("cmd.hl", [this](bool) { search_controller_.showSearchResults(); });
  event_bus_.subscribe<bool>("cmd.nohl", [this](bool) { search_controller_.hideSearchResults(); });

  event_bus_.subscribe<std::string>("password.submit", [this](std::string password) {
    if (document_.authenticate(password))
      onAuthenticate();
    else
      event_bus_.emit("ui.focus", ui::UIElements::PasswordDialog);
  });

  event_bus_.subscribe<const std::string &>("mark.add", [this](const std::string &label) {
    if (!canWorkWithDocument()) {
      event_bus_.emit("notification.msg", std::string("No document currently open"));
      return;
    }

    const auto anchor = layout_manager_.getAnchorPage();
    const auto local_pos = pages_[anchor].inverseTransformPos({0.f, 0.f});

    const auto state = view_controller_.getState();
    const bool ok = global_marks_.addMark(
        label, document_.getFilePath(), anchor + 1, local_pos.x, local_pos.y, state.zoom, state.rotate
    );

    if (ok)
      event_bus_.emit("notification.msg", std::format("Global mark \"{}\" added", label));
    else
      event_bus_
          .emit("notification.msg", std::format("Global mark with name \"{}\" already exists", label));
  });

  event_bus_.subscribe<const std::string &>("mark.del", [this](const std::string &name) {
    const bool ok = global_marks_.remove(name);
    if (ok)
      event_bus_.emit("notification.msg", std::format("Global mark \"{}\" deleted", name));
    else
      event_bus_.emit("notification.msg", std::format("Global mark \"{}\" does not exist", name));
  });

  event_bus_.subscribe<const std::string &>("mark.jump", [this](const std::string &name) {
    const auto mark = global_marks_.getMark(name);
    if (!mark) {
      event_bus_.emit("notification.msg", std::format("Global mark \"{}\" does not exist", name));
      return;
    }

    if (document_.isOpen() && document_.getFilePath() == mark->doc_path) {
      pending_page_swtich_ = mark->page - 1;
      pending_page_scroll_ = {mark->x, mark->y};
      scroll_to_ = ScrollTo::BOOKMARK;
    } else {
      onOpenDocument(mark->doc_path);
      pending_zoom_update_ = mark->zoom;
      pending_rotate_update_ = mark->rotate % 4;
      pending_page_swtich_ = mark->page - 1;
      pending_page_scroll_ = {mark->x, mark->y};
      scroll_to_ = ScrollTo::BOOKMARK;
    }
  });

  event_bus_.subscribe<bool>("mark.clear", [this](bool) { global_marks_.clear(); });

  event_bus_.subscribe<const std::string &>("bmark.add", [this](const std::string &label) {
    if (!canWorkWithDocument()) {
      event_bus_.emit("notification.msg", std::string("No document currently open"));
      return;
    }

    const auto anchor = layout_manager_.getAnchorPage();
    const auto local_pos = pages_[anchor].inverseTransformPos({0.f, 0.f});

    const auto state = view_controller_.getState();
    const std::string display_label = label.empty() ? "Page " + std::to_string(anchor + 1) : label;

    auto res =
        bookmark_manager_
            .addBookmark(anchor + 1, local_pos.x, local_pos.y, state.zoom, state.rotate, display_label);

    if (res == core::BookmarkManager::AddResult::DuplicateLabel)
      event_bus_
          .emit("notification.msg", std::format("Bookmark with name \"{}\" already exists", display_label));
    else if (res == core::BookmarkManager::AddResult::Ok)
      event_bus_.emit("notification.msg", std::format("Bookmark \"{}\" added", display_label));
  });

  event_bus_.subscribe<const std::string &>("bmark.del", [this](const std::string &label) {
    if (!canWorkWithDocument()) {
      event_bus_.emit("notification.msg", std::string("No document currently open"));
      return;
    }

    if (bookmark_manager_.removeBookmark(label))
      event_bus_.emit("notification.msg", std::format("Bookmark \"{}\" deleted", label));
    else
      event_bus_.emit("notification.msg", std::format("Bookmark \"{}\" does not exist", label));
  });

  event_bus_.subscribe<bool>("bmark.clear", [this](bool) {
    if (!canWorkWithDocument()) {
      event_bus_.emit("notification.msg", std::string("No document currently open"));
      return;
    }

    const int count = bookmark_manager_.removeAllBookmarks();
    event_bus_.emit("notification.msg", std::format("Removed {} bookmarks from document", count));
  });

  event_bus_.subscribe<const std::string &>("bmark.jump", [this](const std::string &label) {
    if (!canWorkWithDocument()) {
      event_bus_.emit("notification.msg", std::string("No document currently open"));
      return;
    }

    const auto bmark = bookmark_manager_.getBookmarkByLabel(label);
    if (!bmark) {
      event_bus_.emit("notification.msg", std::format("Bookmark \"{}\" does not exist", label));
      return;
    }

    pending_page_swtich_ = bmark->page - 1;
    pending_page_scroll_ = {bmark->x, bmark->y};
    pending_zoom_update_ = bmark->zoom;
    pending_rotate_update_ = bmark->rotate;
    scroll_to_ = ScrollTo::BOOKMARK;
  });

  event_bus_.subscribe<const std::u32string &>("cmd.search", [this](const std::u32string &text) {
    onSearchPage(text);
  });

  event_bus_.subscribe<ui::UIElements>("ui.focus", [this](ui::UIElements focus) {
    bool prev = should_take_input_;
    should_take_input_ = focus == ui::UIElements::PDFView;
    focus_update_ = prev != should_take_input_;
  });
}

void DocumentView::registerKeybindings() {
  using Key = sf::Keyboard::Key;

  input_handler_.bind({{Key::H}}, DocumentAction::PanLeft);
  input_handler_.bind({{Key::L}}, DocumentAction::PanRight);
  input_handler_.bind({{Key::K}}, DocumentAction::PanUp);
  input_handler_.bind({{Key::J}}, DocumentAction::PanDown);

  input_handler_.bind({{Key::Left}}, DocumentAction::PanLeft);
  input_handler_.bind({{Key::Right}}, DocumentAction::PanRight);
  input_handler_.bind({{Key::Up}}, DocumentAction::PanUp);
  input_handler_.bind({{Key::Down}}, DocumentAction::PanDown);

  input_handler_.bind({{Key::U}}, DocumentAction::PrevPage);
  input_handler_.bind({{Key::D}}, DocumentAction::NextPage);

  input_handler_.bind({{Key::PageUp}}, DocumentAction::PrevPage);
  input_handler_.bind({{Key::PageDown}}, DocumentAction::NextPage);

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

  input_handler_.bind({{Key::Y}}, DocumentAction::CopySelection);
  input_handler_.bind({{Key::C, true, false}}, DocumentAction::CopySelection);

  for (int i = 0; i <= 4; ++i) {
    const auto digit = static_cast<Key>(static_cast<int>(Key::Num1) + i);
    input_handler_.bind({{Key::A}, {Key::H}, {digit}}, DocumentAction::AnnotateHighlight, i);
    input_handler_.bind({{Key::A}, {Key::S}, {digit}}, DocumentAction::AnnotateStrikethrough, i);
    input_handler_.bind({{Key::A}, {Key::U}, {digit}}, DocumentAction::AnnotateUnderline, i);
  }
  input_handler_.bind({{Key::A}, {Key::D}}, DocumentAction::AnnotateRemove);

  input_handler_.bind({{Key::Escape}}, DocumentAction::Cancel);
}

bool DocumentView::canWorkWithDocument() const {
  return document_.isOpen() && !document_.needsAuth();
}

sf::Vector2f DocumentView::getSearchResultPos(std::size_t anchor_page) const {
  const sf::Vector2f pos = pages_[anchor_page].getSearchResultPosition(
      search_controller_.getLocalSearchResult()
  );
  return pos;
}

sf::Vector2f DocumentView::getPageOffsetFromCenter(std::size_t page_idx) const {
  const auto &page = pages_[page_idx];
  if (!page.hasTexture())
    return {0.f, 0.f};

  return pages_[page_idx].inverseTransformPos({0.f, 0.f});
}

void DocumentView::renderRequestedPages() {
  const VisualInfo view_state = view_controller_.getState();
  for (std::size_t i = layout_manager_.getFrontPage(); i <= layout_manager_.getBackPage(); i++) {
    pdf::PDFRenderKey target_key = {i, view_state.zoom, view_state.rotate, recolor_};
    if (pages_[i].hasTexture() && pages_[i].getKey() == target_key) {
      pages_[i].setScale({1.f, 1.f});
      continue;
    }

    auto texture = scheduler_.getTexture(target_key);
    if (!texture) {
      if (!view_controller_.isUpdatePending())
        scheduler_.request(target_key);
      continue;
    }

    pages_[i].setTexture(std::move(texture));
    pages_[i].setKey(target_key);

    pages_[i].setScale({1.f, 1.f});
    pages_[i].setRotation(0);

    view_controller_.setSyncDirty();
    layout_manager_.setPagePosDirty();
  }
}

void DocumentView::requestVisiblePages(std::size_t anchor, float zoom, int rotate, bool recolor) {
  if (!canWorkWithDocument())
    return;

  scheduler_.request({anchor, zoom, rotate, recolor});
  const PageRange visible = computeVisibleRange(anchor, zoom, rotate);
  const PageRange retained = computeRetainedRange(visible);

  releaseOutOfWindowPages(visible, retained, {anchor, zoom, rotate, recolor});
  kept_range_ = retained;

  auto requestIfStale = [&](int i) {
    const pdf::PDFRenderKey key{static_cast<std::size_t>(i), zoom, rotate, recolor};
    if (pages_[i].hasTexture() && pages_[i].getKey() == key)
      return;
    scheduler_.request(key);
  };

  const int anchor_idx = static_cast<int>(anchor);
  for (int d = 1; anchor_idx + d <= visible.back || anchor_idx - d >= visible.front; ++d) {
    if (anchor_idx + d <= visible.back)
      requestIfStale(anchor_idx + d);
    if (anchor_idx - d >= visible.front)
      requestIfStale(anchor_idx - d);
  }

  layout_manager_.setFrontPage(static_cast<std::size_t>(visible.front));
  layout_manager_.setBackPage(static_cast<std::size_t>(visible.back));
}

DocumentView::PageRange
DocumentView::computeVisibleRange(std::size_t anchor, float zoom, int rotate) const {
  const int page_count = static_cast<int>(document_.pageCount());

  auto heightOf = [&](int page) {
    return static_cast<int>(
        scheduler_.getPageSize({static_cast<std::size_t>(page), zoom, rotate, false}).y
    );
  };

  PageRange range{static_cast<int>(anchor), static_cast<int>(anchor)};
  int total_height = heightOf(range.front);

  while (total_height <= window_size_.y) {
    bool expanded = false;

    if (range.back + 1 < page_count) {
      total_height += heightOf(++range.back);
      expanded = true;
    }
    if (range.front > 0) {
      total_height += heightOf(--range.front);
      expanded = true;
    }
    if (!expanded)
      break;
  }

  range.back = std::min(range.back + prefetch_count_, page_count - 1);
  range.front = std::max(range.front - prefetch_count_, 0);
  return range;
}

DocumentView::PageRange DocumentView::computeRetainedRange(PageRange visible) const {
  const int page_count = static_cast<int>(document_.pageCount());
  const int budget = static_cast<int>(std::max<std::size_t>(config_.page_cache_size, 1));

  PageRange range = visible;
  while (range.size() < budget) {
    bool expanded = false;

    if (range.back + 1 < page_count) {
      ++range.back;
      expanded = true;
    }
    if (range.front > 0 && range.size() < budget) {
      --range.front;
      expanded = true;
    }
    if (!expanded)
      break;
  }
  return range;
}

void DocumentView::releaseOutOfWindowPages(
    PageRange visible, PageRange retained, const pdf::PDFRenderKey &target
) {
  if (!kept_range_)
    return;

  const int lo = std::max(kept_range_->front, 0);
  const int hi = std::min(kept_range_->back, static_cast<int>(pages_.size()) - 1);

  for (int i = lo; i <= hi; ++i) {
    auto &page = pages_[i];

    if (!retained.contains(i)) {
      page.reset();
      continue;
    }

    const pdf::PDFRenderKey
        key{static_cast<std::size_t>(i), target.zoom, target.rotate, target.recolor};
    if (!visible.contains(i) && page.hasTexture() && !(page.getKey() == key))
      page.reset();
  }
}

void DocumentView::resetView() {
  if (!canWorkWithDocument())
    return;

  pages_.clear();
  for (std::size_t i = 0; i < document_.pageCount(); i++) {
    pages_.emplace_back(dummy_, selection_box_color_, search_box_color_, selected_search_box_color_);
    const auto &bounds = document_.getPage(i).getPageBounds();
    float width = bounds.x1 - bounds.x0;
    float height = bounds.y1 - bounds.y0;
    pages_[i].setPageShapeSize({width, height});
  }
  pages_.shrink_to_fit();

  layout_manager_.reset();
  view_controller_.reset();
  search_controller_.reset();
  selection_controller_.reset();

  layout_manager_.setPageWithMaxWidth(document_.pageWithMaxWidth());

  emitSearchState(0);
  event_bus_.emit("statusbar.pending_keys", std::string(""));
}

bool DocumentView::isPageSwitchAction(DocumentAction action) {
  switch (action) {
  case DocumentAction::NextPage:
  case DocumentAction::PrevPage:
  case DocumentAction::HalfPageUp:
  case DocumentAction::HalfPageDown:
  case DocumentAction::FirstPage:
  case DocumentAction::LastPage:
    return true;
  default:
    return false;
  }
}

} // namespace ui
