#pragma once

#include <chrono>
#include <map>
#include <string>

namespace utils {

class FrameProfiler {
public:
  using Clock = std::chrono::steady_clock;

  class Scope {
  public:
    Scope(FrameProfiler &profiler, const char *name);
    ~Scope();

    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

  private:
    FrameProfiler &profiler_;
    const char *name_;
    Clock::time_point start_;
  };

  [[nodiscard]] Scope scope(const char *name);

  void endFrame(int interval = 120);
  void setSlowThresholdMs(double ms);

private:
  void record(const char *name, double ms);

  std::map<std::string, double> acc_;
  int frames_ = 0;
  double slow_threshold_ms_ = 4.0;
};

} // namespace utils
