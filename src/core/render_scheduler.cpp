#include <iostream>
#include <stdexcept>

#include "core/render_scheduler.h"

namespace core {

RenderScheduler::RenderScheduler(
    pdf::PDFDocument &document, pdf::PDFRenderer &renderer, const Config &config
)
    : document_(document), renderer_(renderer), image_cache_(config.page_cache_size),
      texture_cache_(config.page_cache_size), active_jobs_(0), quiescing_(false) {
  worker_count_ = config.thread_count;
  cache_size_ = config.page_cache_size;

  slots_.reserve(worker_count_);
  for (std::size_t i = 0; i < worker_count_; i++)
    slots_.push_back(WorkerSlot{i, document_.cloneContext()});

  workers_.reserve(worker_count_);
  for (std::size_t i = 0; i < worker_count_; i++)
    workers_.emplace_back([this, i](std::stop_token st) { workerLoop(st, i); });
}

RenderScheduler::~RenderScheduler() {
  for (auto &worker : workers_)
    worker.request_stop();
  workers_.clear();

  std::lock_guard lock(lock_);
  clearCacheLocked();
}

void RenderScheduler::request(const pdf::PDFRenderKey &key) {
  {
    std::lock_guard lock(lock_);
    requestLocked(key);
  }
  cv_work_.notify_one();
}

void RenderScheduler::refreshPages(const std::vector<size_t> &pages) {
  {
    std::lock_guard lock(lock_);

    if (quiescing_)
      return;

    const std::unordered_set<size_t> targets(pages.begin(), pages.end());

    for (auto page : pages)
      renderer_.invalidatePage(page);

    for (auto &slot : slots_)
      if (slot.current && targets.contains(slot.current->page_idx))
        slot.stale = true;

    std::vector<pdf::PDFRenderKey> stale;
    for (const auto &entry : texture_cache_)
      if (targets.contains(entry.key.page_idx))
        stale.push_back(entry.key);
    for (const auto &entry : image_cache_)
      if (targets.contains(entry.key.page_idx))
        stale.push_back(entry.key);

    for (const auto &key : stale) {
      texture_cache_.erase(key);
      image_cache_.erase(key);
      requestLocked(key);
    }
  }

  cv_work_.notify_all();
}

sf::Vector2u RenderScheduler::getPageSize(const pdf::PDFRenderKey &key) const {
  {
    std::lock_guard lock(lock_);

    if (quiescing_)
      throw std::runtime_error("Scheduler is quiescing; no document is currently valid");

    if (key.page_idx >= document_.pageCount())
      throw std::out_of_range("Page index out of range");
  }

  return renderer_.getPageSize(document_.getPage(key.page_idx).getPageBounds(), key);
}

std::shared_ptr<sf::Texture> RenderScheduler::getTexture(const pdf::PDFRenderKey &key) {
  sf::Image image;

  {
    std::lock_guard lock(lock_);

    if (auto *texture = texture_cache_.get(key))
      return *texture;

    auto *cached_image = image_cache_.get(key);
    if (!cached_image)
      return nullptr;

    image = std::move(*cached_image);
    image_cache_.erase(key);
  }

  auto texture = std::make_shared<sf::Texture>();
  if (!texture->loadFromImage(image))
    throw std::runtime_error("Failed to load image into texture");

  std::lock_guard lock(lock_);
  texture_cache_.put(key, texture);
  return texture;
}

void RenderScheduler::clearCache() {
  std::lock_guard lock(lock_);
  if (!quiescing_)
    throw std::runtime_error("Tried to clear cache during active jobs");
  clearCacheLocked();
}

void RenderScheduler::quiesce() {
  std::unique_lock lock(lock_);
  quiescing_ = true;

  job_queue_.clear();
  pending_.clear();

  cv_idle_.wait(lock, [&] { return active_jobs_ == 0; });
}

void RenderScheduler::resume() {
  std::lock_guard lock(lock_);
  quiescing_ = false;
}

void RenderScheduler::requestLocked(const pdf::PDFRenderKey &key) {
  if (quiescing_)
    return;

  if (key.page_idx >= document_.pageCount())
    throw std::out_of_range("Page index out of range");

  if (texture_cache_.contains(key) || image_cache_.contains(key))
    return;

  if (!pending_.insert(key).second)
    return;

  if (job_queue_.size() >= 64) {
    pending_.erase(job_queue_.front());
    job_queue_.pop_front();
  }

  job_queue_.push_back(key);
}

void RenderScheduler::workerLoop(std::stop_token st, std::size_t idx) {
  WorkerSlot &slot = slots_[idx];

  while (true) {
    std::unique_lock lock(lock_);

    if (!cv_work_.wait(lock, st, [&] { return !job_queue_.empty(); }))
      return;

    if (st.stop_requested())
      return;

    pdf::PDFRenderKey key = job_queue_.front();
    job_queue_.pop_front();
    active_jobs_++;

    slot.current = key;
    slot.stale = false;
    fz_context *ctx = slot.ctx.get();
    lock.unlock();

    bool ok = true;
    try {
      renderer_.render(ctx, document_.getDoc(), key, slot.rgba, slot.image);
    } catch (...) {
      ok = false;
    }

    lock.lock();

    const bool stale = slot.stale;
    slot.current.reset();
    slot.stale = false;

    bool requeue = false;
    if (!quiescing_) {
      pending_.erase(key);

      if (ok) {
        if (stale)
          requeue = true;
        else
          image_cache_.put(key, std::move(slot.image));
      }

      if (requeue) {
        try {
          requestLocked(key);
        } catch (...) {
          std::cerr << "Error requesting page in scheduler's worker loop\n";
        }
      }
    }

    active_jobs_--;
    if (active_jobs_ == 0)
      cv_idle_.notify_all();

    if (requeue) {
      lock.unlock();
      cv_work_.notify_one();
    }
  }
}

void RenderScheduler::clearCacheLocked() {
  image_cache_.clear();
  texture_cache_.clear();
  pending_.clear();
  job_queue_.clear();

  renderer_.clearCache();
}

} // namespace core
