#include "utils/profiler.h"

#include <cstdio>

namespace utils {

FrameProfiler::Scope::Scope(FrameProfiler &profiler, const char *name)
    : profiler_(profiler), name_(name), start_(Clock::now()) {}

FrameProfiler::Scope::~Scope() {
  const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
  profiler_.record(name_, ms);
}

FrameProfiler::Scope FrameProfiler::scope(const char *name) {
  return Scope(*this, name);
}

void FrameProfiler::record(const char *name, double ms) {
  acc_[name] += ms;
  if (slow_threshold_ms_ > 0.0 && ms > slow_threshold_ms_)
    std::printf("[slow] %s: %.2f ms\n", name, ms);
}

void FrameProfiler::endFrame(int interval) {
  if (++frames_ < interval)
    return;

  std::printf("---- avg over %d frames (ms) ----\n", frames_);
  for (auto &[name, total] : acc_) {
    std::printf("%-16s %.3f\n", name.c_str(), total / frames_);
    total = 0.0;
  }
  frames_ = 0;
}

void FrameProfiler::setSlowThresholdMs(double ms) {
  slow_threshold_ms_ = ms;
}

} // namespace utils
