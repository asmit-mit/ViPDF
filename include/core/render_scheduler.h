#pragma once

#include "core/config_reader.h"
#include "pdf/pdf_document.h"
#include "pdf/pdf_renderer.h"
#include "containers/lru_cache.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_set>
#include <vector>

namespace core {

class RenderScheduler {
public:
  RenderScheduler(pdf::PDFDocument &document, pdf::PDFRenderer &renderer, const Config &config);
  ~RenderScheduler();

  void request(const pdf::PDFRenderKey &key);
  void refreshPages(const std::vector<size_t> &pages);

  std::shared_ptr<sf::Texture> getTexture(const pdf::PDFRenderKey &key);
  sf::Vector2u getPageSize(const pdf::PDFRenderKey &key) const;
  void clearCache();

  void quiesce();
  void resume();

private:
  struct WorkerSlot {
    std::size_t index;
    pdf::FzContextPtr ctx;
    std::vector<uint8_t> rgba;
    sf::Image image;
    std::optional<pdf::PDFRenderKey> current;
    bool stale = false;
  };

  void requestLocked(const pdf::PDFRenderKey &key);
  void workerLoop(std::stop_token st, std::size_t idx);
  void clearCacheLocked();

private:
  using TextureCache =
      containers::LRUCache<pdf::PDFRenderKey, std::shared_ptr<sf::Texture>, pdf::PDFRenderKeyHash>;
  using ImageCache = containers::LRUCache<pdf::PDFRenderKey, sf::Image, pdf::PDFRenderKeyHash>;

  pdf::PDFDocument &document_;
  pdf::PDFRenderer &renderer_;

  std::vector<WorkerSlot> slots_;

  ImageCache image_cache_;
  TextureCache texture_cache_;
  std::unordered_set<pdf::PDFRenderKey, pdf::PDFRenderKeyHash> pending_;

  std::deque<pdf::PDFRenderKey> job_queue_;

  mutable std::mutex lock_;
  std::condition_variable_any cv_work_;
  std::condition_variable cv_idle_;

  std::size_t worker_count_;
  std::size_t cache_size_;

  std::size_t active_jobs_;
  bool quiescing_;

  std::vector<std::jthread> workers_;
};

} // namespace core
