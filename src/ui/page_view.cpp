#include "ui/page_view.h"

namespace ui {

PageView::PageView(
    const sf::Texture &dummy,
    const sf::Color selection_color,
    const sf::Color unselected_color,
    const sf::Color selected_color
)
    : sprite_(dummy), key_(0, 0, 0, 0), selection_color_(selection_color),
      unselected_color_(unselected_color), selected_color_(selected_color) {
  active_ = false;
  update_search_boxes_ = false;
  selected_search_result_ = -1;
  show_search_results_ = false;
};

void PageView::draw(sf::RenderTarget &window) const {
  if (!active_ || !isInView(window.getSize()))
    return;

  // window.draw(original_shape_);
  window.draw(sprite_);

  if (show_search_results_ && !search_result_.local_rects.empty())
    window.draw(search_boxes_, sf::RenderStates(original_shape_.getTransform()));

  if (selection_boxes_.getVertexCount() != 0)
    window.draw(selection_boxes_, sf::RenderStates(original_shape_.getTransform()));
}

void PageView::update() {
  if (update_search_boxes_) {
    search_boxes_.clear();

    for (std::size_t i = 0; i < search_result_.local_rects.size(); i++) {
      const sf::Color &color = (i == selected_search_result_) ? selected_color_ : unselected_color_;

      const auto &rect = search_result_.local_rects[i];
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

    update_search_boxes_ = false;
  }
}

sf::Sprite &PageView::getSprite() {
  return sprite_;
}

sf::RectangleShape &PageView::getReferenceShape() {
  return original_shape_;
}

sf::Vector2f PageView::inverseTransformPos(const sf::Vector2f &pos) const {
  return original_shape_.getInverseTransform().transformPoint(pos);
}
sf::Vector2f PageView::transformPos(const sf::Vector2f &pos) const {
  return original_shape_.getTransform().transformPoint(pos);
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

void PageView::setTexture(std::shared_ptr<sf::Texture> texture) {
  texture_ = std::move(texture);
  sprite_.setTexture(*texture_, true);
  const auto bounds = sprite_.getLocalBounds();
  sprite_.setOrigin({bounds.size.x * 0.5f, bounds.size.y * 0.5f});
  active_ = true;
}

void PageView::reset() {
  texture_.reset();
  active_ = false;
}

const PDFSearchResult &PageView::getSearchResults() const {
  return search_result_;
}

sf::Vector2f PageView::getSearchResultPosition(std::size_t idx) const {
  const auto &boxes = search_result_.local_rects[idx];
  sf::Vector2f center = {(boxes.x0 + boxes.x1) * 0.5f, (boxes.y0 + boxes.y1) * 0.5f};
  return center;
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

void PageView::showSearchResults() {
  show_search_results_ = true;
}

void PageView::hideSearchResults() {
  show_search_results_ = false;
}

void PageView::setSelections(const std::vector<fz_quad> *quads) {
  selection_boxes_.clear();

  for (const auto &q : *quads) {
    const auto rect = fz_rect_from_quad(q);
    sf::Vector2f tl = {rect.x0, rect.y0};
    sf::Vector2f tr = {rect.x1, rect.y0};
    sf::Vector2f br = {rect.x1, rect.y1};
    sf::Vector2f bl = {rect.x0, rect.y1};

    selection_boxes_.append({tl, selection_color_});
    selection_boxes_.append({tr, selection_color_});
    selection_boxes_.append({br, selection_color_});
    selection_boxes_.append({tl, selection_color_});
    selection_boxes_.append({br, selection_color_});
    selection_boxes_.append({bl, selection_color_});
  }
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
  const sf::Vector2f pos = sprite_.getPosition();
  if (pos == original_shape_.getPosition())
    return;
  original_shape_.setPosition(pos);
}

void PageView::syncPageShape() {
  const sf::Vector2f base = original_shape_.getSize();
  if (base.x <= 0.f || base.y <= 0.f)
    return;

  const int quarter = ((key_.rotate % 4) + 4) % 4;
  const bool swapped = quarter & 1;

  const sf::Vector2f tex = sprite_.getLocalBounds().size;
  const sf::Vector2f s = sprite_.getScale();
  const sf::Vector2f disp = swapped ? sf::Vector2f{tex.y * s.y, tex.x * s.x}
                                    : sf::Vector2f{tex.x * s.x, tex.y * s.y};

  const sf::Vector2f want_pos = sprite_.getPosition();
  const sf::Angle want_rot = sprite_.getRotation() + sf::degrees(quarter * 90.f);
  const sf::Vector2f want_scale = {disp.x / base.x, disp.y / base.y};

  constexpr float eps = 1e-3f;
  auto close = [](sf::Vector2f a, sf::Vector2f b) {
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps;
  };

  if (close(original_shape_.getPosition(), want_pos) &&
      close(original_shape_.getScale(), want_scale) && original_shape_.getRotation() == want_rot)
    return;

  original_shape_.setPosition(want_pos);
  original_shape_.setScale(want_scale);
  original_shape_.setRotation(want_rot);
}

bool PageView::hasTexture() const {
  return active_;
}

bool PageView::isInView(const sf::Vector2u &window_size) const {
  const auto bounds = sprite_.getGlobalBounds();

  float top = bounds.position.y;
  float bottom = bounds.position.y + bounds.size.y;

  return !(top > window_size.y || bottom < 0.f);
}

bool PageView::containsMouse(const sf::Vector2i &mouse_pos) const {
  return sprite_.getGlobalBounds().contains(sf::Vector2f(mouse_pos));
}

} // namespace ui
