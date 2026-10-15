#include "ui/page_view.h"
#include "utils/settings.h"
#include "utils/utils.h"

namespace ui {

PageView::PageView(const sf::Texture &dummy) : sprite_(dummy), key_(0, 0, 0, 0) {
  active_ = false;
  update_search_boxes_ = false;
  selected_search_result_ = -1;

  normal_color_ = utils::hexToRGBA(settings::search_box_color_);
  selected_color_ = utils::hexToRGBA(settings::search_highlight_color_);
};

void PageView::draw(sf::RenderTarget &window) const {
  if (!active_)
    return;

  // window.draw(original_shape_);
  window.draw(sprite_);

  if (!search_result_.local_rects.empty())
    window.draw(search_boxes_, sf::RenderStates(original_shape_.getTransform()));

  if (selection_boxes_.getVertexCount() != 0)
    window.draw(selection_boxes_, sf::RenderStates(original_shape_.getTransform()));
}

void PageView::update() {
  if (update_search_boxes_) {
    search_boxes_.clear();

    for (std::size_t i = 0; i < search_result_.local_rects.size(); i++) {
      const auto &res = search_result_.local_rects[i];
      const sf::Color &color = (i == selected_search_result_) ? selected_color_ : normal_color_;

      for (std::size_t j = 0; j < res.size(); j++) {
        const auto &rect = search_result_.local_rects[i][j];
        sf::Vector2f tl = {rect.x0, rect.y0};
        sf::Vector2f tr = {rect.x1, rect.y0};
        sf::Vector2f br = {rect.x1, rect.y1};
        sf::Vector2f bl = {rect.x0, rect.y1};

        search_boxes_.append({tl, color});
        search_boxes_.append({tr, color});
        search_boxes_.append({br, color});
        search_boxes_.append({tl, color});
        search_boxes_.append({br, color});
        search_boxes_.append({bl, color});
      }
    }

    update_search_boxes_ = false;
  }
}

sf::Sprite &PageView::getSprite() {
  return sprite_;
}

sf::RectangleShape &PageView::getReferenceShape() {
  return original_shape_;
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
  const auto &boxes = search_result_.local_rects[idx];
  sf::Vector2f center = {(boxes[0].x0 + boxes[0].x1) * 0.5f, (boxes[0].y0 + boxes[0].y1) * 0.5f};
  return original_shape_.getTransform().transformPoint(center);
}

void PageView::setSelectedSearchResult(std::size_t idx) {
  selected_search_result_ = idx;
  update_search_boxes_ = true;
}

void PageView::setSearchResults(PDFSearchResult search_results) {
  search_result_ = std::move(search_results);
  update_search_boxes_ = true;
}

void PageView::clearSearchResults() {
  search_result_.local_rects.clear();
  update_search_boxes_ = true;
  selected_search_result_ = -1;
}

void PageView::setSelections(sf::VertexArray selection_boxes) {
  selection_boxes_ = std::move(selection_boxes);
}

void PageView::clearSelections() {
  selection_boxes_.clear();
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

bool PageView::containsMouse(const sf::Vector2i &mouse_pos) const {
  return sprite_.getGlobalBounds().contains(sf::Vector2f(mouse_pos));
}

} // namespace ui
