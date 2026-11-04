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
  {
    std::lock_guard lock(lock_);
    clearCacheLocked();
  }

  for (auto &worker : workers_)
    worker.request_stop();
  workers_.clear();
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
  std::lock_guard lock(lock_);

  if (quiescing_)
    throw std::runtime_error("Scheduler is quiescing; no document is currently valid");

  if (key.page_idx >= document_.pageCount())
    throw std::out_of_range("Page index out of range");

  return renderer_.getPageSize(document_.getPage(key.page_idx).getPageBounds(), key);
}

bool RenderScheduler::isReady(const pdf::PDFRenderKey &key) const {
  std::lock_guard lock(lock_);
  return texture_cache_.contains(key) || image_cache_.contains(key);
}

sf::Texture *RenderScheduler::getTexture(const pdf::PDFRenderKey &key) {
  std::lock_guard lock(lock_);

  if (auto *texture = texture_cache_.get(key))
    return texture;

  auto *image = image_cache_.get(key);
  if (!image)
    return nullptr;

  sf::Texture texture;
  if (!texture.loadFromImage(*image))
    throw std::runtime_error("Failed to load image into texture");

  texture_cache_.put(key, std::move(texture));
  image_cache_.erase(key);

  return texture_cache_.get(key);
}

void RenderScheduler::clearCache() {
  std::lock_guard lock(lock_);
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

  if (job_queue_.size() >= texture_cache_.capacity()) {
    pending_.erase(job_queue_.front());
    job_queue_.pop_front();
  }

  job_queue_.push_back(key);
}

void RenderScheduler::workerLoop(std::stop_token st, std::size_t idx) {
  while (true) {
    std::unique_lock lock(lock_);

    if (!cv_work_.wait(lock, st, [&] { return !job_queue_.empty(); }))
      return;

    pdf::PDFRenderKey key = job_queue_.front();
    job_queue_.pop_front();
    active_jobs_++;

    fz_context *ctx = slots_[idx].ctx.get();
    lock.unlock();

    bool ok = true;
    try {
      renderer_.render(ctx, document_.getDoc(), key, slots_[idx].rgba, slots_[idx].image);
    } catch (...) {
      ok = false; 
    }

    lock.lock();
    if (ok)
      image_cache_.put(key, std::move(slots_[idx].image));
    pending_.erase(key);

    active_jobs_--;
    if (active_jobs_ == 0)
      cv_idle_.notify_all();
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
