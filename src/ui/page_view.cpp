#include "ui/page_view.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace ui {

PageView::PageView(const sf::Texture &dummy) : sprite_(dummy), key_(0, 0, 0, 0) {
  active_ = false;
  show_search_boxes_ = false;
  update_search_boxes_ = false;
  selected_search_result_ = -1;

  normal_color_ = utils::hexToRGBA(settings::search_box_color_);
  selected_color_ = utils::hexToRGBA(settings::search_highlight_color_);
};

void PageView::draw(sf::RenderTarget &window) const {
  if (!active_)
    return;

  window.draw(sprite_);
  // window.draw(original_shape_);

  if (!show_search_boxes_)
    return;

  window.draw(context_boxes_, sf::RenderStates(original_shape_.getTransform()));
}

void PageView::update() {
  if (show_search_boxes_ && update_search_boxes_) {
    context_boxes_.clear();

    for (std::size_t i = 0; i < search_result_.local_rects.size(); i++) {
      const auto &rect = search_result_.local_rects[i];
      const sf::Color &color = (i == selected_search_result_) ? selected_color_ : normal_color_;

      sf::Vector2f tl = {rect.x0, rect.y0};
      sf::Vector2f tr = {rect.x1, rect.y0};
      sf::Vector2f br = {rect.x1, rect.y1};
      sf::Vector2f bl = {rect.x0, rect.y1};

      context_boxes_.append({tl, color});
      context_boxes_.append({tr, color});
      context_boxes_.append({br, color});
      context_boxes_.append({tl, color});
      context_boxes_.append({br, color});
      context_boxes_.append({bl, color});
    }

    update_search_boxes_ = false;
  }
}

sf::Sprite &PageView::getSprite() {
  return sprite_;
}

void PageView::move(const sf::Vector2f &delta) {
  sprite_.move(delta);
}

void PageView::setPosition(const sf::Vector2f &position) {
  sprite_.setPosition(position);
}

sf::Vector2f PageView::getPosition() const {
  return sprite_.getPosition();
}

sf::FloatRect PageView::getGlobalBounds() const {
  return sprite_.getGlobalBounds();
}

void PageView::setRotation(int rot) {
  sprite_.setRotation(sf::degrees(rot * 90.f));
}

void PageView::setScale(const sf::Vector2f &scale) {
  sprite_.setScale(scale);
}

sf::Vector2u PageView::getTextureSize() const {
  return sprite_.getTexture().getSize();
}

void PageView::setKey(const pdf::PDFRenderKey &key) {
  key_ = key;
}

pdf::PDFRenderKey PageView::getKey() const {
  return key_;
}

void PageView::setTexture(const sf::Texture &texture) {
  sprite_.setTexture(texture, true);
  const auto bounds = sprite_.getLocalBounds();
  sprite_.setOrigin({bounds.size.x * 0.5f, bounds.size.y * 0.5f});
  active_ = true;
}

void PageView::reset() {
  active_ = false;
}

const PDFSearchResult &PageView::getSearchResults() {
  return search_result_;
}

sf::Vector2f PageView::getSearchResultPosition(std::size_t idx) {
  const auto &rect = search_result_.local_rects[idx];
  sf::Vector2f center = {(rect.x0 + rect.x1) * 0.5f, (rect.y0 + rect.y1) * 0.5f};
  return original_shape_.getTransform().transformPoint(center);
}

void PageView::setSelectedSearchResult(std::size_t idx) {
  selected_search_result_ = idx;
  update_search_boxes_ = true;
}

void PageView::setSearchResults(PDFSearchResult search_results) {
  search_result_ = std::move(search_results);
  context_boxes_.resize(search_result_.local_rects.size());
}

void PageView::clearSearchResults() {
  search_result_.local_rects.clear();
  context_boxes_.clear();
  selected_search_result_ = -1;
}

void PageView::showSearchResults() {
  show_search_boxes_ = true;
}

void PageView::hideSearchResults() {
  show_search_boxes_ = false;
}

void PageView::setPageShapeSize(sf::Vector2f size) {
  original_shape_.setSize(size);
  original_shape_.setOrigin({size.x * 0.5f, size.y * 0.5f});
  original_shape_.setFillColor(sf::Color::Black);
}

void PageView::syncPageShapePos() {
  original_shape_.setPosition(sprite_.getPosition());
  update_search_boxes_ = true;
}

void PageView::syncPageShape(int rot) {
  original_shape_.setRotation(sf::degrees(rot * 90.f));

  const auto &page_size_raw = sprite_.getGlobalBounds().size;
  const bool swapped = (rot == 1 || rot == 3);
  const sf::Vector2f page_size = swapped ? sf::Vector2f{page_size_raw.y, page_size_raw.x}
                                         : page_size_raw;

  const auto shape_size = original_shape_.getSize();

  const auto delta = page_size_raw - shape_size;
  if (std::abs(delta.x) <= 1e-3 && std::abs(delta.y) <= 1e-3)
    return;

  original_shape_.setScale({page_size.x / shape_size.x, page_size.y / shape_size.y});
  update_search_boxes_ = true;
}

bool PageView::hasTexture() const {
  return active_;
}

bool PageView::isInView(const sf::Vector2f &window_size) const {
  const auto bounds = sprite_.getGlobalBounds();

  float top = bounds.position.y;
  float bottom = bounds.position.y + bounds.size.y;

  return !(top > window_size.y || bottom < 0.f);
}

} // namespace ui
