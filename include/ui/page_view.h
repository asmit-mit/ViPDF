#pragma once

#include <SFML/Graphics.hpp>

#include "pdf/pdf_renderer.h"
#include "ui/widget.h"

namespace ui {

struct VisualInfo {
  float zoom = 1.f;
  int rotate = 0;
};

struct PDFSearchResult {
  std::vector<fz_rect> local_rects;
  std::size_t start;
};

class PageView : public Widget {
public:
  PageView(const sf::Texture &dummy);

  void draw(sf::RenderTarget &window) const override;
  void update() override;

  sf::Sprite &getSprite();
  sf::RectangleShape &getReferenceShape();

  void move(const sf::Vector2f &delta);
  void setPosition(const sf::Vector2f &position);
  sf::Vector2f getPosition() const;
  sf::FloatRect getGlobalBounds() const;

  void setRotation(int rot);
  void setScale(const sf::Vector2f &size);
  sf::Vector2u getTextureSize() const;

  void setKey(const pdf::PDFRenderKey &key);
  pdf::PDFRenderKey getKey() const;

  void setTexture(const sf::Texture &texture);
  void reset();

  const PDFSearchResult &getSearchResults();
  sf::Vector2f getSearchResultPosition(std::size_t idx);

  void setSelectedSearchResult(std::size_t idx);
  void setSearchResults(PDFSearchResult search_results);
  void clearSearchResults();

  void setSelections(sf::VertexArray selection_boxes);
  void clearSelections();

  void setPageShapeSize(sf::Vector2f size);
  void syncPageShapePos();
  void syncPageShape(int rot);

  bool hasTexture() const;
  bool isInView(const sf::Vector2f &window_size) const;
  bool containsMouse(const sf::Vector2i &mouse_pos) const;

private:
  PDFSearchResult search_result_;
  sf::VertexArray search_boxes_{sf::PrimitiveType::Triangles};
  sf::VertexArray selection_boxes_{sf::PrimitiveType::Triangles};

  sf::RectangleShape original_shape_;
  sf::Sprite sprite_;
  pdf::PDFRenderKey key_;

  std::size_t selected_search_result_;

  bool active_;
  bool update_search_boxes_;

  sf::Color normal_color_;
  sf::Color selected_color_;
};

} // namespace ui
